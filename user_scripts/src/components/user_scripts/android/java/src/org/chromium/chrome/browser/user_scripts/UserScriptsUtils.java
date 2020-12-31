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

import org.chromium.base.ContextUtils;
import org.chromium.base.IntentUtils;

import org.chromium.chrome.browser.settings.SettingsLauncherImpl;
import org.chromium.chrome.browser.settings.SettingsLauncher;

import org.chromium.components.user_scripts.UserScriptsPreferences;
import org.chromium.components.user_scripts.UserScriptsBridge;

public class UserScriptsUtils {
    public static boolean openFile(String filePath, String mimeType, String downloadGuid,
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
}