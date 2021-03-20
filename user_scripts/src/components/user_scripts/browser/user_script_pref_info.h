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

#ifndef USERSCRIPTS_BROWSER_USERSCRIPT_PREF_INFO_H_
#define USERSCRIPTS_BROWSER_USERSCRIPT_PREF_INFO_H_

#include "base/values.h"
#include "base/time/time.h"
#include "components/keyed_service/core/keyed_service.h"

namespace user_scripts {

class UserScriptsListPrefs : public KeyedService {
 public:
  struct ScriptInfo {
    ScriptInfo(const std::string& name,
               const std::string& description,
               const base::Time& install_time,
               bool enabled);
    ScriptInfo(const ScriptInfo& other);
    ~ScriptInfo();

    const std::string& name() const { return name_; }
    void set_name(const std::string& name) { name_ = name; }

    const std::string& description() const { return description_; }
    void set_description(const std::string& description) { description_ = description; }

    const std::string& version() const { return version_; }
    void set_version(const std::string& version) { version_ = version; }

    const std::string& file_path() const { return file_path_; }
    void set_file_path(const std::string& file_path) { file_path_ = file_path; }

    const std::string& url_source() const { return url_source_; }
    void set_url_source(const std::string& url_source) { url_source_ = url_source; }

    base::Time install_time;
    bool enabled;

  private:
    std::string name_;
    std::string description_;
    std::string version_;
    std::string file_path_;
    std::string url_source_;
  };
};

}

#endif // USERSCRIPTS_BROWSER_USERSCRIPT_PREF_INFO_H_