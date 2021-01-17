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

#include <map>

#include "base/values.h"
#include "base/strings/string_number_conversions.h"
#include "base/json/json_writer.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"

#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "user_script_prefs.h"
#include "user_script_pref_info.h"
#include "../common/user_script.h"
#include "../common/user_scripts_features.h"

namespace user_scripts {

namespace {

const char kUserScriptsEnabled[] = "userscripts.enabled";

const char kUserScriptsList[] = "userscripts.scripts";
const char kScriptIsEnabled[] = "enabled";
const char kScriptName[] = "name";
const char kScriptDescription[] = "description";
const char kScriptVersion[] = "version";
const char kScriptInstallTime[] = "install_time";

class PrefUpdate : public DictionaryPrefUpdate {
 public:
  PrefUpdate(PrefService* service,
             const std::string& id,
             const std::string& path)
      : DictionaryPrefUpdate(service, path), id_(id) {}

  ~PrefUpdate() override = default;

  base::DictionaryValue* Get() override {
    base::DictionaryValue* dict = DictionaryPrefUpdate::Get();
    base::Value* dict_item =
        dict->FindKeyOfType(id_, base::Value::Type::DICTIONARY);
    if (!dict_item)
      dict_item = dict->SetKey(id_, base::Value(base::Value::Type::DICTIONARY));
    return static_cast<base::DictionaryValue*>(dict_item);
  }

 private:
  const std::string id_;

  DISALLOW_COPY_AND_ASSIGN(PrefUpdate);
};

bool GetInt64FromPref(const base::DictionaryValue* dict,
                      const std::string& key,
                      int64_t* value) {
  DCHECK(dict);
  std::string value_str;
  if (!dict->GetStringWithoutPathExpansion(key, &value_str)) {
    VLOG(2) << "Can't find key in local pref dictionary. Invalid key: " << key
            << ".";
    return false;
  }

  if (!base::StringToInt64(value_str, value)) {
    VLOG(2) << "Can't change string to int64_t. Invalid string value: "
            << value_str << ".";
    return false;
  }

  return true;
}

}

UserScriptsPrefs::UserScriptsPrefs(
    PrefService* prefs)
    : prefs_(prefs) {
}

// static
void UserScriptsPrefs::RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(kUserScriptsEnabled, false);
  registry->RegisterDictionaryPref(kUserScriptsList);
}

bool UserScriptsPrefs::IsEnabled() {
  return prefs_->GetBoolean(kUserScriptsEnabled);
}

void UserScriptsPrefs::SetEnabled(bool enabled) {
  prefs_->SetBoolean(kUserScriptsEnabled, enabled);
}

void UserScriptsPrefs::CompareWithPrefs(UserScriptList& user_scripts) {
  if( IsEnabled() == false ) {
    if (base::FeatureList::IsEnabled(features::kEnableLoggingUserScripts))
      LOG(INFO) << "UserScriptsPrefs: disabled by user";

    user_scripts.clear();
    return;
  }

  std::vector<std::string> all_scripts;

  auto it = user_scripts.begin();
  while (it != user_scripts.end())
  {
    std::string key = it->get()->key();
    all_scripts.push_back(key);

    std::unique_ptr<UserScriptsListPrefs::ScriptInfo> scriptInfo =
        UserScriptsPrefs::CreateScriptInfoFromPrefs(key);

    // add or update prefs
    scriptInfo->set_name(it->get()->name());
    scriptInfo->set_description(it->get()->description());
    scriptInfo->set_version(it->get()->version());

    PrefUpdate update(prefs_, key, kUserScriptsList);
    base::DictionaryValue* script_dict = update.Get();

    script_dict->SetString(kScriptName, scriptInfo->name());
    script_dict->SetString(kScriptDescription, scriptInfo->description());
    script_dict->SetBoolean(kScriptIsEnabled, scriptInfo->enabled);
    script_dict->SetString(kScriptVersion, scriptInfo->version());

    std::string install_time_str =
        base::NumberToString(scriptInfo->install_time.ToInternalValue());
    script_dict->SetString(kScriptInstallTime, install_time_str);

   if (!scriptInfo->enabled) {
      it = user_scripts.erase(it);
    } else {
      ++it;
    }
  }

  // remove script from prefs if no more present
  std::vector<std::string> all_scripts_to_remove;
  const base::DictionaryValue* dict =
    prefs_->GetDictionary(kUserScriptsList);
  for (base::DictionaryValue::Iterator script_it(*dict); !script_it.IsAtEnd();
       script_it.Advance()) {
    const std::string& key = script_it.key();

    if (std::find(all_scripts.begin(), all_scripts.end(), key) == all_scripts.end()) {
      all_scripts_to_remove.push_back(key);
    }
  }

  DictionaryPrefUpdate update(prefs_, kUserScriptsList);
  base::DictionaryValue* const update_dict = update.Get();
  for (auto key : all_scripts_to_remove) {
      update_dict->RemoveKey(key);
  }

  return;
}

std::string UserScriptsPrefs::GetScriptsInfo() {
  std::string json_string;

  const base::DictionaryValue* dict =
    prefs_->GetDictionary(kUserScriptsList);

  if (dict) {
    base::JSONWriter::WriteWithOptions(
        *dict, base::JSONWriter::OPTIONS_PRETTY_PRINT, &json_string);
    base::TrimWhitespaceASCII(json_string, base::TRIM_ALL, &json_string);
  }

  return json_string;
}

std::unique_ptr<UserScriptsListPrefs::ScriptInfo> UserScriptsPrefs::CreateScriptInfoFromPrefs(
    const std::string& script_id) const {

  auto scriptInfo = std::make_unique<UserScriptsListPrefs::ScriptInfo>(
      script_id, "", base::Time::Now(), false);

  const base::DictionaryValue* scripts =
      prefs_->GetDictionary(kUserScriptsList);
  if (!scripts)
    return scriptInfo;

  const base::DictionaryValue* script = static_cast<const base::DictionaryValue*>(
    scripts->FindDictKey(script_id));
  if (!script)
    return scriptInfo;

  const std::string* name = script->FindStringKey(kScriptName);
  const std::string* description = script->FindStringKey(kScriptDescription);
  const std::string* version = script->FindStringKey(kScriptVersion);

  scriptInfo->set_name( name ? *name : "no name" );
  scriptInfo->set_description( description ? *description : "no description" );
  scriptInfo->set_version( version ? *version : "no version" );
  scriptInfo->enabled = script->FindBoolKey(kScriptIsEnabled).value_or(false);

  int64_t time_interval = 0;
  if (GetInt64FromPref(script, kScriptInstallTime, &time_interval)) {
    scriptInfo->install_time = base::Time::FromInternalValue(time_interval);
  }

  return scriptInfo;
}

void UserScriptsPrefs::RemoveScriptFromPrefs(const std::string& script_id) {
  DictionaryPrefUpdate update(prefs_, kUserScriptsList);
  base::DictionaryValue* const update_dict = update.Get();
  update_dict->RemoveKey(script_id);
}

void UserScriptsPrefs::SetScriptEnabled(const std::string& script_id, bool is_enabled) {
  PrefUpdate update(prefs_, script_id, kUserScriptsList);
  base::DictionaryValue* script_dict = update.Get();
  script_dict->SetBoolean(kScriptIsEnabled, is_enabled);
}

}
