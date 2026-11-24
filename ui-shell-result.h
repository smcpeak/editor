// ui-shell-result.h
// `UIShellResult` class.

// See license.txt for copyright and terms of use.

#ifndef EDITOR_UI_SHELL_RESULT_H
#define EDITOR_UI_SHELL_RESULT_H

#include "ui-shell-result-fwd.h"       // fwds for this module

// smbase
#include "smbase/either.h"             // smbase::Either
#include "smbase/gdvalue.h"            // gdv::GDValue

// libc++
#include <string>                      // std::string


// The "real" definition is in ui-shell-result-fwd.h, but repeated here
// for convenience.  It's uncommented so the compiler enforces that they
// stay consistent.  Clients are supposed to include this file to get a
// fully defined type, and the -fwd file for an incomplete type.

/*
  Either a GDValue representing the result of a successful command or
  query, or a string carrying an error message.
*/
using UIShellResult = smbase::Either<gdv::GDValue, std::string>;


gdv::GDValue toGDValue(UIShellResult const &res);


#endif // EDITOR_UI_SHELL_RESULT_H
