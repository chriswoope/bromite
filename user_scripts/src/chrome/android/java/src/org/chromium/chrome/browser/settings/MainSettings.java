// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import android.content.Context;
import android.content.Intent;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.provider.Settings;
import android.view.View;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.preference.Preference;
import androidx.preference.PreferenceFragmentCompat;

import org.chromium.base.ContextUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.homepage.HomepageManager;
import org.chromium.chrome.browser.night_mode.NightModeUtils;
import org.chromium.chrome.browser.offlinepages.prefetch.PrefetchConfiguration;
import org.chromium.chrome.browser.password_check.PasswordCheck;
import org.chromium.chrome.browser.password_check.PasswordCheckFactory;
import org.chromium.chrome.browser.password_manager.ManagePasswordsReferrer;
import org.chromium.chrome.browser.password_manager.PasswordManagerLauncher;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.safety_check.SafetyCheckSettingsFragment;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.tracing.settings.DeveloperSettings;
import org.chromium.components.browser_ui.settings.ChromeBasePreference;
import org.chromium.components.browser_ui.settings.ManagedPreferenceDelegate;
import org.chromium.components.browser_ui.settings.SettingsLauncher;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.search_engines.TemplateUrl;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.metrics.SigninAccessPoint;

import java.util.HashMap;
import java.util.Map;

/**
 * The main settings screen, shown when the user first opens Settings.
 */
