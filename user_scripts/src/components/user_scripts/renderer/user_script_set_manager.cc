#include "user_script_set_manager.h"

#include "base/logging.h"
#include "content/public/renderer/render_thread.h"
#include "../common/host_id.h"
#include "../common/extension_messages.h"
#include "../common/user_scripts_features.h"
#include "user_script_set.h"

namespace user_scripts {

UserScriptSetManager::UserScriptSetManager() {
  content::RenderThread::Get()->AddObserver(this);
}

UserScriptSetManager::~UserScriptSetManager() {
}

void UserScriptSetManager::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void UserScriptSetManager::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool UserScriptSetManager::OnControlMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(UserScriptSetManager, message)
    IPC_MESSAGE_HANDLER(ExtensionMsg_UpdateUserScripts, OnUpdateUserScripts)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void UserScriptSetManager::GetAllInjections(
    std::vector<std::unique_ptr<ScriptInjection>>* injections,
    content::RenderFrame* render_frame,
    int tab_id,
    UserScript::RunLocation run_location) {

  if (base::FeatureList::IsEnabled(features::kEnableLoggingUserScripts))
    LOG(INFO) << "UserScripts: GetAllInjections";

  // static_scripts_ is UserScriptSet
  static_scripts_.GetInjections(injections, render_frame, tab_id, run_location,
                                activity_logging_enabled_);
}

void UserScriptSetManager::OnUpdateUserScripts(
    base::ReadOnlySharedMemoryRegion shared_memory) {
  if (!shared_memory.IsValid()) {
    NOTREACHED() << "Bad scripts handle";
    return;
  }

  UserScriptSet* scripts = NULL;
  scripts = &static_scripts_;

  DCHECK(scripts);

  // If no hosts are included in the set, that indicates that all
  // hosts were updated. Add them all to the set so that observers and
  // individual UserScriptSets don't need to know this detail.
  //const std::set<HostID>* effective_hosts = &changed_hosts;
  std::set<HostID> all_hosts;
  const std::set<HostID>* effective_hosts = &all_hosts;

  if (scripts->UpdateUserScripts(std::move(shared_memory), *effective_hosts,
                                 false /*whitelisted_only*/)) {
    for (auto& observer : observers_)
      observer.OnUserScriptsUpdated(all_hosts /* *effective_hosts*/);
  }
}

}