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

  Note: `UIShellResult(std::string)` compiles, and yields a *right*
  (error) object, even though there is a `GDValue` constructor that
  accepts a string.  Use the `makeLeft` and `makeRight` static methods
  on `Either` for maximum clarity.

  Note on terminology: A UI shell command can complete successfully or
  have an error.  Inside the UI shell module, we call the disjoint union
  of these two a "result".  When that information is transmitted over
  the UI shell protocol, it is called a "reply".
*/
using UIShellResult = smbase::Either<gdv::GDValue, std::string>;


gdv::GDValue toGDValue(UIShellResult const &res);


#endif // EDITOR_UI_SHELL_RESULT_H
