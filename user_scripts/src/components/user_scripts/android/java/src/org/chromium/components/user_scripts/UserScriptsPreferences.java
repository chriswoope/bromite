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

package org.chromium.components.user_scripts;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.Context;
import android.content.Intent;
import android.content.DialogInterface;
import android.os.Bundle;
import android.provider.Browser;
import android.net.Uri;
import android.view.MenuItem;
import android.view.View;

import androidx.preference.Preference;
import androidx.preference.PreferenceFragmentCompat;

import org.chromium.base.Log;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.base.ActivityWindowAndroid;

import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.browser_ui.settings.TextMessagePreference;

import org.chromium.components.user_scripts.UserScriptsBridge;
import org.chromium.components.user_scripts.FragmentWindowAndroid;

public class UserScriptsPreferences
        extends PreferenceFragmentCompat
        implements SettingsUtils.ISupportHelpAndFeedback {

    private static final String PREF_ENABLED_SWITCH = "enabled_switch";
    private static final String PREF_SCRIPTLISTPREFERENCE = "script_list";
    private static final String PREF_SCRIPTS_DISABLED_DESCRIPTION = "scripts_disabled_description";

    public static final String EXTRA_SCRIPT_FILE = "org.chromium.chrome.preferences.script_file";

    private FragmentWindowAndroid mwindowAndroid;

    @Override
    public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
        getActivity().setTitle(R.string.prefs_userscripts_settings);
        SettingsUtils.addPreferencesFromResource(this, R.xml.userscripts_preferences);

        ChromeSwitchPreference enabledSwitch =
                (ChromeSwitchPreference) findPreference(PREF_ENABLED_SWITCH);
        ScriptListPreference listPreference =
                (ScriptListPreference) findPreference(PREF_SCRIPTLISTPREFERENCE);

        boolean enabled = UserScriptsBridge.isUserEnabled();
        enabledSwitch.setChecked(enabled);
        listPreference.setEnabled(enabled);
        enabledSwitch.setOnPreferenceChangeListener((preference, newValue) -> {
            UserScriptsBridge.setUserEnabled((boolean) newValue);
            listPreference.setEnabled((boolean) newValue);
            return true;
        });

        mwindowAndroid = new FragmentWindowAndroid(getContext(), this);
        listPreference.setWindowAndroid(mwindowAndroid);

        if (UserScriptsBridge.isFeatureEnabled() == false) {
            enabledSwitch.setEnabled(false);
            listPreference.setEnabled(false);

            TextMessagePreference scripts_disabled_description =
                (TextMessagePreference) findPreference(PREF_SCRIPTS_DISABLED_DESCRIPTION);
            scripts_disabled_description.setVisible(true);
        }
    }

    @Override
    public void onActivityResult(int requestCode, int resultCode, Intent data) {
        // handle picker callback from SelectFileDialog
        mwindowAndroid.onActivityResult(requestCode, resultCode, data);
    }

    public void onHelpAndFeebackPressed() {
        Context context = getContext();

        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse("https://github.com/bromite/bromite/wiki/UserScripts"));
        // Let Chromium know that this intent is from Chromium, so that it does not close the app when
        // the user presses 'back' button.
        intent.putExtra(Browser.EXTRA_APPLICATION_ID, context.getPackageName());
        intent.putExtra(Browser.EXTRA_CREATE_NEW_TAB, true);
        intent.setPackage(context.getPackageName());
        context.startActivity(intent);
    }

    public static Bundle createFragmentArgsForInstall(String filePath) {
        Bundle fragmentArgs = new Bundle();
        fragmentArgs.putSerializable(EXTRA_SCRIPT_FILE, filePath);
        return fragmentArgs;
    }

    @Override
    public void onActivityCreated(Bundle savedInstanceState) {
        super.onActivityCreated(savedInstanceState);

        String scriptToInstall = (String)getArguments().getSerializable(EXTRA_SCRIPT_FILE);
        if (scriptToInstall != null) {
            DialogInterface.OnClickListener dialogClickListener = new DialogInterface.OnClickListener() {
                @Override
                public void onClick(DialogInterface dialog, int which) {
                    switch (which){
                        case DialogInterface.BUTTON_POSITIVE:
                            UserScriptsBridge.TryToInstall(scriptToInstall);
                            break;

                        case DialogInterface.BUTTON_NEGATIVE:
                            break;
                    }
                }
            };

            Context context = getContext();
            String message = context.getString(R.string.ask_to_install) + " " +
                             scriptToInstall;

            AlertDialog.Builder builder = new AlertDialog.Builder(context);
            builder.setMessage(message)
                .setPositiveButton(context.getString(R.string.yes), dialogClickListener)
                .setNegativeButton(context.getString(R.string.no), dialogClickListener)
                .show();
        }
    }
}
