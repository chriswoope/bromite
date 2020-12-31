// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// IPC messages for extensions.

#ifndef EXTENSIONS_COMMON_EXTENSION_MESSAGES_H_
#define EXTENSIONS_COMMON_EXTENSION_MESSAGES_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/values.h"
#include "content/public/common/common_param_traits.h"
#include "constants.h"
#include "host_id.h"
#include "ipc/ipc_message_macros.h"
#include "url/gurl.h"
#include "url/origin.h"

#define IPC_MESSAGE_START ExtensionMsgStart

IPC_ENUM_TRAITS_MAX_VALUE(HostID::HostType, HostID::HOST_TYPE_LAST)

// Singly-included section for custom IPC traits.
#ifndef INTERNAL_EXTENSIONS_COMMON_EXTENSION_MESSAGES_H_
#define INTERNAL_EXTENSIONS_COMMON_EXTENSION_MESSAGES_H_

namespace IPC {

template <>
struct ParamTraits<HostID> {
  typedef HostID param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};


}  // namespace IPC

#endif  // INTERNAL_EXTENSIONS_COMMON_EXTENSION_MESSAGES_H_

// Notification that the user scripts have been updated. It has one
// ReadOnlySharedMemoryRegion argument consisting of the pickled script data.
// This memory region is valid in the context of the renderer.
// If |owner| is not empty, then the shared memory handle refers to |owner|'s
// programmatically-defined scripts. Otherwise, the handle refers to all
// hosts' statically defined scripts. So far, only extension-hosts support
// statically defined scripts; WebUI-hosts don't.
// If |changed_hosts| is not empty, only the host in that set will
// be updated. Otherwise, all hosts that have scripts in the shared memory
// region will be updated. Note that the empty set => all hosts case is not
// supported for per-extension programmatically-defined script regions; in such
// regions, the owner is expected to list itself as the only changed host.
// If |whitelisted_only| is true, this process should only run whitelisted
// scripts and not all user scripts.
IPC_MESSAGE_CONTROL1(ExtensionMsg_UpdateUserScripts,
                     base::ReadOnlySharedMemoryRegion)

#endif  // EXTENSIONS_COMMON_EXTENSION_MESSAGES_H_
