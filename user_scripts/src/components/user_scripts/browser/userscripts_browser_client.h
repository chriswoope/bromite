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

#ifndef USERSCRIPTS_BROWSER_USERSCRIPTS_BROWSER_CLIENT_H_
#define USERSCRIPTS_BROWSER_USERSCRIPTS_BROWSER_CLIENT_H_

#include <memory>
#include <string>
#include <vector>

#include "content/public/browser/browser_context.h"

#include "../common/user_script.h"
#include "user_script_loader.h"
#include "user_script_prefs.h"

namespace user_scripts {

class UserScriptsBrowserClient {
 public:
  UserScriptsBrowserClient();
  UserScriptsBrowserClient(const UserScriptsBrowserClient&) = delete;
  UserScriptsBrowserClient& operator=(const UserScriptsBrowserClient&) = delete;
  virtual ~UserScriptsBrowserClient();

  // Returns the single instance of |this|.
  static UserScriptsBrowserClient* GetInstance();

  void SetProfile(content::BrowserContext* context);

  user_scripts::UserScriptsPrefs* GetPrefs() {
    return prefs_.get();
  }

  user_scripts::UserScriptLoader* GetLoader() {
    return userscript_loader_.get();
  }

  private:
    std::unique_ptr<UserScriptList> scripts_;
    content::BrowserContext* browser_context_;
    std::unique_ptr<user_scripts::UserScriptsPrefs> prefs_;
    std::unique_ptr<user_scripts::UserScriptLoader> userscript_loader_;
};

}  // namespace extensions

#endif  // USERSCRIPTS_BROWSER_USERSCRIPTS_BROWSER_CLIENT_H_
