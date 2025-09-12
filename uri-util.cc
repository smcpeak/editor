// uri-util.cc
// Code for `uri-util` module.

#include "uri-util.h"                  // this module

#include "smbase/codepoint.h"          // CodePoint
#include "smbase/exc.h"                // EXN_CONTEXT, smbase::xformat
#include "smbase/gdvalue.h"            // gdv::GDValue
#include "smbase/sm-env.h"             // smbase::envAsBool
#include "smbase/sm-file-util.h"       // SMFileUtil
#include "smbase/string-util.h"        // beginsWith, doubleQuote, hasSubstring
#include "smbase/stringb.h"            // stringb

#include <iomanip>                     // std::{setfill, setw}
#include <sstream>                     // std::ostringstream
#include <string>                      // std::string
#include <string_view>                 // std::string_view

using namespace gdv;
using namespace smbase;


gdv::GDValue toGDValue(URIPathSemantics semantics)
{
  if (semantics == URIPathSemantics::NORMAL) {
    return "NORMAL"_sym;
  }
  else {
    return "CYGWIN"_sym;
  }
}


// Return true if `pt` can be used as-is in a URI.
//
// Whether a character is safe is context-dependent.  I'm just following
// what clangd does for file names; this is probably not right.
static bool isSafeInURI(CodePoint pt)
{
  return isCIdentifierCharacter(pt) ||
         pt.value() == ':' ||
         pt.value() == '/' ||
         pt.value() == '.';
}


std::string percentEncode(std::string_view src)
{
  std::ostringstream oss;

  // Configure for hex.
  oss << std::uppercase << std::hex << std::setfill('0');

  for (char c : src) {
    CodePoint pt(c);

    if (isSafeInURI(pt)) {
      oss << c;
    }
    else {
      oss << '%' << std::setw(2) << pt.value();
    }
  }

  return oss.str();
}


std::string percentDecode(std::string_view src)
{
  std::ostringstream oss;

  for (auto it = src.begin(); it != src.end(); ++it) {
    if (*it == '%') {
      // Decode first digit.
      ++it;
      if (it == src.end()) {
        xformat("percent not followed by anything");
      }
      CodePoint pt(*it);
      if (!isASCIIHexDigit(pt)) {
        xformat("percent followed by non-hex");
      }
      int n = decodeASCIIHexDigit(pt) * 16;

      // Decode second digit.
      ++it;
      if (it == src.end()) {
        xformat("percent only followed by one hex digit");
      }
      pt = *it;
      if (!isASCIIHexDigit(pt)) {
        xformat("percent followed by hex then non-hex");
      }
      n += decodeASCIIHexDigit(pt);

      oss << (char)(unsigned char)n;
    }
    else {
      oss << *it;
    }
  }

  return oss.str();
}


// TODO: Move someplace more general.
char tolower_char(char c)
{
  if ('A' <= c && c <= 'Z') {
    return c + ('a' - 'A');
  }
  else {
    return c;
  }
}


char toupper_char(char c)
{
  if ('a' <= c && c <= 'z') {
    return c - ('a' - 'A');
  }
  else {
    return c;
  }
}


static std::string windowsToCygwin(std::string_view fname)
{
  if (fname.size() >= 3 &&
      fname[1] == ':' &&
      fname[2] == '/') {
    // "C:/Windows" -> "/cygdrive/c/Windows"
    return stringb(
      "/cygdrive/" << tolower_char(fname[0]) << fname.substr(2));
  }
  else {
    return std::string(fname);
  }
}


static std::string cygwinToWindows(std::string_view fname)
{
  if (fname.size() >= 12 &&
      fname.substr(0, 10) == "/cygdrive/" &&
      fname[11] == '/') {
    // "/cygdrive/c/Windows" -> "C:/Windows"
    return stringb(
      toupper_char(fname[10]) << ":" << fname.substr(11));
  }
  else {
    return std::string(fname);
  }
}


std::string makeFileURI(
  std::string_view fname, URIPathSemantics semantics)
{
  SMFileUtil sfu;

  std::string absFname = sfu.getAbsolutePath(std::string(fname));

  absFname = sfu.normalizePathSeparators(absFname);

  if (semantics == URIPathSemantics::CYGWIN) {
    absFname = windowsToCygwin(absFname);
  }

  // In the URI format, a path like "C:/Windows" gets written
  // "/C:/Windows".
  if (!beginsWith(absFname, "/")) {
    absFname = std::string("/") + absFname;
  }

  return stringb("file://" << percentEncode(absFname));
}


// For now this is very crude, doing just enough to invert the encodings
// I see from `clangd`.
std::string getFileURIPath(
  std::string const &uri, URIPathSemantics semantics)
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
  if (hasSubstring(path, "?")) {
    xformat("URI has a query part but I can't handle that.");
  }

  // The path should always be absolute.
  if (!beginsWith(path, "/")) {
    xformat("Path does not begin with '/'.");
  }

  if (semantics == URIPathSemantics::CYGWIN) {
    path = cygwinToWindows(path);
  }

  // Check for Windows path stuff.
  if (path.size() >= 4 &&
      path[2] == ':' &&
      path[3] == '/')
  {
    // Path is something like "/C:/blah".  We want to discard the first
    // slash since Windows won't like it.
    return percentDecode(std::string_view(path).substr(1));
  }
  else {
    return percentDecode(path);
  }
}


// EOF
