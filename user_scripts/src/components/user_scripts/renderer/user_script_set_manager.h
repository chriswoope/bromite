#ifndef USERSCRIPTS_RENDER_SET_MANAGER_H_
#define USERSCRIPTS_RENDER_SET_MANAGER_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/observer_list.h"
#include "content/public/renderer/render_thread_observer.h"
#include "../common/host_id.h"
#include "user_script_set.h"
#include "script_injection.h"

namespace user_scripts {

class UserScriptSetManager : public content::RenderThreadObserver {
 public:
  class Observer {
   public:
    virtual void OnUserScriptsUpdated(const std::set<HostID>& changed_hosts) = 0;
  };

  UserScriptSetManager();

  ~UserScriptSetManager() override;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Append all injections from |static_scripts| and each of
  // |programmatic_scripts_| to |injections|.
  void GetAllInjections(
      std::vector<std::unique_ptr<ScriptInjection>>* injections,
      content::RenderFrame* render_frame,
      int tab_id,
      UserScript::RunLocation run_location);

private:
  // content::RenderThreadObserver implementation.
  bool OnControlMessageReceived(const IPC::Message& message) override;

  base::ObserverList<Observer>::Unchecked observers_;

  // Handle the UpdateUserScripts extension message.
  void OnUpdateUserScripts(base::ReadOnlySharedMemoryRegion shared_memory);
                           //, const HostID& host_id,
                           //const std::set<HostID>& changed_hosts,
                           //bool whitelisted_only);

  // Scripts statically defined in extension manifests.
  UserScriptSet static_scripts_;

  // Whether or not dom activity should be logged for scripts injected.
  bool activity_logging_enabled_ = false;
};

}

#endif