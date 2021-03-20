#ifndef USERSCRIPTS_RENDER_CLIENT_H_
#define USERSCRIPTS_RENDER_CLIENT_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "user_scripts_dispatcher.h"
#include "services/service_manager/public/cpp/binder_registry.h"

namespace user_scripts {

class UserScriptsRendererClient {
 public:
  UserScriptsRendererClient();
  ~UserScriptsRendererClient();

  static UserScriptsRendererClient* GetInstance();

  void RenderThreadStarted();
  void RenderFrameCreated(content::RenderFrame* render_frame,
    service_manager::BinderRegistry* registry);
  void RunScriptsAtDocumentStart(content::RenderFrame* render_frame);
  void RunScriptsAtDocumentEnd(content::RenderFrame* render_frame);
  void RunScriptsAtDocumentIdle(content::RenderFrame* render_frame);

 private:
  std::unique_ptr<UserScriptsDispatcher> dispatcher_;
};

}

#endif