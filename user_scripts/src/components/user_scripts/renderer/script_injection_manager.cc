// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "script_injection_manager.h"

#include <memory>
#include <utility>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/feature_list.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "base/logging.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_frame_observer.h"
#include "content/public/renderer/render_thread.h"
#include "extension_frame_helper.h"
#include "../common/host_id.h"
#include "script_injection.h"
#include "scripts_run_info.h"
#include "web_ui_injection_host.h"
#include "ipc/ipc_message_macros.h"
#include "third_party/blink/public/platform/web_url_error.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_frame.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_view.h"
#include "url/gurl.h"
#include "../common/user_scripts_features.h"

namespace user_scripts {

namespace {

// The length of time to wait after the DOM is complete to try and run user
// scripts.
const int kScriptIdleTimeoutInMs = 200;

// Returns the RunLocation that follows |run_location|.
UserScript::RunLocation NextRunLocation(UserScript::RunLocation run_location) {
  switch (run_location) {
    case UserScript::DOCUMENT_START:
      return UserScript::DOCUMENT_END;
    case UserScript::DOCUMENT_END:
      return UserScript::DOCUMENT_IDLE;
    case UserScript::DOCUMENT_IDLE:
      return UserScript::RUN_LOCATION_LAST;
    case UserScript::UNDEFINED:
    case UserScript::RUN_DEFERRED:
    case UserScript::BROWSER_DRIVEN:
    case UserScript::RUN_LOCATION_LAST:
      break;
  }
  NOTREACHED();
  return UserScript::RUN_LOCATION_LAST;
}

}  // namespace

class ScriptInjectionManager::RFOHelper : public content::RenderFrameObserver {
 public:
  RFOHelper(content::RenderFrame* render_frame,
            ScriptInjectionManager* manager);
  ~RFOHelper() override;

  // commit @9f2aac4
  void Initialize();

 private:
  // RenderFrameObserver implementation.
  void DidCreateNewDocument() override;
  void DidCreateDocumentElement() override;
  void DidFailProvisionalLoad() override;
  void DidFinishDocumentLoad() override;
  void WillDetach() override;
  void OnDestruct() override;
  void OnStop() override;

  // Tells the ScriptInjectionManager to run tasks associated with
  // document_idle.
  void RunIdle();

  void StartInjectScripts(UserScript::RunLocation run_location);

  // Indicate that the frame is no longer valid because it is starting
  // a new load or closing.
  void InvalidateAndResetFrame(bool force_reset);

  // The owning ScriptInjectionManager.
  ScriptInjectionManager* manager_;

  bool should_run_idle_ = true; // commit @9f2aac4

