// uri-util.h
// Utilities related to URIs.

// See license.txt for copyright and terms of use.

#ifndef EDITOR_URI_UTIL_H
#define EDITOR_URI_UTIL_H

#include "smbase/std-string-fwd.h"               // std::string
#include "smbase/std-string-view-fwd.h"          // std::string_view


// Semantics of paths encoded as URIs.
enum class URIPathSemantics {
  // Path is treated normally.
  NORMAL,

  // The native path has to be turned into a cygwin path when stored in
  // a URI because the LSP server is a cygwin program (`pylsp`).  And, a
  // URI has to have the reverse transformation to yield a native path.
  CYGWIN
};


// Given a file name, convert that into a "file:" URI.
std::string makeFileURI(
  std::string_view fname, URIPathSemantics semantics);

// Given a file URI, convert that back into a file name.  Throws
// `smbase::XFormat` (smbase/exc.h) if there is a problem.
//
// The reason this one accepts a `string` instead of `string_view` is
// the implementation wants to use some `string-util` functions that, at
// present, only exist for the former, and it would be a little wasteful
// to allocate another string just for that.
std::string getFileURIPath(std::string const &uri,
  URIPathSemantics semantics);


#endif // EDITOR_URI_UTIL_H
