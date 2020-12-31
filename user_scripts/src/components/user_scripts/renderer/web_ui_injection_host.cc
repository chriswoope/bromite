// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "web_ui_injection_host.h"
#include "base/no_destructor.h"

namespace {

// The default secure CSP to be used in order to prevent remote scripts.
const char kDefaultSecureCSP[] = "script-src 'self'; object-src 'self';";

}

WebUIInjectionHost::WebUIInjectionHost(const HostID& host_id)
  : InjectionHost(host_id),
    url_(host_id.id()) {
}

WebUIInjectionHost::~WebUIInjectionHost() {
}

const std::string* WebUIInjectionHost::GetContentSecurityPolicy() const {
  // Use the main world CSP.
  // return nullptr;

  // The isolated world will use its own CSP which blocks remotely hosted
  // code.
  static const base::NoDestructor<std::string> default_isolated_world_csp(
      kDefaultSecureCSP);
  return default_isolated_world_csp.get();
}

const GURL& WebUIInjectionHost::url() const {
  return url_;
}

const std::string& WebUIInjectionHost::name() const {
  return id().id();
}
