// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.user_scripts.UserScriptsUtils;

/**
 * JNI call glue for AfterStartupTaskUtils in C++.
 */
public final class AfterStartupTaskUtils {
    private AfterStartupTaskUtils() {}

    /**
     * Informs the C++ side that startup is complete. Tasks that
     * have been deferred until after startup will be scheduled
     * to run and newly posted tasks will no longer be deferred.
     */
    public static void setStartupComplete() {
        UserScriptsUtils.Initialize();
        AfterStartupTaskUtilsJni.get().setStartupComplete();
    }

    @NativeMethods
    interface Natives {
        void setStartupComplete();
    }
}
