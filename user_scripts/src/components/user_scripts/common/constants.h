// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef USERSCRIPTS_COMMON_CONSTANTS_H_
#define USERSCRIPTS_COMMON_CONSTANTS_H_

#include "base/files/file_path.h"
#include "base/strings/string_piece_forward.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "components/version_info/channel.h"
#include "ui/base/layout.h"

namespace user_scripts {

// The origin of injected CSS.
enum CSSOrigin { /*CSS_ORIGIN_AUTHOR,*/ CSS_ORIGIN_USER };

}  // namespace user_scripts

#endif  // USERSCRIPTS_COMMON_CONSTANTS_H_
