// uri-util.cc
// Code for `uri-util` module.

#include "uri-util.h"                  // this module

#include "smbase/exc.h"                // EXN_CONTEXT, smbase::xformat
#include "smbase/sm-file-util.h"       // SMFileUtil
#include "smbase/string-util.h"        // beginsWith, doubleQuote, hasSubstring
#include "smbase/stringb.h"            // stringb

#include <string>                      // std::string
#include <string_view>                 // std::string_view

using namespace smbase;


std::string makeFileURI(std::string_view fname)
{
  SMFileUtil sfu;

  std::string absFname = sfu.getAbsolutePath(std::string(fname));

  absFname = sfu.normalizePathSeparators(absFname);

  // In the URI format, a path like "C:/Windows" gets written
  // "/C:/Windows".
  if (!beginsWith(absFname, "/")) {
    absFname = std::string("/") + absFname;
  }

  return stringb("file://" << absFname);
}


// For now this is very crude, doing just enough to invert the encodings
// I see from `clangd`.
std::string getFileURIPath(std::string const &uri)
{
  EXN_CONTEXT(doubleQuote(uri));

  if (hasSubstring(uri, "@")) {
    xformat("URI has a user name part but I can't handle that.");
  }

  if (!beginsWith(uri, "file://")) {
    xformat("URI does not begin with \"file://\".");
  }

  // Skip the scheme part.
  std::string path = uri.substr(7);

  // Check for some things that can be in URIs but I don't handle.
  if (hasSubstring(path, "%")) {
    xformat("URI uses percent encoding but I can't handle that.");
  }
  if (hasSubstring(path, "?")) {
    xformat("URI has a query part but I can't handle that.");
  }

  // The path should always be absolute.
  if (!beginsWith(path, "/")) {
    xformat("Path does not begin with '/'.");
  }

  // Check for Windows path stuff.
  if (path.size() >= 4 &&
      path[2] == ':' &&
      path[3] == '/')
  {
    // Path is something like "/C:/blah".  We want to discard the first
    // slash since Windows won't like it.
    return path.substr(1);
  }
  else {
    return path;
  }
}


// EOF
