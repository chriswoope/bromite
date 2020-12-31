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

#include <jni.h>
#include <algorithm>
#include <string>
#include <vector>
#include <sstream>
#include <iterator>

#include "base/android/callback_android.h"
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "components/embedder_support/android/browser_context/browser_context_handle.h"
#include "ui/android/window_android.h"

#include "components/user_scripts/android/user_scripts_jni_headers/UserScriptsBridge_jni.h"
#include "../browser/userscripts_browser_client.h"
#include "user_scripts_bridge.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF16ToJavaString;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;
using content::BrowserContext;

namespace {

user_scripts::UserScriptsBrowserClient* GetUserScriptsBrowserClient() {
  return user_scripts::UserScriptsBrowserClient::GetInstance();
}

class CallbackObserver : public user_scripts::UserScriptLoader::Observer {
  private:
    void OnScriptsLoaded(user_scripts::UserScriptLoader* loader,
                         content::BrowserContext* browser_context) override {
        user_scripts::ShouldRefreshUserScriptList(base::android::AttachCurrentThread());
    }

    void OnUserScriptLoaded(user_scripts::UserScriptLoader* loader,
                            bool result, const std::string& error) override {
        user_scripts::OnUserScriptLoaded(base::android::AttachCurrentThread(),
          result, error);
    }

    void OnUserScriptLoaderDestroyed(user_scripts::UserScriptLoader* loader) override {}
};

CallbackObserver* g_userscripts_loader_observer = NULL;

}

namespace user_scripts {

static jboolean JNI_UserScriptsBridge_IsFeatureEnabled(
                    JNIEnv* env) {
  return GetUserScriptsBrowserClient() != NULL;
}

static jboolean JNI_UserScriptsBridge_IsUserEnabled(
                    JNIEnv* env) {
  user_scripts::UserScriptsBrowserClient* client = GetUserScriptsBrowserClient();
  if (client == NULL) return false;
  return client->GetPrefs()->IsEnabled();
}

static void JNI_UserScriptsBridge_SetUserEnabled(
                    JNIEnv* env,
                    jboolean is_enabled) {
  user_scripts::UserScriptsBrowserClient* client = GetUserScriptsBrowserClient();
  if (client == NULL) return;
  client->GetPrefs()->SetEnabled(is_enabled);
}

static base::android::ScopedJavaLocalRef<jstring> JNI_UserScriptsBridge_GetScriptsInfo(
                    JNIEnv* env) {
  user_scripts::UserScriptsBrowserClient* client = GetUserScriptsBrowserClient();
  if (client == NULL) return ConvertUTF8ToJavaString(env, {});

  std::string json = client->GetPrefs()->GetScriptsInfo();
  return ConvertUTF8ToJavaString(env, json);
}

static void JNI_UserScriptsBridge_RemoveScript(
                    JNIEnv* env,
                    const JavaParamRef<jstring>& jscript_key) {
  user_scripts::UserScriptsBrowserClient* client = GetUserScriptsBrowserClient();
  if (client == NULL) return;

  std::string script_key = base::android::ConvertJavaStringToUTF8(jscript_key);
  client->GetLoader()->RemoveScript(script_key);
}

static void JNI_UserScriptsBridge_SetScriptEnabled(
                    JNIEnv* env,
                    const JavaParamRef<jstring>& jscript_key,
                    jboolean is_enabled) {
  user_scripts::UserScriptsBrowserClient* client = GetUserScriptsBrowserClient();
  if (client == NULL) return;

  std::string script_key = base::android::ConvertJavaStringToUTF8(jscript_key);
  client->GetLoader()->SetScriptEnabled(script_key, is_enabled);
}

static void JNI_UserScriptsBridge_Reload(
                    JNIEnv* env) {
  user_scripts::UserScriptsBrowserClient* client = GetUserScriptsBrowserClient();
  if (client == NULL) return;

  client->GetLoader()->StartLoad();
  user_scripts::ShouldRefreshUserScriptList(env);
}

static void JNI_UserScriptsBridge_SelectAndAddScriptFromFile(
                    JNIEnv* env,
                    const JavaParamRef<jobject>& jwindow_android) {
  user_scripts::UserScriptsBrowserClient* client = GetUserScriptsBrowserClient();
  if (client == NULL) return;

  client->GetLoader()->SelectAndAddScriptFromFile(
                    ui::WindowAndroid::FromJavaWindowAndroid(jwindow_android));
}

static void JNI_UserScriptsBridge_TryToInstall(JNIEnv* env,
                         const JavaParamRef<jstring>& jscript_path) {
  user_scripts::UserScriptsBrowserClient* client = GetUserScriptsBrowserClient();
  if (client == NULL) return;

  std::string script_path = base::android::ConvertJavaStringToUTF8(jscript_path);
  base::FilePath path(script_path);

  client->GetLoader()->TryToInstall(path);
}

static void JNI_UserScriptsBridge_RegisterLoadCallback(
                    JNIEnv* env) {
  user_scripts::UserScriptsBrowserClient* client = GetUserScriptsBrowserClient();
  if (client == NULL) return;

  if( g_userscripts_loader_observer == NULL ) {
    g_userscripts_loader_observer = new CallbackObserver();
    client->GetLoader()->AddObserver(g_userscripts_loader_observer);
  }
}

static void ShouldRefreshUserScriptList(JNIEnv* env) {
  Java_UserScriptsBridge_shouldRefreshUserScriptList(env);
}

static void OnUserScriptLoaded(JNIEnv* env,
              bool result, const std::string& error) {
  base::android::ScopedJavaLocalRef<jstring> j_error =
    base::android::ConvertUTF8ToJavaString(env, error);

  Java_UserScriptsBridge_onUserScriptLoaded(env, result, j_error);
}

}