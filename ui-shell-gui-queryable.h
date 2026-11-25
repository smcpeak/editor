// ui-shell-gui-queryable.h
// `UIShellGUIQueryable` class.

// See license.txt for copyright and terms of use.

#ifndef EDITOR_UI_SHELL_GUI_QUERYABLE_H
#define EDITOR_UI_SHELL_GUI_QUERYABLE_H

#include "ui-shell-gui-queryable-fwd.h"          // fwds for this module

// editor
#include "ui-shell-queryable.h"                  // UIShellQueryable

// smbase
#include "smbase/std-string-fwd.h"               // std::string [f]

class QImage;


// A `UIShellQueryable` with additional queries that involve GUI
// concepts, such as images.
class UIShellGUIQueryable : public UIShellQueryable {
public:      // funcs
  // Return an app-specific image, identified by `what`.  Corresponds
  // to the "CheckImage" command.
  //
  // The default throws `XMessage("unknown image: $what")`.
  virtual QImage uiShellImage(std::string const &what);
};


#endif // EDITOR_UI_SHELL_GUI_QUERYABLE_H