public class MainSettings extends PreferenceFragmentCompat
        implements TemplateUrlService.LoadListener {
    public static final String PREF_ACCOUNT_SECTION = "account_section";
    public static final String PREF_ACCOUNT_AND_GOOGLE_SERVICES_SECTION =
            "account_and_google_services_section";
    public static final String PREF_SIGN_IN = "sign_in";
    public static final String PREF_SEARCH_ENGINE = "search_engine";
    public static final String PREF_PASSWORDS = "passwords";
    public static final String PREF_HOMEPAGE = "homepage";
    public static final String PREF_UI_THEME = "ui_theme";
    public static final String PREF_PRIVACY = "privacy";
    public static final String PREF_SAFETY_CHECK = "safety_check";
    public static final String PREF_NOTIFICATIONS = "notifications";
    public static final String PREF_DOWNLOADS = "downloads";
    public static final String PREF_DEVELOPER = "developer";

    // Used for elevating the privacy section behind the flag (see crbug.com/1099233).
    public static final int PRIVACY_ORDER_DEFAULT = 18;
    public static final int PRIVACY_ORDER_ELEVATED = 12;

    private final ManagedPreferenceDelegate mManagedPreferenceDelegate;
    private final Map<String, Preference> mAllPreferences = new HashMap<>();
    private @Nullable PasswordCheck mPasswordCheck;

    public MainSettings() {
        setHasOptionsMenu(true);
        mManagedPreferenceDelegate = createManagedPreferenceDelegate();
    }

    @Override
    public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
        createPreferences();
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        mPasswordCheck = PasswordCheckFactory.getOrCreate(new SettingsLauncherImpl());
    }

    @Override
    public void onViewCreated(View view, Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);

        // Disable animations of preference changes.
        getListView().setItemAnimator(null);
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        // The component should only be destroyed when the activity has been closed by the user
        // (e.g. by pressing on the back button) and not when the activity is temporarily destroyed
        // by the system.
        if (getActivity().isFinishing() && mPasswordCheck != null) PasswordCheckFactory.destroy();
    }

    @Override
    public void onResume() {
        super.onResume();
        updatePreferences();
    }

    private void createPreferences() {
        SettingsUtils.addPreferencesFromResource(this, R.xml.main_preferences);

        // If the flag for adding a "Security" section is enabled, the "Privacy" section will be
        // renamed to a "Privacy and security" section and the "Security" section will be added
        // under the renamed section. See (go/esb-clank-dd) for more context.
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.SAFE_BROWSING_SECTION_UI)) {
            findPreference(PREF_PRIVACY).setTitle(R.string.prefs_privacy_security);
        }

        createBromitePreferences();

        cachePreferences();

        updatePasswordsPreference();

        setManagedPreferenceDelegateForPreference(PREF_SEARCH_ENGINE);

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            // If we are on Android O+ the Notifications preference should lead to the Android
            // Settings notifications page, not to Chrome's notifications settings page.
            Preference notifications = findPreference(PREF_NOTIFICATIONS);
            notifications.setOnPreferenceClickListener(preference -> {
                Intent intent = new Intent();
                intent.setAction(Settings.ACTION_APP_NOTIFICATION_SETTINGS);
                intent.putExtra(Settings.EXTRA_APP_PACKAGE,
                        ContextUtils.getApplicationContext().getPackageName());
                startActivity(intent);
                // We handle the click so the default action (opening NotificationsPreference)
                // isn't triggered.
                return true;
            });
        } else if (!PrefetchConfiguration.isPrefetchingFlagEnabled()) {
            // The Notifications Preferences page currently contains the Content Suggestions
            // Notifications setting (used only by the Offline Prefetch feature) and an entry to the
            // per-website notification settings page. The latter can be accessed from Site
            // Settings, so we only show the entry to the Notifications Preferences page if the
            // Prefetching feature flag is enabled.
            getPreferenceScreen().removePreference(findPreference(PREF_NOTIFICATIONS));
        }

        if (!TemplateUrlServiceFactory.get().isLoaded()) {
            TemplateUrlServiceFactory.get().registerLoadListener(this);
            TemplateUrlServiceFactory.get().load();
        }

        // Only show the Safety check section if the Safety check flag is on.
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.SAFETY_CHECK_ANDROID)) {
            getPreferenceScreen().removePreference(findPreference(PREF_SAFETY_CHECK));
        } else {
            findPreference(PREF_SAFETY_CHECK)
                    .setTitle(SafetyCheckSettingsFragment.getSafetyCheckSettingsElementTitle(
                            getContext()));
        }
    }

    /**
     * Stores all preferences in memory so that, if they needed to be added/removed from the
     * PreferenceScreen, there would be no need to reload them from 'main_preferences.xml'.
     */
    private void cachePreferences() {
        int preferenceCount = getPreferenceScreen().getPreferenceCount();
        for (int index = 0; index < preferenceCount; index++) {
            Preference preference = getPreferenceScreen().getPreference(index);
            mAllPreferences.put(preference.getKey(), preference);
        }
    }

    private void setManagedPreferenceDelegateForPreference(String key) {
        ChromeBasePreference chromeBasePreference = (ChromeBasePreference) mAllPreferences.get(key);
        chromeBasePreference.setManagedPreferenceDelegate(mManagedPreferenceDelegate);
    }

    private void updatePreferences() {
        updateSearchEnginePreference();

        Preference homepagePref = addPreferenceIfAbsent(PREF_HOMEPAGE);
        setOnOffSummary(homepagePref, HomepageManager.isHomepageEnabled());

        if (NightModeUtils.isNightModeSupported()) {
            addPreferenceIfAbsent(PREF_UI_THEME);
        } else {
            removePreferenceIfPresent(PREF_UI_THEME);
        }

        if (DeveloperSettings.shouldShowDeveloperSettings()) {
            addPreferenceIfAbsent(PREF_DEVELOPER);
        } else {
            removePreferenceIfPresent(PREF_DEVELOPER);
        }
    }

    private Preference addPreferenceIfAbsent(String key) {
        Preference preference = getPreferenceScreen().findPreference(key);
        if (preference == null) getPreferenceScreen().addPreference(mAllPreferences.get(key));
        return mAllPreferences.get(key);
    }

    private void removePreferenceIfPresent(String key) {
        Preference preference = getPreferenceScreen().findPreference(key);
        if (preference != null) getPreferenceScreen().removePreference(preference);
    }

    private void updateSearchEnginePreference() {
        if (!TemplateUrlServiceFactory.get().isLoaded()) {
            ChromeBasePreference searchEnginePref =
                    (ChromeBasePreference) findPreference(PREF_SEARCH_ENGINE);
            searchEnginePref.setEnabled(false);
            return;
        }

        String defaultSearchEngineName = null;
        TemplateUrl dseTemplateUrl =
                TemplateUrlServiceFactory.get().getDefaultSearchEngineTemplateUrl();
        if (dseTemplateUrl != null) defaultSearchEngineName = dseTemplateUrl.getShortName();

        Preference searchEnginePreference = findPreference(PREF_SEARCH_ENGINE);
        searchEnginePreference.setEnabled(true);
        searchEnginePreference.setSummary(defaultSearchEngineName);
    }

    private void updatePasswordsPreference() {
        Preference passwordsPreference = findPreference(PREF_PASSWORDS);
        passwordsPreference.setOnPreferenceClickListener(preference -> {
            PasswordManagerLauncher.showPasswordSettings(
                    getActivity(), ManagePasswordsReferrer.CHROME_SETTINGS);
            return true;
        });
    }

    private void setOnOffSummary(Preference pref, boolean isOn) {
        pref.setSummary(isOn ? R.string.text_on : R.string.text_off);
    }

    // TemplateUrlService.LoadListener implementation.
    @Override
    public void onTemplateUrlServiceLoaded() {
        TemplateUrlServiceFactory.get().unregisterLoadListener(this);
        updateSearchEnginePreference();
    }

    @VisibleForTesting
    public ManagedPreferenceDelegate getManagedPreferenceDelegateForTest() {
        return mManagedPreferenceDelegate;
    }

    private ManagedPreferenceDelegate createManagedPreferenceDelegate() {
        return new ChromeManagedPreferenceDelegate() {
            @Override
            public boolean isPreferenceControlledByPolicy(Preference preference) {
                if (PREF_SEARCH_ENGINE.equals(preference.getKey())) {
                    return TemplateUrlServiceFactory.get().isDefaultSearchManaged();
                }
                return false;
            }

            @Override
            public boolean isPreferenceClickDisabledByPolicy(Preference preference) {
                if (PREF_SEARCH_ENGINE.equals(preference.getKey())) {
                    return TemplateUrlServiceFactory.get().isDefaultSearchManaged();
                }
                return isPreferenceControlledByPolicy(preference)
                        || isPreferenceControlledByCustodian(preference);
            }
        };
    }

    private void createBromitePreferences() {
        SettingsUtils.inflatePrefsAddon(this, "main_preferences_addon_",
            R.xml.class.getFields());
    }
}
