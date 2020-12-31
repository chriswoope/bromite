#ifndef COMPONENTS_USER_SCRIPTS_RENDER_DISPATCHER_H_
#define COMPONENTS_USER_SCRIPTS_RENDER_DISPATCHER_H_

#include "user_script_set_manager.h"
#include "script_injection_manager.h"

#include <stdint.h>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/scoped_observer.h"
#include "content/public/renderer/render_thread_observer.h"
#include "content/public/renderer/render_thread.h"
#include "../common/host_id.h"
#include "user_script_set_manager.h"
#include "script_injection.h"

namespace user_scripts {

class UserScriptsDispatcher : public content::RenderThreadObserver,
                              public UserScriptSetManager::Observer {

 public:
  explicit UserScriptsDispatcher();
  ~UserScriptsDispatcher() override;

  void OnRenderThreadStarted(content::RenderThread* thread);
  void OnUserScriptsUpdated(const std::set<HostID>& changed_hosts) override;
  void OnRenderFrameCreated(content::RenderFrame* render_frame);

 private:
  std::unique_ptr<UserScriptSetManager> user_script_set_manager_;

  std::unique_ptr<ScriptInjectionManager> script_injection_manager_;

  ScopedObserver<UserScriptSetManager, UserScriptSetManager::Observer>
      user_script_set_manager_observer_;
};

}

#endif