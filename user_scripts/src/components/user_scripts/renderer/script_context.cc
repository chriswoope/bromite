// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "script_context.h"

#include "base/command_line.h"
#include "base/containers/flat_set.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/renderer/render_frame.h"
#include "../common/constants.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration.mojom.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_document_loader.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_navigation_params.h"
#include "v8/include/v8.h"

namespace user_scripts {

namespace {

GURL GetEffectiveDocumentURL(
    blink::WebLocalFrame* frame,
    const GURL& document_url,
    MatchOriginAsFallbackBehavior match_origin_as_fallback,
    bool allow_inaccessible_parents) {
  auto should_consider_origin = [document_url, match_origin_as_fallback]() {
    switch (match_origin_as_fallback) {
      case MatchOriginAsFallbackBehavior::kNever:
        return false;
      case MatchOriginAsFallbackBehavior::kMatchForAboutSchemeAndClimbTree:
        return document_url.SchemeIs(url::kAboutScheme);
      case MatchOriginAsFallbackBehavior::kAlways:
        // TODO(devlin): Add more schemes here - blob, filesystem, etc.
        return document_url.SchemeIs(url::kAboutScheme) ||
               document_url.SchemeIs(url::kDataScheme);
    }

    NOTREACHED();
  };

  // If we don't need to consider the origin, we're done.
  if (!should_consider_origin())
    return document_url;

  // Get the "security origin" for the frame. For about: frames, this is the
  // origin of that of the controlling frame - e.g., an about:blank frame on
  // https://example.com will have the security origin of https://example.com.
  // Other frames, like data: frames, will have an opaque origin. For these,
  // we can get the precursor origin.
  const blink::WebSecurityOrigin web_frame_origin = frame->GetSecurityOrigin();
  const url::Origin frame_origin = web_frame_origin;
  const url::SchemeHostPort& tuple_or_precursor_tuple =
      frame_origin.GetTupleOrPrecursorTupleIfOpaque();

  // When there's no valid tuple (which can happen in the case of e.g. a
  // browser-initiated navigation to an opaque URL), there's no origin to
  // fallback to. Bail.
  if (!tuple_or_precursor_tuple.IsValid())
    return document_url;

  const url::Origin origin_or_precursor_origin =
      url::Origin::Create(tuple_or_precursor_tuple.GetURL());

  if (!allow_inaccessible_parents &&
      !web_frame_origin.CanAccess(
          blink::WebSecurityOrigin(origin_or_precursor_origin))) {
    // The frame can't access its precursor. Bail.
    return document_url;
  }

  // Looks like the initiator origin is an appropriate fallback!

  if (match_origin_as_fallback == MatchOriginAsFallbackBehavior::kAlways) {
    // The easy case! We use the origin directly. We're done.
    return origin_or_precursor_origin.GetURL();
  }

  DCHECK_EQ(MatchOriginAsFallbackBehavior::kMatchForAboutSchemeAndClimbTree,
            match_origin_as_fallback);

  // Unfortunately, in this case, we have to climb the frame tree. This is for
  // match patterns that are associated with paths as well, not just origins.
  // For instance, if an extension wants to run on google.com/maps/* with
  // match_about_blank true, then it should run on about:-scheme frames created
  // by google.com/maps, but not about:-scheme frames created by google.com
  // (which is what the precursor tuple origin would be).

  // Traverse the frame/window hierarchy to find the closest non-about:-page
  // with the same origin as the precursor and return its URL.
  // Note: This can return the incorrect result, e.g. if a parent frame
  // navigates a grandchild frame.
  blink::WebFrame* parent = frame;
  GURL parent_url;
  blink::WebDocument parent_document;
  base::flat_set<blink::WebFrame*> already_visited_frames;
  do {
    already_visited_frames.insert(parent);
    if (parent->Parent())
      parent = parent->Parent();
    else
      parent = parent->Opener();

    // Avoid an infinite loop - see https://crbug.com/568432 and
    // https://crbug.com/883526.
    if (base::Contains(already_visited_frames, parent))
      return document_url;

    parent_document = parent && parent->IsWebLocalFrame()
                          ? parent->ToWebLocalFrame()->GetDocument()
                          : blink::WebDocument();

    // We reached the end of the ancestral chain without finding a valid parent,
    // or found a remote web frame (in which case, it's a different origin).
    // Bail and use the original URL.
    if (parent_document.IsNull())
      return document_url;

    url::SchemeHostPort parent_tuple_or_precursor_tuple =
        url::Origin(parent->GetSecurityOrigin())
            .GetTupleOrPrecursorTupleIfOpaque();
    if (!parent_tuple_or_precursor_tuple.IsValid() ||
        parent_tuple_or_precursor_tuple != tuple_or_precursor_tuple) {
      // The parent has a different tuple origin than frame; this could happen
      // in edge cases where a parent navigates an iframe or popup of a child
      // frame at a different origin. [1] In this case, bail, since we can't
      // find a full URL (i.e., one including the path) with the same security
      // origin to use for the frame in question.
      // [1] Consider a frame tree like:
      // <html> <!--example.com-->
      //   <iframe id="a" src="a.com">
      //     <iframe id="b" src="b.com"></iframe>
      //   </iframe>
      // </html>
      // Frame "a" is cross-origin from the top-level frame, and so the
      // example.com top-level frame can't directly access frame "b". However,
      // it can navigate it through
      // window.frames[0].frames[0].location.href = 'about:blank';
      // In that case, the precursor origin tuple origin of frame "b" would be
      // example.com, but the parent tuple origin is a.com.
      // Note that usually, this would have bailed earlier with a remote frame,
      // but it may not if we're at the process limit.
      return document_url;
    }

    parent_url = GURL(parent_document.Url());
  } while (parent_url.SchemeIs(url::kAboutScheme));

  DCHECK(!parent_url.is_empty());
  DCHECK(!parent_document.IsNull());

  // We should know that the frame can access the parent document (unless we
  // explicitly allow it not to), since it has the same tuple origin as the
  // frame, and we checked the frame access above.
  DCHECK(allow_inaccessible_parents ||
         web_frame_origin.CanAccess(parent_document.GetSecurityOrigin()));
  return parent_url;
}

using FrameToDocumentLoader =
    base::flat_map<blink::WebLocalFrame*, blink::WebDocumentLoader*>;

FrameToDocumentLoader& FrameDocumentLoaderMap() {
  static base::NoDestructor<FrameToDocumentLoader> map;
  return *map;
}

blink::WebDocumentLoader* CurrentDocumentLoader(
    const blink::WebLocalFrame* frame) {
  auto& map = FrameDocumentLoaderMap();
  auto it = map.find(frame);
  return it == map.end() ? frame->GetDocumentLoader() : it->second;
}

}  // namespace

// static
GURL ScriptContext::GetDocumentLoaderURLForFrame(
    const blink::WebLocalFrame* frame) {
  // Normally we would use frame->document().url() to determine the document's
  // URL, but to decide whether to inject a content script, we use the URL from
  // the data source. This "quirk" helps prevents content scripts from
  // inadvertently adding DOM elements to the compose iframe in Gmail because
  // the compose iframe's dataSource URL is about:blank, but the document URL
  // changes to match the parent document after Gmail document.writes into
  // it to create the editor.
  // http://code.google.com/p/chromium/issues/detail?id=86742
  blink::WebDocumentLoader* document_loader = CurrentDocumentLoader(frame);
  return document_loader ? GURL(document_loader->GetUrl()) : GURL();
}

// static
GURL ScriptContext::GetEffectiveDocumentURLForInjection(
    blink::WebLocalFrame* frame,
    const GURL& document_url,
    MatchOriginAsFallbackBehavior match_origin_as_fallback) {
  // We explicitly allow inaccessible parents here. Extensions should still be
  // able to inject into a sandboxed iframe if it has access to the embedding
  // origin.
  constexpr bool allow_inaccessible_parents = true;
  return GetEffectiveDocumentURL(frame, document_url, match_origin_as_fallback,
                                 allow_inaccessible_parents);
}

}  // namespace extensions
