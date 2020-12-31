// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef USERSCRIPTS_COMMON_ERROR_UTILS_H_
#define USERSCRIPTS_COMMON_ERROR_UTILS_H_

#include <string>

#include "base/strings/string16.h"
#include "base/strings/string_piece.h"

namespace user_scripts {

class ErrorUtils {
 public:
   // Creates an error messages from a pattern.
   static std::string FormatErrorMessage(base::StringPiece format,
                                         base::StringPiece s1);

};

}  // namespace extensions

#endif  // USERSCRIPTS_COMMON_ERROR_UTILS_H_
