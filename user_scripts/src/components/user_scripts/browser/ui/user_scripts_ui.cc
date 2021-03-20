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


#include <memory>

#include "base/bind.h"
#include "base/json/json_string_value_serializer.h"
#include "base/macros.h"
#include "base/memory/writable_shared_memory_region.h"
#include "base/strings/string_util.h"
#include "base/values.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/userscripts_browser_resources.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"

#include "user_scripts_ui.h"
#include "../userscripts_browser_client.h"
#include "../../common/user_script.h"

namespace {

class UserScriptsUIHandler : public content::WebUIMessageHandler {
 public:
  UserScriptsUIHandler();
  ~UserScriptsUIHandler() override;

  // content::WebUIMessageHandler:
  void RegisterMessages() override;

 private:
  void HandleRequestSource(const base::ListValue* args);
  void OnScriptsLoaded(
      const std::string callback_id,
      const std::string script_key,
      std::unique_ptr<user_scripts::UserScriptList> user_scripts,
      base::ReadOnlySharedMemoryRegion shared_memory);

  std::unique_ptr<user_scripts::UserScriptList> loaded_scripts_;

  base::WeakPtrFactory<UserScriptsUIHandler> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(UserScriptsUIHandler);
};

UserScriptsUIHandler::UserScriptsUIHandler()
  : loaded_scripts_(new user_scripts::UserScriptList()) {
}

UserScriptsUIHandler::~UserScriptsUIHandler() {
}

void UserScriptsUIHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "requestSource",
      base::BindRepeating(&UserScriptsUIHandler::HandleRequestSource,
                          base::Unretained(this)));
}

void UserScriptsUIHandler::HandleRequestSource(const base::ListValue* args) {
  AllowJavascript();
  std::string callback_id;
  CHECK(args->GetString(0, &callback_id));

  std::string script_key;
  if (args->GetString(1, &script_key) == false) {
    std::string json = "Missing key value.";
    ResolveJavascriptCallback(base::Value(callback_id), base::Value(json));
    return;
  }

  user_scripts::UserScriptsBrowserClient* client = user_scripts::UserScriptsBrowserClient::GetInstance();
  if (client == NULL) {
    std::string json = "User scripts disabled.";
    ResolveJavascriptCallback(base::Value(callback_id), base::Value(json));
  } else {
    std::unique_ptr<user_scripts::UserScriptList> scripts_to_load =
                                    std::move(loaded_scripts_);
    scripts_to_load->clear();

    client->GetLoader()->LoadScripts(std::move(scripts_to_load),
      base::BindOnce(
          &UserScriptsUIHandler::OnScriptsLoaded,
          weak_factory_.GetWeakPtr(),
          callback_id, script_key)
      );
  }
}

void UserScriptsUIHandler::OnScriptsLoaded(
    const std::string callback_id,
    const std::string script_key,
    std::unique_ptr<user_scripts::UserScriptList> user_scripts,
    base::ReadOnlySharedMemoryRegion shared_memory) {
  loaded_scripts_ = std::move(user_scripts);

  base::ListValue response;
  for (const std::unique_ptr<user_scripts::UserScript>& script : *loaded_scripts_) {
    if (script->key() == script_key) {
      auto scriptData = std::make_unique<base::DictionaryValue>();
      for (const std::unique_ptr<user_scripts::UserScript::File>& js_file :
          script->js_scripts()) {
        base::StringPiece contents = js_file->GetContent();
        scriptData->SetString("content", contents.data());
      }
      response.Append(std::move(scriptData));
    }
  }

  ResolveJavascriptCallback(base::Value(callback_id), response);
}

}  // namespace

namespace user_scripts {

UserScriptsUI::UserScriptsUI(content::WebUI* web_ui) : WebUIController(web_ui) {
  content::WebUIDataSource* html_source =
      content::WebUIDataSource::Create(kChromeUIUserScriptsHost);
  html_source->SetDefaultResource(IDR_USER_SCRIPTS_HTML);
  html_source->AddResourcePath("user-scripts-ui.js", IDR_USER_SCRIPTS_JS);
  content::WebUIDataSource::Add(Profile::FromWebUI(web_ui), html_source);
  web_ui->AddMessageHandler(std::make_unique<UserScriptsUIHandler>());
}

UserScriptsUI::~UserScriptsUI() {
}

}