  base::WeakPtrFactory<RFOHelper> weak_factory_{this};
};

ScriptInjectionManager::RFOHelper::RFOHelper(content::RenderFrame* render_frame,
                                             ScriptInjectionManager* manager)
    : content::RenderFrameObserver(render_frame),
      manager_(manager),
      should_run_idle_(true) {}

ScriptInjectionManager::RFOHelper::~RFOHelper() {
}


void ScriptInjectionManager::RFOHelper::Initialize() {
  // Set up for the initial empty document, for which the Document created
  // events do not happen as it's already present.
  DidCreateNewDocument();
  // The initial empty document for a main frame may have scripts attached to it
  // but we do not want to invalidate the frame and lose them when the next
  // document loads. For example the IncognitoApiTest.IncognitoSplitMode test
  // does `chrome.tabs.create()` with a script to be run, which is added to the
  // frame before it navigates, so it needs to be preserved. However scripts in
  // child frames are expected to be run inside the initial empty document. For
  // example the ExecuteScriptApiTest.FrameWithHttp204 test creates a child
  // frame at about:blank and expects to run injected scripts inside it.
  // This is all quite inconsistent however tests both depend on us queuing and
  // not queueing the DOCUMENT_START events in the initial empty document.
  if (!render_frame()->IsMainFrame()) {
    DidCreateDocumentElement();
  }
}

void ScriptInjectionManager::RFOHelper::DidCreateNewDocument() {
  // A new document is going to be shown, so invalidate the old document state.
  // Don't force-reset the frame, because it is possible that a script injection
  // was scheduled before the page was loaded, e.g. by navigating to a
  // javascript: URL before the page has loaded.
  constexpr bool kForceReset = false;
  InvalidateAndResetFrame(kForceReset);
}

void ScriptInjectionManager::RFOHelper::DidCreateDocumentElement() {
  if (base::FeatureList::IsEnabled(features::kEnableLoggingUserScripts))
    LOG(INFO) << "UserScripts: DidCreateDocumentElement -> DOCUMENT_START";

  ExtensionFrameHelper::Get(render_frame())
      ->ScheduleAtDocumentStart(
          base::BindOnce(&ScriptInjectionManager::RFOHelper::StartInjectScripts,
                     weak_factory_.GetWeakPtr(), UserScript::DOCUMENT_START));
}

void ScriptInjectionManager::RFOHelper::DidFailProvisionalLoad() {
  auto it = manager_->frame_statuses_.find(render_frame());
  if (it != manager_->frame_statuses_.end() &&
      it->second == UserScript::DOCUMENT_START) {
    // Since the provisional load failed, the frame stays at its previous loaded
    // state and origin (or the parent's origin for new/about:blank frames).
    // Reset the frame to DOCUMENT_IDLE in order to reflect that the frame is
    // done loading, and avoid any deadlock in the system.
    //
    // We skip injection of DOCUMENT_END and DOCUMENT_IDLE scripts, because the
    // injections closely follow the DOMContentLoaded (and onload) events, which
    // are not triggered after a failed provisional load.
    // This assumption is verified in the checkDOMContentLoadedEvent subtest of
    // ExecuteScriptApiTest.FrameWithHttp204 (browser_tests).
    constexpr bool kForceReset = true;
    InvalidateAndResetFrame(kForceReset);
    should_run_idle_ = false;
    manager_->frame_statuses_[render_frame()] = UserScript::DOCUMENT_IDLE;
  }
}

void ScriptInjectionManager::RFOHelper::DidFinishDocumentLoad() {
  if (base::FeatureList::IsEnabled(features::kEnableLoggingUserScripts))
    LOG(INFO) << "UserScripts: DidFinishDocumentLoad -> DOCUMENT_END";

  DCHECK(content::RenderThread::Get());
  ExtensionFrameHelper::Get(render_frame())
      ->ScheduleAtDocumentEnd(
          base::BindOnce(&ScriptInjectionManager::RFOHelper::StartInjectScripts,
                     weak_factory_.GetWeakPtr(), UserScript::DOCUMENT_END));

  // We try to run idle in two places: a delayed task here and in response to
  // ContentRendererClient::RunScriptsAtDocumentIdle(). DidFinishDocumentLoad()
  // corresponds to completing the document's load, whereas
  // RunScriptsAtDocumentIdle() corresponds to completing the document and all
  // subresources' load (but before the window.onload event). We don't want to
  // hold up script injection for a particularly slow subresource, so we set a
  // delayed task from here - but if we finish everything before that point
  // (i.e., RunScriptsAtDocumentIdle() is triggered), then there's no reason to
  // keep waiting.
  render_frame()
      ->GetTaskRunner(blink::TaskType::kInternalDefault)
      ->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&ScriptInjectionManager::RFOHelper::RunIdle,
                         weak_factory_.GetWeakPtr()),
          base::TimeDelta::FromMilliseconds(kScriptIdleTimeoutInMs));

  ExtensionFrameHelper::Get(render_frame())
      ->ScheduleAtDocumentIdle(
          base::BindOnce(&ScriptInjectionManager::RFOHelper::RunIdle,
                     weak_factory_.GetWeakPtr()));
}

void ScriptInjectionManager::RFOHelper::WillDetach() {
  // The frame is closing - invalidate.
  constexpr bool kForceReset = true;
  InvalidateAndResetFrame(kForceReset);
}

