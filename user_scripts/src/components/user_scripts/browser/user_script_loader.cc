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

#include "user_script_loader.h"

#include <stddef.h>

#include <set>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/memory/writable_shared_memory_region.h"
#include "base/strings/string_util.h"
#include "base/version.h"
#include "base/task/task_traits.h"
#include "base/files/file_util.h"
#include "base/files/file_enumerator.h"
#include "base/path_service.h"
#include "base/base_paths_android.h"
#include "base/strings/utf_string_conversions.h"
#include "base/android/content_uri_utils.h"
#include "base/android/jni_android.h"

#include "crypto/sha2.h"
#include "base/base64.h"

#include "build/build_config.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"
#include "ui/shell_dialogs/select_file_dialog.h"
#include "content/browser/file_system_access/file_system_chooser.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "ui/android/window_android.h"

#include "../common/user_scripts_features.h"
#include "../common/extension_messages.h"
#include "file_task_runner.h"
#include "user_script_prefs.h"
#include "user_script_pref_info.h"

using content::BrowserThread;
using content::BrowserContext;

namespace user_scripts {

namespace {

bool invalidChar(unsigned char c)
{
  return !(c>=0 && c <128);
}

void stripUnicode(std::string& str)
{
  str.erase(remove_if(str.begin(),str.end(), invalidChar), str.end());
}

// Helper function to parse greasesmonkey headers
bool GetDeclarationValue(const base::StringPiece& line,
                         const base::StringPiece& prefix,
                         std::string* value) {
  base::StringPiece::size_type index = line.find(prefix);
  if (index == base::StringPiece::npos)
    return false;

  std::string temp(line.data() + index + prefix.length(),
                   line.length() - index - prefix.length());

  if (temp.empty() || !base::IsUnicodeWhitespace(temp[0]))
    return false;

  base::TrimWhitespaceASCII(temp, base::TRIM_ALL, value);
  return true;
}

}  // namespace

// static
bool UserScriptLoader::ParseMetadataHeader(const base::StringPiece& script_text,
                                           std::unique_ptr<UserScript>& script,
                                           std::string& error_message) {
  // http://wiki.greasespot.net/Metadata_block
  base::StringPiece line;
  size_t line_start = 0;
  size_t line_end = line_start;
  bool in_metadata = false;

  static const base::StringPiece kUserScriptBegin("// ==UserScript==");
  static const base::StringPiece kUserScriptEng("// ==/UserScript==");
  static const base::StringPiece kNamespaceDeclaration("// @namespace");
  static const base::StringPiece kNameDeclaration("// @name");
  static const base::StringPiece kVersionDeclaration("// @version");
  static const base::StringPiece kDescriptionDeclaration("// @description");
  static const base::StringPiece kIncludeDeclaration("// @include");
  static const base::StringPiece kExcludeDeclaration("// @exclude");
  static const base::StringPiece kMatchDeclaration("// @match");
  static const base::StringPiece kExcludeMatchDeclaration("// @exclude_match");
  static const base::StringPiece kRunAtDeclaration("// @run-at");
  static const base::StringPiece kRunAtDocumentStartValue("document-start");
  static const base::StringPiece kRunAtDocumentEndValue("document-end");
  static const base::StringPiece kRunAtDocumentIdleValue("document-idle");
  static const base::StringPiece kUrlSourceDeclaration("// @url");

  while (line_start < script_text.length()) {
    line_end = script_text.find('\n', line_start);

    // Handle the case where there is no trailing newline in the file.
    if (line_end == std::string::npos)
      line_end = script_text.length() - 1;

    line = base::StringPiece(script_text.data() + line_start,
                             line_end - line_start);

    if (!in_metadata) {
      if (base::StartsWith(line, kUserScriptBegin))
        in_metadata = true;
    } else {
      if (base::StartsWith(line, kUserScriptEng))
        break;

      std::string value;
      if (GetDeclarationValue(line, kIncludeDeclaration, &value)) {
        // We escape some characters that MatchPattern() considers special.
        base::ReplaceSubstringsAfterOffset(&value, 0, "\\", "\\\\");
        base::ReplaceSubstringsAfterOffset(&value, 0, "?", "\\?");
        script->add_glob(value);
      } else if (GetDeclarationValue(line, kExcludeDeclaration, &value)) {
        base::ReplaceSubstringsAfterOffset(&value, 0, "\\", "\\\\");
        base::ReplaceSubstringsAfterOffset(&value, 0, "?", "\\?");
        script->add_exclude_glob(value);
      } else if (GetDeclarationValue(line, kNamespaceDeclaration, &value)) {
        script->set_name_space(value);
      } else if (GetDeclarationValue(line, kNameDeclaration, &value)) {
        script->set_name(value);
      } else if (GetDeclarationValue(line, kVersionDeclaration, &value)) {
        base::Version version(value);
        if (version.IsValid())
          script->set_version(version.GetString());
      } else if (GetDeclarationValue(line, kDescriptionDeclaration, &value)) {
        script->set_description(value);
      } else if (GetDeclarationValue(line, kMatchDeclaration, &value)) {
        URLPattern pattern(UserScript::ValidUserScriptSchemes());
        if (URLPattern::ParseResult::kSuccess != pattern.Parse(value)) {
          error_message = "Invalid UserScript Schema " + value;
          return false;
        }
        script->add_url_pattern(pattern);
      } else if (GetDeclarationValue(line, kExcludeMatchDeclaration, &value)) {
        URLPattern exclude(UserScript::ValidUserScriptSchemes());
        if (URLPattern::ParseResult::kSuccess != exclude.Parse(value)) {
          error_message = "Invalid UserScript Schema " + value;
          return false;
        }
        script->add_exclude_url_pattern(exclude);
      } else if (GetDeclarationValue(line, kRunAtDeclaration, &value)) {
        if (value == kRunAtDocumentStartValue)
          script->set_run_location(UserScript::DOCUMENT_START);
        else if (value == kRunAtDocumentEndValue)
          script->set_run_location(UserScript::DOCUMENT_END);
        else if (value == kRunAtDocumentIdleValue)
          script->set_run_location(UserScript::DOCUMENT_IDLE);
        else {
          error_message = "Invalid RunAtDeclaration " + value;
          return false;
        }
      } else if(GetDeclarationValue(line, kUrlSourceDeclaration, &value)) {
        script->set_url_source(value);
      }

      // TODO(aa): Handle more types of metadata.
    }

    line_start = line_end + 1;
  }

  // If no patterns were specified, default to @include *. This is what
  // Greasemonkey does.
  if (script->globs().empty() && script->url_patterns().is_empty())
    script->add_glob("*");

  return true;
}

// static
bool LoadUserScriptFromFile(
    const base::FilePath& user_script_path, const GURL& original_url,
    std::unique_ptr<UserScript>& script,
    base::string16* error) {

    std::string script_key = user_script_path.BaseName().value();

    std::string content;
    if (user_script_path.IsContentUri()) {
      if (base::FeatureList::IsEnabled(features::kEnableLoggingUserScripts))
        LOG(INFO) << "UserScriptLoader: path " << user_script_path << " is a content uri";

      base::FilePath tempFilePath;
      if( base::CreateTemporaryFile(&tempFilePath) == false ) {
        *error = base::ASCIIToUTF16("Could not read create temp file.");
        return false;
      }

      if( base::CopyFile(user_script_path, tempFilePath) == false ) {
        *error = base::ASCIIToUTF16("Could not copy file to temp file.");
        return false;
      }

      if (!base::ReadFileToString(tempFilePath, &content)) {
        *error = base::ASCIIToUTF16("Could not read source file from temp path.");
        return false;
      }
    } else {
      if (!base::ReadFileToString(user_script_path, &content)) {
        *error = base::ASCIIToUTF16("Could not read source file.");
        return false;
      }
    }

    if (!base::IsStringUTF8(content)) {
      *error = base::ASCIIToUTF16("User script must be UTF8 encoded.");
      return false;
    }

    std::string detailed_error;
    if (!UserScriptLoader::ParseMetadataHeader(content, script, detailed_error)) {
      *error = base::ASCIIToUTF16("Invalid script header. " + detailed_error);
      return false;
    }

    // add into key the filename
    // this value is used in ui to discriminate scripts
    script->set_key(script_key);

    script->set_match_origin_as_fallback(MatchOriginAsFallbackBehavior::kNever);

    // remove unicode chars and set content into File
    stripUnicode(content);
    std::unique_ptr<UserScript::File> file(new UserScript::File());
    file->set_content(content);
    file->set_url(GURL(/*script_key*/ "script.js")); // name doesn't matter

    // create SHA256 of file
    char raw[crypto::kSHA256Length] = {0};
    std::string key;
    crypto::SHA256HashString(content, raw, crypto::kSHA256Length);
    base::Base64Encode(base::StringPiece(raw, crypto::kSHA256Length), &key);
    file->set_key(key);

    script->js_scripts().push_back(std::move(file));

    return true;
}

// static
bool GetOrCreatePath(base::FilePath& path) {
  base::PathService::Get(base::DIR_ANDROID_APP_DATA, &path);
  path = path.AppendASCII("userscripts");

  // create snippets directory if not exists
  if(!base::PathExists(path)) {
    LOG(INFO) << "Path " << path << " doesn't exists. Creating";
    base::File::Error error = base::File::FILE_OK;
    if( !base::CreateDirectoryAndGetError(path, &error) ) {
      LOG(ERROR) <<
               "UserScriptLoader: failed to create directory: " << path
               << " with error code " << error;
      return false;
    }
  }
  return true;
}

// static
void LoadUserScripts(UserScriptList* user_scripts_list) {
  base::FilePath path;
  if (GetOrCreatePath(path) == false) return;

  // enumerate all files from script path
  // we accept all files, but we check if it's a real
  // userscript
  base::FileEnumerator dir_enum(
    path,
    /*recursive=*/false, base::FileEnumerator::FILES);
  base::FilePath full_name;
  while (full_name = dir_enum.Next(), !full_name.empty()) {
    std::unique_ptr<UserScript> userscript(new UserScript());

    base::string16 error;
    if (LoadUserScriptFromFile(full_name, GURL(), userscript, &error)) {
      if (base::FeatureList::IsEnabled(features::kEnableLoggingUserScripts))
        LOG(INFO) << "UserScriptLoader: Found user script " << userscript->name() <<
              "-" << userscript->version() <<
              "-" << userscript->description();

      userscript->set_file_path(full_name.AsUTF8Unsafe());
      user_scripts_list->push_back(std::move(userscript));
    } else {
      LOG(ERROR) << "UserScriptLoader: load error " << error;
    }
  }
}

UserScriptLoader::UserScriptLoader(BrowserContext* browser_context,
                                   UserScriptsPrefs* prefs)
    : loaded_scripts_(new UserScriptList()),
      ready_(false),
      browser_context_(browser_context),
      prefs_(prefs) {}

UserScriptLoader::~UserScriptLoader() {
  for (auto& observer : observers_)
     observer.OnUserScriptLoaderDestroyed(this);
}

void UserScriptLoader::OnRenderProcessHostCreated(
    content::RenderProcessHost* process_host) {
  if (initial_load_complete()) {
    SendUpdate(process_host, shared_memory_);
  }
}

void UserScriptLoader::AttemptLoad() {
  int tryOut = prefs_->GetCurrentStartupTryout();
  if (tryOut >= 3) {
    LOG(INFO) << "UserScriptLoader: Possible crash detected. UserScript disabled";
    prefs_->SetEnabled(false);
  } else {
    prefs_->StartupTryout(tryOut+1);
    StartLoad();
  }
}

void UserScriptLoader::StartLoad() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (base::FeatureList::IsEnabled(features::kEnableLoggingUserScripts))
    LOG(INFO) << "UserScriptLoader: StartLoad";

