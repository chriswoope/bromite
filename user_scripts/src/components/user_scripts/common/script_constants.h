// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef USERSCRIPTS_COMMON_SCRIPT_CONSTANTS_H_
#define USERSCRIPTS_COMMON_SCRIPT_CONSTANTS_H_

namespace user_scripts {

// Whether to fall back to matching the origin for frames where the URL
// cannot be matched directly, such as those with about: or data: schemes.
enum class MatchOriginAsFallbackBehavior {
  // Never fall back on the origin; this means scripts will never match on
  // these frames.
  kNever,
  // Match the origin only for about:-scheme frames, and then climb the frame
  // tree to find an appropriate ancestor to get a full URL (including path).
  // This is for supporting the "match_about_blank" key.
  // TODO(devlin): I wonder if we could simplify this to be "MatchForAbout",
  // and not worry about climbing the frame tree. It would be a behavior
  // change, but I wonder how many extensions it would impact in practice.
  kMatchForAboutSchemeAndClimbTree,
  // Match the origin as a fallback whenever applicable. This won't have a
  // corresponding path.
  kAlways,
};

// TODO(devlin): Move the other non-UserScript-specific constants like
// RunLocation and InjectionType from UserScript into here.

}

#endif  // USERSCRIPTS_COMMON_SCRIPT_CONSTANTS_H_