void ScriptInjectionManager::RFOHelper::OnDestruct() {
  manager_->RemoveObserver(this);
}

void ScriptInjectionManager::RFOHelper::OnStop() {
  // If the navigation request fails (e.g. 204/205/downloads), notify the
  // extension to avoid keeping the frame in a START state indefinitely which
  // leads to deadlocks.
  DidFailProvisionalLoad();
}

void ScriptInjectionManager::RFOHelper::RunIdle() {
  // Only notify the manager if the frame hasn't already had idle run since the
  // task to RunIdle() was posted.
  if (should_run_idle_) {
    should_run_idle_ = false;
    if (base::FeatureList::IsEnabled(features::kEnableLoggingUserScripts))
      LOG(INFO) << "UserScripts: RunIdle -> DOCUMENT_IDLE";
    manager_->StartInjectScripts(render_frame(), UserScript::DOCUMENT_IDLE);
  }
}

void ScriptInjectionManager::RFOHelper::StartInjectScripts(
    UserScript::RunLocation run_location) {
  manager_->StartInjectScripts(render_frame(), run_location);
}

void ScriptInjectionManager::RFOHelper::InvalidateAndResetFrame(
    bool force_reset) {
  // Invalidate any pending idle injections, and reset the frame inject on idle.
  weak_factory_.InvalidateWeakPtrs();
  // We reset to inject on idle, because the frame can be reused (in the case of
  // navigation).
  should_run_idle_ = true;

  // Reset the frame if either |force_reset| is true, or if the manager is
  // keeping track of the state of the frame (in which case we need to clean it
  // up).
  if (force_reset || manager_->frame_statuses_.count(render_frame()) != 0)
    manager_->InvalidateForFrame(render_frame());
}

ScriptInjectionManager::ScriptInjectionManager(
    UserScriptSetManager* user_script_set_manager)
    : user_script_set_manager_(user_script_set_manager),
      user_script_set_manager_observer_(this) {
  user_script_set_manager_observer_.Add(user_script_set_manager_);
}

ScriptInjectionManager::~ScriptInjectionManager() {
  for (const auto& injection : pending_injections_)
    injection->invalidate_render_frame();
  for (const auto& injection : running_injections_)
    injection->invalidate_render_frame();
}

void ScriptInjectionManager::OnRenderFrameCreated(
    content::RenderFrame* render_frame) {
  rfo_helpers_.push_back(std::make_unique<RFOHelper>(render_frame, this));
  rfo_helpers_.back()->Initialize(); // commit @9f2aac4
}

void ScriptInjectionManager::OnInjectionFinished(
    ScriptInjection* injection) {
  auto iter =
      std::find_if(running_injections_.begin(), running_injections_.end(),
                   [injection](const std::unique_ptr<ScriptInjection>& mode) {
                     return injection == mode.get();
                   });
  if (iter != running_injections_.end())
    running_injections_.erase(iter);
}

void ScriptInjectionManager::OnUserScriptsUpdated(
    const std::set<HostID>& changed_hosts) {
  for (auto iter = pending_injections_.begin();
       iter != pending_injections_.end();) {
    if (changed_hosts.count((*iter)->host_id()) > 0)
      iter = pending_injections_.erase(iter);
    else
      ++iter;
  }
}

void ScriptInjectionManager::RemoveObserver(RFOHelper* helper) {
  for (auto iter = rfo_helpers_.begin(); iter != rfo_helpers_.end(); ++iter) {
    if (iter->get() == helper) {
      rfo_helpers_.erase(iter);
      break;
    }
  }
}

void ScriptInjectionManager::InvalidateForFrame(content::RenderFrame* frame) {
  // If the frame invalidated is the frame being injected into, we need to
  // note it.
  active_injection_frames_.erase(frame);

  for (auto iter = pending_injections_.begin();
       iter != pending_injections_.end();) {
    if ((*iter)->render_frame() == frame)
      iter = pending_injections_.erase(iter);
    else
      ++iter;
  }

  frame_statuses_.erase(frame);
}