  // Reload any loaded scripts, and clear out |loaded_scripts_| to indicate that
  // the scripts aren't currently ready.
  std::unique_ptr<UserScriptList> scripts_to_load = std::move(loaded_scripts_);
  scripts_to_load->clear();

  LoadScripts(std::move(scripts_to_load),
              base::BindOnce(&UserScriptLoader::OnScriptsLoaded,
                             weak_factory_.GetWeakPtr()));
}

// static
base::ReadOnlySharedMemoryRegion UserScriptLoader::Serialize(
    const UserScriptList& scripts) {
  base::Pickle pickle;
  pickle.WriteUInt32(scripts.size());
  for (const std::unique_ptr<UserScript>& script : scripts) {
    // TODO(aa): This can be replaced by sending content script metadata to
    // renderers along with other extension data in ExtensionMsg_Loaded.
    // See crbug.com/70516.
    script->Pickle(&pickle);
    // Write scripts as 'data' so that we can read it out in the slave without
    // allocating a new string.
    for (const std::unique_ptr<UserScript::File>& js_file :
         script->js_scripts()) {
      base::StringPiece contents = js_file->GetContent();
      pickle.WriteData(contents.data(), contents.length());
    }
    for (const std::unique_ptr<UserScript::File>& css_file :
         script->css_scripts()) {
      base::StringPiece contents = css_file->GetContent();
      pickle.WriteData(contents.data(), contents.length());
    }
  }

  // Create the shared memory object.
  base::MappedReadOnlyRegion shared_memory =
      base::ReadOnlySharedMemoryRegion::Create(pickle.size());
  if (!shared_memory.IsValid())
    return {};

  // Copy the pickle to shared memory.
  memcpy(shared_memory.mapping.memory(), pickle.data(), pickle.size());
  return std::move(shared_memory.region);
}

