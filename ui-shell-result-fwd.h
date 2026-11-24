// ui-shell-result-fwd.h
// Forward decls for `ui-shell-result.h`.

// See license.txt for copyright and terms of use.

// This file actually has the entire definition, albeit on top of
// forwards for the constituent pieces.

#ifndef EDITOR_UI_SHELL_RESULT_FWD_H
#define EDITOR_UI_SHELL_RESULT_FWD_H

// smbase
#include "smbase/either-fwd.h"         // smbase::Either [f]
#include "smbase/gdvalue-fwd.h"        // gdv::GDValue [f]
#include "smbase/std-string-fwd.h"     // std::string [f]


/*
  Either a GDValue representing the result of a successful command or
  query, or a string carrying an error message.
*/
using UIShellResult = smbase::Either<gdv::GDValue, std::string>;


#endif // EDITOR_UI_SHELL_RESULT_FWD_H
