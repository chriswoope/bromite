// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef USERSCRIPTS_RENDERER_INJECTION_HOST_H_
#define USERSCRIPTS_RENDERER_INJECTION_HOST_H_

#include "base/macros.h"
#include "../common/host_id.h"
#include "url/gurl.h"

namespace content {
class RenderFrame;
}

// An interface for all kinds of hosts who own user scripts.
class InjectionHost {
 public:
  InjectionHost(const HostID& host_id);
  virtual ~InjectionHost();

  // Returns the CSP to be used for the isolated world. Currently this only
  // bypasses the main world CSP. If null is returned, the main world CSP is not
  // bypassed.
  virtual const std::string* GetContentSecurityPolicy() const = 0;

  // The base url for the host.
  virtual const GURL& url() const = 0;

  // The human-readable name of the host.
  virtual const std::string& name() const = 0;

  const HostID& id() const { return id_; }

 private:
  // The ID of the host.
  HostID id_;

  DISALLOW_COPY_AND_ASSIGN(InjectionHost);
};

#endif  // USERSCRIPTS_RENDERER_INJECTION_HOST_H_