void UserScriptLoader::AddObserver(Observer* observer) {
   observers_.AddObserver(observer);
}

void UserScriptLoader::RemoveObserver(Observer* observer) {
   observers_.RemoveObserver(observer);
}

void UserScriptLoader::SetReady(bool ready) {
  bool was_ready = ready_;
  ready_ = ready;
  if (ready_ && !was_ready)
    AttemptLoad();
}

void UserScriptLoader::OnScriptsLoaded(
    std::unique_ptr<UserScriptList> user_scripts,
    base::ReadOnlySharedMemoryRegion shared_memory) {

  if (base::FeatureList::IsEnabled(features::kEnableLoggingUserScripts))
    LOG(INFO) << "UserScriptLoader: OnScriptsLoaded";

  // Check user preferences for loaded user scripts
  prefs_->CompareWithPrefs(*user_scripts);
  loaded_scripts_ = std::move(user_scripts);

  shared_memory =
      UserScriptLoader::Serialize(*loaded_scripts_);

  if (!shared_memory.IsValid()) {
    // This can happen if we run out of file descriptors.  In that case, we
    // have a choice between silently omitting all user scripts for new tabs,
    // by nulling out shared_memory_, or only silently omitting new ones by
    // leaving the existing object in place. The second seems less bad, even
    // though it removes the possibility that freeing the shared memory block
    // would open up enough FDs for long enough for a retry to succeed.

    // Pretend the extension change didn't happen.
    return;
  }

  // We've got scripts ready to go.
  shared_memory_ = std::move(shared_memory);

  for (content::RenderProcessHost::iterator i(
           content::RenderProcessHost::AllHostsIterator());
       !i.IsAtEnd(); i.Advance()) {
    SendUpdate(i.GetCurrentValue(), shared_memory_);
  }

  // DCHECK(false); trying crash
  prefs_->StartupTryout(0);

  for (auto& observer : observers_)
    observer.OnScriptsLoaded(this, browser_context_);
}

