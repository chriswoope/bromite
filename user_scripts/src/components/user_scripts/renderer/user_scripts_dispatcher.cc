#include "user_scripts_dispatcher.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <utility>

#include "content/public/renderer/render_thread.h"
#include "extension_frame_helper.h"

namespace user_scripts {

// ex ChromeExtensionsDispatcherDelegate
UserScriptsDispatcher::UserScriptsDispatcher()
    : user_script_set_manager_observer_(this) {
      user_script_set_manager_.reset(new UserScriptSetManager());
      script_injection_manager_.reset(
          new ScriptInjectionManager(user_script_set_manager_.get()));
      user_script_set_manager_observer_.Add(user_script_set_manager_.get());
}

UserScriptsDispatcher::~UserScriptsDispatcher() {
}

void UserScriptsDispatcher::OnRenderThreadStarted(content::RenderThread* thread) {
}

void UserScriptsDispatcher::OnUserScriptsUpdated(const std::set<HostID>& changed_hosts) {
}

void UserScriptsDispatcher::OnRenderFrameCreated(content::RenderFrame* render_frame) {
  script_injection_manager_->OnRenderFrameCreated(render_frame);
}

}