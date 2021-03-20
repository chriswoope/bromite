// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef USERSCRIPTS_RENDERER_SCRIPT_CONTEXT_H_
#define USERSCRIPTS_RENDERER_SCRIPT_CONTEXT_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "base/unguessable_token.h"
#include "../common/script_constants.h"
#include "script_injection_callback.h"
#include "url/gurl.h"
#include "v8/include/v8.h"

namespace blink {
class WebDocumentLoader;
class WebLocalFrame;
}

namespace content {
class RenderFrame;
}

namespace user_scripts {

// Extensions wrapper for a v8::Context.
//
// v8::Contexts can be constructed on any thread, and must only be accessed or
// destroyed that thread.
//
// Note that ScriptContexts bound to worker threads will not have the full
// functionality as those bound to the main RenderThread.
class ScriptContext {
 public:
  // TODO(devlin): Move all these Get*URL*() methods out of here? While they are
  // vaguely ScriptContext related, there's enough here that they probably
  // warrant another class or utility file.

  // Utility to get the URL we will match against for a frame. If the frame has
  // committed, this is the commited URL. Otherwise it is the provisional URL.
  // The returned URL may be invalid.
  static GURL GetDocumentLoaderURLForFrame(const blink::WebLocalFrame* frame);

  // Used to determine the "effective" URL for extension script injection.
  // If |document_url| is an about: or data: URL, returns the URL of the first
  // frame without an about: or data: URL that matches the initiator origin.
  // This may not be the immediate parent. Returns |document_url| if it is not
  // an about: or data: URL, if |match_origin_as_fallback| is set to not match,
  // or if a suitable parent cannot be found.
  // Considers parent contexts that cannot be accessed (as is the case for
  // sandboxed frames).
  static GURL GetEffectiveDocumentURLForInjection(
      blink::WebLocalFrame* frame,
      const GURL& document_url,
      MatchOriginAsFallbackBehavior match_origin_as_fallback);

//   DISALLOW_COPY_AND_ASSIGN(ScriptContext);
};

}  // namespace extensions

#endif  // USERSCRIPTS_RENDERER_SCRIPT_CONTEXT_H_
