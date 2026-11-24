// ui-shell-queryable.h
// `UIShellQueryable` abstract class.

// See license.txt for copyright and terms of use.

#ifndef EDITOR_UI_SHELL_QUERYABLE_H
#define EDITOR_UI_SHELL_QUERYABLE_H

#include "ui-shell-queryable-fwd.h"    // fwds for this module

// smbase
#include "smbase/gdvalue-fwd.h"        // gdv::GDValue [f]
#include "smbase/std-string-fwd.h"     // std::string [f]

class QImage;


// Interface that the UI shell uses to query arbitrary state
// elements of objects.
//
// This is also used during recording to do some filtering.
class UIShellQueryable {
public:      // funcs
  /*
    Returns the tagged tuple:

      Error("$msg")

    A tagged `Error` tuple with a single string element containing a
    human-readable error message is the usual way to convey errors.
  */
  static gdv::GDValue errorTuple(std::string const &msg);

  /*
    Return some application-specific state.  This result must match the
    argument to the "CheckQuery" command in test scripts for the test
    to pass.  Typically, the returned value is a string or an integer,
    but it can be any GDValue.

    If it throws an exception, that will normally be converted into an
    `Error` tagged tuple by `tryUIShellQuery`.

    The default calls `errorTuple` with "unknown state: $state".
  */
  virtual gdv::GDValue uiShellQuery(std::string const &state);

  // Calls `uiShellQuery`.  If it throws, its message is passed to
  // `errorTuple` and that returned.
  gdv::GDValue tryUIShellQuery(std::string const &state);

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
