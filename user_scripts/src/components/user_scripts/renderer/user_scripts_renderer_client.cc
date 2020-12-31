#include "user_scripts_renderer_client.h"

#include <memory>
#include <utility>

#include "base/logging.h"
#include "base/lazy_instance.h"
#include "content/public/renderer/render_thread.h"

#include "../common/user_scripts_features.h"
#include "user_scripts_dispatcher.h"
#include "extension_frame_helper.h"

namespace user_scripts {

// was ChromeExtensionsRendererClient
UserScriptsRendererClient::UserScriptsRendererClient() {}

UserScriptsRendererClient::~UserScriptsRendererClient() {}

// static
UserScriptsRendererClient* UserScriptsRendererClient::GetInstance() {
  if (base::FeatureList::IsEnabled(features::kEnableUserScripts) == false)
    return NULL;

  static base::LazyInstance<UserScriptsRendererClient>::Leaky client =
      LAZY_INSTANCE_INITIALIZER;
  return client.Pointer();
}

void UserScriptsRendererClient::RenderThreadStarted() {
  if (base::FeatureList::IsEnabled(features::kEnableLoggingUserScripts))
    LOG(INFO) << "UserScripts: RenderThreadStarted";
  content::RenderThread* thread = content::RenderThread::Get();

  dispatcher_ = std::make_unique<UserScriptsDispatcher>();

  dispatcher_->OnRenderThreadStarted(thread);
  thread->AddObserver(dispatcher_.get());
}

void UserScriptsRendererClient::RenderFrameCreated(
    content::RenderFrame* render_frame,
    service_manager::BinderRegistry* registry) {
  new user_scripts::ExtensionFrameHelper(render_frame);
  dispatcher_->OnRenderFrameCreated(render_frame);
}

void UserScriptsRendererClient::RunScriptsAtDocumentStart(content::RenderFrame* render_frame) {
  ExtensionFrameHelper* frame_helper = ExtensionFrameHelper::Get(render_frame);
  if (!frame_helper)
    return;  // The frame is invisible to extensions.

  frame_helper->RunScriptsAtDocumentStart();
  // |frame_helper| and |render_frame| might be dead by now.
}

void UserScriptsRendererClient::RunScriptsAtDocumentEnd(content::RenderFrame* render_frame) {
  ExtensionFrameHelper* frame_helper = ExtensionFrameHelper::Get(render_frame);
  if (!frame_helper)
    return;  // The frame is invisible to extensions.

  frame_helper->RunScriptsAtDocumentEnd();
  // |frame_helper| and |render_frame| might be dead by now.
}

void UserScriptsRendererClient::RunScriptsAtDocumentIdle(content::RenderFrame* render_frame) {
  ExtensionFrameHelper* frame_helper = ExtensionFrameHelper::Get(render_frame);
  if (!frame_helper)
    return;  // The frame is invisible to extensions.

  frame_helper->RunScriptsAtDocumentIdle();
  // |frame_helper| and |render_frame| might be dead by now.
}

}