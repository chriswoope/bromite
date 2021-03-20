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

#ifndef USERSCRIPTS_BROWSER_USERSCRIPT_PREFS_H_
#define USERSCRIPTS_BROWSER_USERSCRIPT_PREFS_H_

#include "content/public/browser/browser_context.h"
#include "components/prefs/pref_service.h"
#include "components/pref_registry/pref_registry_syncable.h"

#include "user_script_pref_info.h"
#include "../common/user_script.h"

namespace user_scripts {

class UserScriptsPrefs {
 public:
    UserScriptsPrefs(
        PrefService* prefs);

    static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

    bool IsEnabled();
    void SetEnabled(bool enabled);
   
    void StartupTryout(int number);
    int GetCurrentStartupTryout();

    void CompareWithPrefs(UserScriptList& user_scripts);

    std::string GetScriptsInfo();
    void RemoveScriptFromPrefs(const std::string& script_id);
    void SetScriptEnabled(const std::string& script_id, bool is_enabled);

    std::unique_ptr<UserScriptsListPrefs::ScriptInfo> CreateScriptInfoFromPrefs(
        const std::string& script_id) const;

 private:
    PrefService* prefs_;
};

}

#endif // USERSCRIPTS_BROWSER_USERSCRIPT_PREFS_H_