// ui-shell-queryable.h
// `UIShellQueryable` abstract class.

// See license.txt for copyright and terms of use.

#ifndef EDITOR_UI_SHELL_QUERYABLE_H
#define EDITOR_UI_SHELL_QUERYABLE_H

#include "ui-shell-queryable-fwd.h"    // fwds for this module

// editor
#include "ui-shell-result-fwd.h"       // UIShellResult [f]

// smbase
#include "smbase/gdvalue-fwd.h"        // gdv::GDValue [f]
#include "smbase/std-string-fwd.h"     // std::string [f]

class QImage;


// Interface that the UI shell uses to query arbitrary state
// elements of UI objects.
//
// This is also used during recording to do some filtering based on
// `wantResizeEventsRecorded`.
//
class UIShellQueryable {
public:      // funcs
  /*
    Return some application-specific state.  Typically, the returned
    value is a string or an integer, but it can be any GDValue.

    One use is in `CheckQuery` UI shell commands, in which case the
    returned value must match the `CheckQuery` argument.

    Another use is as part of the UI shell server (doc/ui-shell.txt),
    where the value goes back to the shell client for intropspection
    purposes.

    This method conveys an error by throwing an exception.

    The default behavior throws `XMessage("unknown state: $state")`.
  */
  virtual gdv::GDValue uiShellQuery(std::string const &state);

  // Calls `uiShellQuery` and wraps the result as a `UIShellResult`.
  UIShellResult tryUIShellQuery(std::string const &state);

  // Return an app-specific image, identified by 'what'.  Corresponds to
  // the "CheckQuery" command.  The default implementation returns a
  // null QImage.
  virtual QImage uiShellImage(std::string const &what);

  // Return true if resize events should be recorded for this object.
  // Doing so facilitates writing tests for resizing.  The default is
  // false.
  virtual bool wantResizeEventsRecorded();
};


#endif // EDITOR_UI_SHELL_QUERYABLE_H
