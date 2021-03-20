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

package org.chromium.chrome.browser.user_scripts;

import android.content.Context;
import android.content.Intent;
import android.provider.Browser;
import android.net.Uri;

import org.chromium.base.ContextUtils;
import org.chromium.base.IntentUtils;

import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.settings.SettingsLauncherImpl;
import org.chromium.components.browser_ui.settings.SettingsLauncher;

import org.chromium.components.user_scripts.UserScriptsPreferences;
import org.chromium.components.user_scripts.UserScriptsBridge;
import org.chromium.components.user_scripts.IUserScriptsUtils;

public class UserScriptsUtils implements IUserScriptsUtils
{
    private static UserScriptsUtils instance;

    private UserScriptsUtils() {}

    public static void Initialize() {
        instance = new UserScriptsUtils();
        UserScriptsBridge.registerUtils(instance);
    }

    public static UserScriptsUtils getInstance() {
        return instance;
    }

    public boolean openFile(String filePath, String mimeType, String downloadGuid,
                                   boolean isOffTheRecord, String originalUrl, String referrer) {
        if (UserScriptsBridge.isUserEnabled() == false) return false;

        if (filePath.toUpperCase().endsWith(".USER.JS") == false) return false;

        Context context = ContextUtils.getApplicationContext();

        SettingsLauncher settingsLauncher = new SettingsLauncherImpl();
        Intent intent = settingsLauncher.createSettingsActivityIntent(
                context, UserScriptsPreferences.class.getName(),
                UserScriptsPreferences.createFragmentArgsForInstall(filePath));
        IntentUtils.safeStartActivity(context, intent);

        return true;
    }

    public void openSourceFile(String scriptKey) {
        Context context = ContextUtils.getApplicationContext();

        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse("chrome://user-scripts/?key=" + scriptKey));
        intent.putExtra(Browser.EXTRA_APPLICATION_ID, context.getPackageName());
        intent.putExtra(Browser.EXTRA_CREATE_NEW_TAB, true);
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.setPackage(context.getPackageName());
        IntentHandler.startChromeLauncherActivityForTrustedIntent(intent);
    }
}