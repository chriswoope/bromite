// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "scripts_run_info.h"

#include "base/metrics/histogram_macros.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "script_context.h"
#include "third_party/blink/public/web/web_local_frame.h"

namespace user_scripts {

ScriptsRunInfo::ScriptsRunInfo(content::RenderFrame* render_frame,
                               UserScript::RunLocation location)
    : num_css(0u),
      num_js(0u),
      num_blocking_js(0u),
      routing_id_(render_frame->GetRoutingID()),
      run_location_(location),
      frame_url_(ScriptContext::GetDocumentLoaderURLForFrame(
          render_frame->GetWebFrame())) {}

ScriptsRunInfo::~ScriptsRunInfo() {
}

void ScriptsRunInfo::LogRun(bool send_script_activity) {
}

}  // namespace extensions