void UserScriptLoader::SendUpdate(
    content::RenderProcessHost* process,
    const base::ReadOnlySharedMemoryRegion& shared_memory) {
  if (base::FeatureList::IsEnabled(features::kEnableLoggingUserScripts))
    LOG(INFO) << "UserScriptLoader: SendUpdate";

  // If the process is being started asynchronously, early return.  We'll end up
  // calling InitUserScripts when it's created which will call this again.
  base::ProcessHandle handle = process->GetProcess().Handle();
  if (!handle)
    return;

  base::ReadOnlySharedMemoryRegion region_for_process =
      shared_memory.Duplicate();
  if (!region_for_process.IsValid())
    return;

  process->Send(new ExtensionMsg_UpdateUserScripts(
     std::move(region_for_process)));
}

void LoadScriptsOnFileTaskRunner(
    std::unique_ptr<UserScriptList> user_scripts,
    UserScriptLoader::LoadScriptsCallback callback) {
  DCHECK(GetUserScriptsFileTaskRunner()->RunsTasksInCurrentSequence());
  DCHECK(user_scripts.get());

  // load user scripts from path
  LoadUserScripts(user_scripts.get());

  base::ReadOnlySharedMemoryRegion memory;

  // Explicit priority to prevent unwanted task priority inheritance.
  content::GetUIThreadTaskRunner({base::TaskPriority::USER_BLOCKING})
      ->PostTask(FROM_HERE,
                 base::BindOnce(std::move(callback), std::move(user_scripts),
                                std::move(memory)));
}

void UserScriptLoader::LoadScripts(
    std::unique_ptr<UserScriptList> user_scripts,
    LoadScriptsCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  GetUserScriptsFileTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&LoadScriptsOnFileTaskRunner, std::move(user_scripts),
                     std::move(callback)));
}

void RemoveScriptsOnFileTaskRunner(
    const std::string& script_id,
    UserScriptLoader::RemoveScriptCallback callback) {
  DCHECK(GetUserScriptsFileTaskRunner()->RunsTasksInCurrentSequence());

  base::FilePath path;
  if (GetOrCreatePath(path)) {
    base::FilePath file = path.Append(script_id);
    if( base::DeleteFile(file) == false ) {
      LOG(ERROR) <<
               "ERROR: failed to delete file : " << path;
    }
  }

  content::GetUIThreadTaskRunner({base::TaskPriority::USER_BLOCKING})
      ->PostTask(FROM_HERE,
                 base::BindOnce(std::move(callback)));
}

void UserScriptLoader::OnScriptRemoved() {
  StartLoad();
}