void ScriptInjectionManager::StartInjectScripts(
    content::RenderFrame* frame,
    UserScript::RunLocation run_location) {
  auto iter = frame_statuses_.find(frame);
  // We also don't execute if we detect that the run location is somehow out of
  // order. This can happen if:
  // - The first run location reported for the frame isn't DOCUMENT_START, or
  // - The run location reported doesn't immediately follow the previous
  //   reported run location.
  // We don't want to run because extensions may have requirements that scripts
  // running in an earlier run location have run by the time a later script
  // runs. Better to just not run.
  // Note that we check run_location > NextRunLocation() in the second clause
  // (as opposed to !=) because earlier signals (like DidCreateDocumentElement)
  // can happen multiple times, so we can receive earlier/equal run locations.
  if ((iter == frame_statuses_.end() &&
           run_location != UserScript::DOCUMENT_START) ||
      (iter != frame_statuses_.end() &&
           run_location > NextRunLocation(iter->second))) {
    // We also invalidate the frame, because the run order of pending injections
    // may also be bad.
    InvalidateForFrame(frame);
    return;
  } else if (iter != frame_statuses_.end() && iter->second >= run_location) {
    // Certain run location signals (like DidCreateDocumentElement) can happen
    // multiple times. Ignore the subsequent signals.
    return;
  }

  // Otherwise, all is right in the world, and we can get on with the
  // injections!
  frame_statuses_[frame] = run_location;
  InjectScripts(frame, run_location);
}

void ScriptInjectionManager::InjectScripts(
    content::RenderFrame* frame,
    UserScript::RunLocation run_location) {
  // Find any injections that want to run on the given frame.
  ScriptInjectionVector frame_injections;
  for (auto iter = pending_injections_.begin();
       iter != pending_injections_.end();) {
    if ((*iter)->render_frame() == frame) {
      frame_injections.push_back(std::move(*iter));
      iter = pending_injections_.erase(iter);
    } else {
      ++iter;
    }
  }

  // Add any injections for user scripts.
  int tab_id = ExtensionFrameHelper::Get(frame)->tab_id();
  user_script_set_manager_->GetAllInjections(&frame_injections, frame, tab_id,
                                             run_location);

  // Note that we are running in |frame|.
  active_injection_frames_.insert(frame);

  ScriptsRunInfo scripts_run_info(frame, run_location);

  for (auto iter = frame_injections.begin(); iter != frame_injections.end();) {
    // It's possible for thScriptsRunInfoe frame to be invalidated in the course of injection
    // (if a script removes its own frame, for example). If this happens, abort.
    if (!active_injection_frames_.count(frame))
      break;
    std::unique_ptr<ScriptInjection> injection(std::move(*iter));
    iter = frame_injections.erase(iter);
    TryToInject(std::move(injection), run_location, &scripts_run_info);
  }

  // We are done running in the frame.
  active_injection_frames_.erase(frame);

  scripts_run_info.LogRun(activity_logging_enabled_);
}

void ScriptInjectionManager::TryToInject(
    std::unique_ptr<ScriptInjection> injection,
    UserScript::RunLocation run_location,
    ScriptsRunInfo* scripts_run_info) {
  // Try to inject the script. If the injection is waiting (i.e., for
  // permission), add it to the list of pending injections. If the injection
  // has blocked, add it to the list of running injections.
  // The Unretained below is safe because this object owns all the
  // ScriptInjections, so is guaranteed to outlive them.
  switch (injection->TryToInject(
      run_location, scripts_run_info,
      base::Bind(&ScriptInjectionManager::OnInjectionFinished,
                 base::Unretained(this)))) {
    case ScriptInjection::INJECTION_WAITING:
      pending_injections_.push_back(std::move(injection));
      break;
    case ScriptInjection::INJECTION_BLOCKED:
      running_injections_.push_back(std::move(injection));
      break;
    case ScriptInjection::INJECTION_FINISHED:
      break;
  }
}

}  // namespace extensions
