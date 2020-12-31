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

#include "userscripts_browser_client.h"

#include "base/logging.h"

#include "chrome/browser/browser_process.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"

#include "../common/user_scripts_features.h"
#include "user_script_loader.h"
#include "file_task_runner.h"
#include "user_script_prefs.h"

namespace user_scripts {

namespace {

// remember: was ExtensionsBrowserClient
UserScriptsBrowserClient* g_userscripts_browser_client = NULL;

}  // namespace

UserScriptsBrowserClient::UserScriptsBrowserClient() {}

UserScriptsBrowserClient::~UserScriptsBrowserClient() = default;

// static
UserScriptsBrowserClient* UserScriptsBrowserClient::GetInstance() {
  // only for browser process
  if(!g_browser_process)
    return NULL;

  if (base::FeatureList::IsEnabled(features::kEnableUserScripts) == false)
    return NULL;

  // singleton
  if(g_userscripts_browser_client)
    return g_userscripts_browser_client;

  // make file task runner
  GetUserScriptsFileTaskRunner().get();

  // new instance singleton
  g_userscripts_browser_client = new UserScriptsBrowserClient();

  return g_userscripts_browser_client;
}

void UserScriptsBrowserClient::SetProfile(content::BrowserContext* context) {
  browser_context_ = context;

  prefs_ =
    std::make_unique<user_scripts::UserScriptsPrefs>(
      static_cast<Profile*>(context)->GetPrefs());

  userscript_loader_ =
    std::make_unique<user_scripts::UserScriptLoader>(browser_context_, prefs_.get());
  userscript_loader_->SetReady(true);
}

}  // namespace user_scripts
