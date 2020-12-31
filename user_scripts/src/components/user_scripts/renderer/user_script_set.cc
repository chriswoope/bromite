// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "user_script_set.h"

#include <stddef.h>

#include <utility>

#include "base/logging.h"
#include "base/debug/alias.h"
#include "base/memory/ref_counted.h"
#include "base/strings/strcat.h"
#include "content/public/common/url_constants.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "injection_host.h"
#include "script_context.h"
#include "script_injection.h"
#include "user_script_injector.h"
#include "web_ui_injection_host.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "url/gurl.h"
#include "../common/user_scripts_features.h"

namespace user_scripts {

namespace {

// These two strings are injected before and after the Greasemonkey API and
// user script to wrap it in an anonymous scope.
const char kUserScriptHead[] = "(function (unsafeWindow) {\n";
const char kUserScriptTail[] = "\n})(window);";
// Maximum number of total content scripts we allow (across all extensions).
// The limit exists to diagnose https://crbug.com/723381. The number is
// arbitrarily chosen.
// TODO(lazyboy): Remove when the bug is fixed.
const uint32_t kNumScriptsArbitraryMax = 100000u;

GURL GetDocumentUrlForFrame(blink::WebLocalFrame* frame) {
  GURL data_source_url = ScriptContext::GetDocumentLoaderURLForFrame(frame);
  if (!data_source_url.is_empty() && frame->IsViewSourceModeEnabled()) {
    data_source_url = GURL(content::kViewSourceScheme + std::string(":") +
                           data_source_url.spec());
  }

  return data_source_url;
}

}  // namespace

UserScriptSet::UserScriptSet() {}

UserScriptSet::~UserScriptSet() {
}

void UserScriptSet::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void UserScriptSet::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void UserScriptSet::GetInjections(
    std::vector<std::unique_ptr<ScriptInjection>>* injections,
    content::RenderFrame* render_frame,
    int tab_id,
    UserScript::RunLocation run_location,
    bool log_activity) {
  GURL document_url = GetDocumentUrlForFrame(render_frame->GetWebFrame());
  for (const std::unique_ptr<UserScript>& script : scripts_) {
    std::unique_ptr<ScriptInjection> injection = GetInjectionForScript(
        script.get(), render_frame, tab_id, run_location, document_url,
        /* is_declarative, */ log_activity);
    if (injection.get())
      injections->push_back(std::move(injection));
  }
}

bool UserScriptSet::UpdateUserScripts(
    base::ReadOnlySharedMemoryRegion shared_memory,
    const std::set<HostID>& changed_hosts,
    bool whitelisted_only) {
  bool only_inject_incognito = false;
  //ExtensionsRendererClient::Get()->IsIncognitoProcess();

  // Create the shared memory mapping.
  shared_memory_mapping_ = shared_memory.Map();
  if (!shared_memory.IsValid())
    return false;

  // First get the size of the memory block.
  const base::Pickle::Header* pickle_header =
      shared_memory_mapping_.GetMemoryAs<base::Pickle::Header>();
  if (!pickle_header)
    return false;

  // Now read in the rest of the block.
  size_t pickle_size =
      sizeof(base::Pickle::Header) + pickle_header->payload_size;

  // Unpickle scripts.
  uint32_t num_scripts = 0;
  auto memory = shared_memory_mapping_.GetMemoryAsSpan<char>(pickle_size);
  if (!memory.size())
    return false;

  base::Pickle pickle(memory.data(), pickle_size);
  base::PickleIterator iter(pickle);
  base::debug::Alias(&pickle_size);
  CHECK(iter.ReadUInt32(&num_scripts));

  // Sometimes the shared memory contents seem to be corrupted
  // (https://crbug.com/723381). Set an arbitrary max limit to the number of
  // scripts so that we don't add OOM noise to crash reports.
  CHECK_LT(num_scripts, kNumScriptsArbitraryMax);

  scripts_.clear();
  script_sources_.clear();
  scripts_.reserve(num_scripts);
  for (uint32_t i = 0; i < num_scripts; ++i) {
    std::unique_ptr<UserScript> script(new UserScript());
    script->Unpickle(pickle, &iter);

    // Note that this is a pointer into shared memory. We don't own it. It gets
    // cleared up when the last renderer or browser process drops their
    // reference to the shared memory.
    for (size_t j = 0; j < script->js_scripts().size(); ++j) {
      const char* body = NULL;
      int body_length = 0;
      CHECK(iter.ReadData(&body, &body_length));
      script->js_scripts()[j]->set_external_content(
          base::StringPiece(body, body_length));
    }
    for (size_t j = 0; j < script->css_scripts().size(); ++j) {
      const char* body = NULL;
      int body_length = 0;
      CHECK(iter.ReadData(&body, &body_length));
      script->css_scripts()[j]->set_external_content(
          base::StringPiece(body, body_length));
    }

    if (only_inject_incognito && !script->is_incognito_enabled())
      continue;  // This script shouldn't run in an incognito tab.

    scripts_.push_back(std::move(script));
  }

  for (auto& observer : observers_)
    observer.OnUserScriptsUpdated(changed_hosts, scripts_);
  return true;
}

void UserScriptSet::AddScript(std::unique_ptr<UserScript> script) {
  scripts_.push_back(std::move(script));
}

std::unique_ptr<ScriptInjection> UserScriptSet::GetInjectionForScript(
    const UserScript* script,
    content::RenderFrame* render_frame,
    int tab_id,
    UserScript::RunLocation run_location,
    const GURL& document_url,
    //bool is_declarative,
    bool log_activity) {
  std::unique_ptr<ScriptInjection> injection;
  std::unique_ptr<const InjectionHost> injection_host;
  blink::WebLocalFrame* web_frame = render_frame->GetWebFrame();

  const HostID& host_id = script->host_id();
  injection_host.reset(new WebUIInjectionHost(host_id));

  GURL effective_document_url =
      ScriptContext::GetEffectiveDocumentURLForInjection(
          web_frame, document_url, script->match_origin_as_fallback());

  bool is_subframe = web_frame->Parent();
  if (!script->MatchesDocument(effective_document_url, is_subframe)) {
    if (base::FeatureList::IsEnabled(features::kEnableLoggingUserScripts))
      LOG(INFO) << "UserScripts: No Match name=" << script->name() << " " <<
                                         "url=" << effective_document_url.spec();
    return injection;
  }

  std::unique_ptr<ScriptInjector> injector(
      new UserScriptInjector(script, this));

  bool inject_css = !script->css_scripts().empty() &&
                    run_location == UserScript::DOCUMENT_START;
  bool inject_js =
      !script->js_scripts().empty() && script->run_location() == run_location;
  if (inject_css || inject_js) {
    injection.reset(new ScriptInjection(std::move(injector), render_frame,
                                        std::move(injection_host), run_location,
                                        log_activity));
    if (base::FeatureList::IsEnabled(features::kEnableLoggingUserScripts))
      LOG(INFO) << "UserScripts: Match name=" << script->name() << " " <<
                                         "url=" << effective_document_url.spec();
  } else {
    if (base::FeatureList::IsEnabled(features::kEnableLoggingUserScripts)) {
      if (script->run_location() != run_location) 
        LOG(INFO) << "UserScripts: wrong run location current " << run_location << " " <<
                                                     "expeted " << script->run_location();
      else
        LOG(INFO) << "UserScripts: Match but no script name=" << script->name() << " " <<
                                          "url=" << effective_document_url.spec();
    }
  }
  return injection;
}

blink::WebString UserScriptSet::GetJsSource(const UserScript::File& file,
                                            bool emulate_greasemonkey) {
  const GURL& url = file.url();
  auto iter = script_sources_.find(url);
  if (iter != script_sources_.end()) {
    // LOG(INFO) << "---UserScriptSet::GetJsSource found " << url;
    return iter->second;
  }

  base::StringPiece script_content = file.GetContent();
  blink::WebString source;
  if (emulate_greasemonkey) {
    // We add this dumb function wrapper for user scripts to emulate what
    // Greasemonkey does. |script_content| becomes:
    // concat(kUserScriptHead, script_content, kUserScriptTail).
    std::string content =
        base::StrCat({kUserScriptHead, script_content, kUserScriptTail});
    source = blink::WebString::FromUTF8(content);
    if (base::FeatureList::IsEnabled(features::kEnableLoggingUserScripts))
      LOG(INFO) << "UserScripts: Injecting w/greasemonkey " << file.url();
    // LOG(INFO) << "---UserScriptSet::GetJsSource emu " << script_content;
  } else {
    source = blink::WebString::FromUTF8(script_content.data(),
                                        script_content.length());
    if (base::FeatureList::IsEnabled(features::kEnableLoggingUserScripts))
      LOG(INFO) << "UserScripts: Injecting " << file.url();
  }
  script_sources_[url] = source;
  return source;
}

blink::WebString UserScriptSet::GetCssSource(const UserScript::File& file) {
  const GURL& url = file.url();
  auto iter = script_sources_.find(url);
  if (iter != script_sources_.end())
    return iter->second;

  base::StringPiece script_content = file.GetContent();
  return script_sources_
      .insert(std::make_pair(
          url, blink::WebString::FromUTF8(script_content.data(),
                                          script_content.length())))
      .first->second;
}

}  // namespace extensions
