// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extension_frame_helper.h"

#include <set>

#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/timer/elapsed_timer.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_view.h"
#include "../common/constants.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/web/web_console_message.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_document_loader.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_settings.h"
#include "third_party/blink/public/web/web_view.h"

namespace user_scripts {

namespace {

base::LazyInstance<std::set<const ExtensionFrameHelper*>>::DestructorAtExit
    g_frame_helpers = LAZY_INSTANCE_INITIALIZER;

// Runs every callback in |callbacks_to_be_run_and_cleared| while |frame_helper|
// is valid, and clears |callbacks_to_be_run_and_cleared|.
void RunCallbacksWhileFrameIsValid(
    base::WeakPtr<ExtensionFrameHelper> frame_helper,
    std::vector<base::OnceClosure>* callbacks_to_be_run_and_cleared) {
  // The JavaScript code can cause re-entrancy. To avoid a deadlock, don't run
  // callbacks that are added during the iteration.
  std::vector<base::OnceClosure> callbacks;
  callbacks_to_be_run_and_cleared->swap(callbacks);
  for (auto& callback : callbacks) {
    std::move(callback).Run();
    if (!frame_helper.get())
      return;  // Frame and ExtensionFrameHelper invalidated by callback.
  }
}

}  // namespace

ExtensionFrameHelper::ExtensionFrameHelper(content::RenderFrame* render_frame)
    : content::RenderFrameObserver(render_frame),
      content::RenderFrameObserverTracker<ExtensionFrameHelper>(render_frame),
      tab_id_(-1) {
  g_frame_helpers.Get().insert(this);
}

ExtensionFrameHelper::~ExtensionFrameHelper() {
  g_frame_helpers.Get().erase(this);
}

void ExtensionFrameHelper::ScheduleAtDocumentStart(
    base::OnceClosure callback) {
  document_element_created_callbacks_.push_back(std::move(callback));
}

void ExtensionFrameHelper::ScheduleAtDocumentEnd(
    base::OnceClosure callback) {
  document_load_finished_callbacks_.push_back(std::move(callback));
}

void ExtensionFrameHelper::ScheduleAtDocumentIdle(
    base::OnceClosure callback) {
  document_idle_callbacks_.push_back(std::move(callback));
}

void ExtensionFrameHelper::RunScriptsAtDocumentStart() {
  RunCallbacksWhileFrameIsValid(weak_ptr_factory_.GetWeakPtr(),
                                &document_element_created_callbacks_);
  // |this| might be dead by now.
}

void ExtensionFrameHelper::RunScriptsAtDocumentEnd() {
  RunCallbacksWhileFrameIsValid(weak_ptr_factory_.GetWeakPtr(),
                                &document_load_finished_callbacks_);
  // |this| might be dead by now.
}

void ExtensionFrameHelper::RunScriptsAtDocumentIdle() {
  RunCallbacksWhileFrameIsValid(weak_ptr_factory_.GetWeakPtr(),
                                &document_idle_callbacks_);
  // |this| might be dead by now.
}

void ExtensionFrameHelper::OnDestruct() {
  delete this;
}

}  // namespace user_scripts
