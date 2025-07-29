// uri-util.h
// Utilities related to URIs.

// See license.txt for copyright and terms of use.

#ifndef EDITOR_URI_UTIL_H
#define EDITOR_URI_UTIL_H

#include "smbase/std-string-fwd.h"               // std::string
#include "smbase/std-string-view-fwd.h"          // std::string_view


// Given a file name, convert that into a "file:" URI.
std::string makeFileURI(std::string_view fname);

// Given a file URI, convert that back into a file name.  Throws
// `XParseString` if there is a problem.
std::string getFileURIPath(std::string const &uri);


#endif // EDITOR_URI_UTIL_H
