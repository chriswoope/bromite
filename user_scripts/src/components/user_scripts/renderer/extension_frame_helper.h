// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_EXTENSION_FRAME_HELPER_H_
#define EXTENSIONS_RENDERER_EXTENSION_FRAME_HELPER_H_

#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/public/renderer/render_frame_observer.h"
#include "content/public/renderer/render_frame_observer_tracker.h"
#include "../common/view_type.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom.h"
#include "v8/include/v8.h"

struct ExtensionMsg_ExternalConnectionInfo;
struct ExtensionMsg_TabConnectionInfo;

namespace base {
class ListValue;
}

namespace user_scripts {

class Dispatcher;
struct Message;
struct PortId;
class ScriptContext;

// RenderFrame-level plumbing for extension features.
class ExtensionFrameHelper
    : public content::RenderFrameObserver,
      public content::RenderFrameObserverTracker<ExtensionFrameHelper> {
 public:
  ExtensionFrameHelper(content::RenderFrame* render_frame /*,
                       Dispatcher* extension_dispatcher*/);
  ~ExtensionFrameHelper() override;

  int tab_id() const { return tab_id_; }

  // Called when the document element has been inserted in this frame. This
  // method may invoke untrusted JavaScript code that invalidate the frame and
  // this ExtensionFrameHelper.
  void RunScriptsAtDocumentStart();

  // Called after the DOMContentLoaded event has fired.
  void RunScriptsAtDocumentEnd();

  // Called before the window.onload event is fired.
  void RunScriptsAtDocumentIdle();

  // Schedule a callback, to be run at the next RunScriptsAtDocumentStart
  // notification. Only call this when you are certain that there will be such a
  // notification, e.g. from RenderFrameObserver::DidCreateDocumentElement.
  // Otherwise the callback is never invoked, or invoked for a document that you
  // were not expecting.
  void ScheduleAtDocumentStart(const base::Closure& callback);

  // Schedule a callback, to be run at the next RunScriptsAtDocumentEnd call.
  void ScheduleAtDocumentEnd(const base::Closure& callback);

  // Schedule a callback, to be run at the next RunScriptsAtDocumentIdle call.
  void ScheduleAtDocumentIdle(const base::Closure& callback);

 private:

  void OnDestruct() override;

  // The id of the tab the render frame is attached to.
  int tab_id_;

  // Callbacks to be run at the next RunScriptsAtDocumentStart notification.
  std::vector<base::Closure> document_element_created_callbacks_;

  // Callbacks to be run at the next RunScriptsAtDocumentEnd notification.
  std::vector<base::Closure> document_load_finished_callbacks_;

  // Callbacks to be run at the next RunScriptsAtDocumentIdle notification.
  std::vector<base::Closure> document_idle_callbacks_;

  base::WeakPtrFactory<ExtensionFrameHelper> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ExtensionFrameHelper);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_EXTENSION_FRAME_HELPER_H_
