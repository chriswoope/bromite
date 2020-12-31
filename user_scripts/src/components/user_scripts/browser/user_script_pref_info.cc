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

#include "user_script_pref_info.h"

namespace user_scripts {

UserScriptsListPrefs::ScriptInfo::ScriptInfo(const std::string& name,
                                             const std::string& description,
                                             const base::Time& install_time,
                                             bool enabled)
    : install_time(install_time),
      enabled(enabled),
      name_(name),
      description_(description) {}

UserScriptsListPrefs::ScriptInfo::ScriptInfo(const ScriptInfo& other) = default;
UserScriptsListPrefs::ScriptInfo::~ScriptInfo() = default;

}