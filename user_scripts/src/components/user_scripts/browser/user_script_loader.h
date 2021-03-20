/*
    This file is part of Bromite.

    Bromite is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Bromite is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Bromite. If not, see <https://www.gnu.org/licenses/>.
*/

#ifndef USERSCRIPTS_BROWSER_USER_SCRIPT_LOADER_H_
#define USERSCRIPTS_BROWSER_USER_SCRIPT_LOADER_H_

#include <map>
#include <memory>
#include <set>

#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observer.h"
#include "content/public/browser/render_process_host_creation_observer.h"
#include "ui/shell_dialogs/select_file_dialog.h"
#include "content/browser/file_system_access/file_system_chooser.h"
#include "ui/android/window_android.h"

#include "../common/host_id.h"
#include "../common/user_script.h"
#include "user_script_prefs.h"

namespace base {
class ReadOnlySharedMemoryRegion;
}

namespace content {
class BrowserContext;
class RenderProcessHost;
}

namespace user_scripts {

// Manages one "logical unit" of user scripts in shared memory by constructing a
// new shared memory region when the set of scripts changes. Also notifies
// renderers of new shared memory region when new renderers appear, or when
// script reloading completes. Script loading lives on the file thread.
class UserScriptLoader : public content::RenderProcessHostCreationObserver,
                         public ui::SelectFileDialog::Listener {
 public:
  using LoadScriptsCallback =
      base::OnceCallback<void(std::unique_ptr<UserScriptList>,
                              base::ReadOnlySharedMemoryRegion shared_memory)>;
  using LoadSingleScriptCallback =
      base::OnceCallback<void(bool result, const std::string& error)>;

  using RemoveScriptCallback =
      base::OnceCallback<void()>;
  class Observer {
   public:
    virtual void OnScriptsLoaded(UserScriptLoader* loader,
                                 content::BrowserContext* browser_context) = 0;
    virtual void OnUserScriptLoaderDestroyed(UserScriptLoader* loader) = 0;
    virtual void OnUserScriptLoaded(UserScriptLoader* loader,
                                bool result, const std::string& error) = 0;
  };

  // Parses the includes out of |script| and returns them in |includes|.
  static bool ParseMetadataHeader(const base::StringPiece& script_text,
                                  std::unique_ptr<UserScript>& script,
                                  std::string& error_message);

  UserScriptLoader(content::BrowserContext* browser_context,
                   UserScriptsPrefs* prefs);
                   //const HostID& host_id);
  ~UserScriptLoader() override;

  // Initiates procedure to start loading scripts on the file thread.
  void StartLoad();

  // Returns true if we have any scripts ready.
  bool initial_load_complete() const { return shared_memory_.IsValid(); }

  // Pickle user scripts and return pointer to the shared memory.
  static base::ReadOnlySharedMemoryRegion Serialize(
      const user_scripts::UserScriptList& scripts);

  // Adds or removes observers.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Sets the flag if the initial set of hosts has finished loading; if it's
  // set to be true, calls AttempLoad() to bootstrap.
  void SetReady(bool ready);

  void RemoveScript(const std::string& script_id);
  void SetScriptEnabled(const std::string& script_id, bool is_enabled);

  void SelectAndAddScriptFromFile(ui::WindowAndroid* wa);
  void TryToInstall(const base::FilePath& script_path);

  void LoadScripts(std::unique_ptr<UserScriptList> user_scripts,
                      LoadScriptsCallback callback);

 protected:
  content::BrowserContext* browser_context() const { return browser_context_; }

  UserScriptsPrefs* prefs() const { return prefs_; }

 private:
  void OnRenderProcessHostCreated(
      content::RenderProcessHost* process_host) override;

  // Attempts to initiate a load.
  void AttemptLoad();

  // Called once we have finished loading the scripts on the file thread.
  void OnScriptsLoaded(std::unique_ptr<UserScriptList> user_scripts,
                       base::ReadOnlySharedMemoryRegion shared_memory);

  // Sends the renderer process a new set of user scripts. If
  // |changed_hosts| is not empty, this signals that only the scripts from
  // those hosts should be updated. Otherwise, all hosts will be
  // updated.
  void SendUpdate(content::RenderProcessHost* process,
                  const base::ReadOnlySharedMemoryRegion& shared_memory);

  // Contains the scripts that were found the last time scripts were updated.
  base::ReadOnlySharedMemoryRegion shared_memory_;

  // List of scripts that are currently loaded. This is null when a load is in
  // progress.
  std::unique_ptr<UserScriptList> loaded_scripts_;

  // If the initial set of hosts has finished loading.
  bool ready_;

  // The browser_context for which the scripts managed here are installed.
  content::BrowserContext* browser_context_;

  // Manage load and store from preferences
  UserScriptsPrefs* prefs_;

  // The associated observers.
  base::ObserverList<Observer>::Unchecked observers_;

  void OnScriptRemoved();

  // Manage file dialog requests
  scoped_refptr<ui::SelectFileDialog> dialog_;
  void FileSelected(const base::FilePath& path,
                    int index, void* params) override;
  void FileSelectionCanceled(void* params) override;
  void LoadScriptFromPathOnFileTaskRunnerCallback(
                    bool result, const std::string& error );

  base::WeakPtrFactory<UserScriptLoader> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(UserScriptLoader);
};

}  // namespace extensions

#endif  // USERSCRIPTS_BROWSER_USER_SCRIPT_LOADER_H_
