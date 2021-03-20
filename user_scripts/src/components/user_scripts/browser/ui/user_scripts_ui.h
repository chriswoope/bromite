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

#ifndef USERSCRIPTS_BROWSER_UI_USER_SCRIPTS_UI_H_
#define USERSCRIPTS_BROWSER_UI_USER_SCRIPTS_UI_H_

#include "base/macros.h"
#include "content/public/browser/web_ui_controller.h"

namespace user_scripts {

const char kChromeUIUserScriptsHost[] = "user-scripts";

class UserScriptsUI : public content::WebUIController {
 public:
  explicit UserScriptsUI(content::WebUI* web_ui);
  ~UserScriptsUI() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(UserScriptsUI);
};

}

#endif