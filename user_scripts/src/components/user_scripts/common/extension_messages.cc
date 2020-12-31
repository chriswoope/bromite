// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extension_messages.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "content/public/common/common_param_traits.h"

namespace IPC {

void ParamTraits<HostID>::Write(base::Pickle* m, const param_type& p) {
  WriteParam(m, p.type());
  WriteParam(m, p.id());
}

bool ParamTraits<HostID>::Read(const base::Pickle* m,
                               base::PickleIterator* iter,
                               param_type* r) {
  HostID::HostType type;
  std::string id;
  if (!ReadParam(m, iter, &type))
    return false;
  if (!ReadParam(m, iter, &id))
    return false;
  *r = HostID(type, id);
  return true;
}

void ParamTraits<HostID>::Log(
    const param_type& p, std::string* l) {
  LogParam(p.type(), l);
  LogParam(p.id(), l);
}

}  // namespace IPC
