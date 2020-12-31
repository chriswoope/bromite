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

#ifndef COMPONENTS_USERSCRIPS_HELPER_ANDROID_H_
#define COMPONENTS_USERSCRIPS_HELPER_ANDROID_H_

#include <jni.h>

namespace user_scripts {

static void ShouldRefreshUserScriptList(JNIEnv* env);
static void OnUserScriptLoaded(JNIEnv* env,
              bool result, const std::string& error);

}  // namespace user_scripts

#endif