void UserScriptLoader::RemoveScript(const std::string& script_id) {
  prefs_->RemoveScriptFromPrefs(script_id);

  GetUserScriptsFileTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&RemoveScriptsOnFileTaskRunner,
                     std::move(script_id),
                     base::BindOnce(&UserScriptLoader::OnScriptRemoved,
                             weak_factory_.GetWeakPtr())));
}

void UserScriptLoader::SetScriptEnabled(const std::string& script_id, bool is_enabled) {
  prefs_->SetScriptEnabled(script_id, is_enabled);
  StartLoad();
}

void UserScriptLoader::SelectAndAddScriptFromFile(ui::WindowAndroid* nativeWindow) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  dialog_ = ui::SelectFileDialog::Create(
      this, std::make_unique<ChromeSelectFilePolicy>(nullptr /*web_contents*/));

  ui::SelectFileDialog::FileTypeInfo allowed_file_info;
  allowed_file_info.extensions = {{FILE_PATH_LITERAL("js")}};
  allowed_file_info.allowed_paths =
      ui::SelectFileDialog::FileTypeInfo::ANY_PATH;
  base::FilePath suggested_name;

  std::vector<base::string16> types;
  types.push_back(base::ASCIIToUTF16("*/*")); /*= java SelectFileDialog.ALL_TYPES*/
  std::pair<std::vector<base::string16>, bool> accept_types = std::make_pair(
      types, false /*use_media_capture*/);

  dialog_->SelectFile(
      ui::SelectFileDialog::SELECT_OPEN_FILE,
      base::string16() /* dialog title*/, suggested_name, &allowed_file_info,
      0 /* file type index */, std::string() /* default file extension */,
      nativeWindow,
      &accept_types /* params */);
}


void LoadScriptFromPathOnFileTaskRunner(
  const base::FilePath& path,
  const std::string& display_name,
  UserScriptLoader::LoadSingleScriptCallback callback ) {
  DCHECK(GetUserScriptsFileTaskRunner()->RunsTasksInCurrentSequence());

  std::unique_ptr<UserScript> userscript(new UserScript());
  base::string16 error;
  bool result = LoadUserScriptFromFile(path, GURL(), userscript, &error);

  if(result) {
    if (display_name.empty() == false) {
      userscript->set_key(display_name);
    }

    if (base::FeatureList::IsEnabled(features::kEnableLoggingUserScripts))
      LOG(INFO) << "UserScriptLoader: Loaded " << userscript->name() <<
                                    "-" << userscript->version() <<
                                    "-" << userscript->description();
    base::FilePath destination;
    result = GetOrCreatePath(destination);
    if( result == false ) {
      error = base::ASCIIToUTF16("Cannot create destination.");
    } else {
      destination = destination.Append(userscript->key());
      result = base::CopyFile(path, destination);
      if (result == false) {
        error = base::ASCIIToUTF16("Copy error.");
      }
    }
  } else {
    LOG(ERROR) << "UserScriptLoader: load error " << error;
  }

  const std::string string_error = base::UTF16ToASCII(error);

  content::GetUIThreadTaskRunner({base::TaskPriority::USER_BLOCKING})
      ->PostTask(FROM_HERE,
                 base::BindOnce(std::move(callback), result,
                                std::move(string_error)));
}

void UserScriptLoader::TryToInstall(const base::FilePath& script_path) {
  base::string16 file_display_name;
  base::MaybeGetFileDisplayName(script_path, &file_display_name);

  std::string display_name = script_path.BaseName().value();
  if (base::IsStringASCII(file_display_name))
    display_name = base::UTF16ToASCII(file_display_name);

  GetUserScriptsFileTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(
        &LoadScriptFromPathOnFileTaskRunner,
        script_path, display_name,
        base::BindOnce(
            &UserScriptLoader::LoadScriptFromPathOnFileTaskRunnerCallback,
            weak_factory_.GetWeakPtr()
        )
      ));
}

void UserScriptLoader::FileSelected(
    const base::FilePath& path, int index, void* params) {
  if (base::FeatureList::IsEnabled(features::kEnableLoggingUserScripts))
    LOG(INFO) << "UserScriptLoader: FileSelected " << path;

  UserScriptLoader::TryToInstall(path);
}

void UserScriptLoader::LoadScriptFromPathOnFileTaskRunnerCallback(
              bool result, const std::string& error) {
  for (auto& observer : observers_)
     observer.OnUserScriptLoaded(this, result, error);

  StartLoad();
}

void UserScriptLoader::FileSelectionCanceled(
    void* params) {
}

}  // namespace extensions
