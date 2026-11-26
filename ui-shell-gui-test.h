// ui-shell-gui-test.h
// Classes for `ui-shell-gui-test`, a program for testing the GUI UI
// shell commands.  See doc/plans/claude-ui-shell-gui-test.txt.

// See license.txt for copyright and terms of use.

#ifndef EDITOR_UI_SHELL_GUI_TEST_H
#define EDITOR_UI_SHELL_GUI_TEST_H

// editor
#include "ui-shell-gui-queryable.h"              // UIShellGUIQueryable

// smbase
#include "smbase/gdvalue-fwd.h"                  // gdv::GDValue [f]
#include "smbase/sm-macros.h"                    // NO_OBJECT_COPIES
#include "smbase/sm-noexcept.h"                  // NOEXCEPT
#include "smbase/sm-override.h"                  // OVERRIDE
#include "smbase/std-string-fwd.h"               // std::string [f]

// Qt
#include <QMainWindow>

class QLabel;
class QPushButton;
class QString;
class QWidget;


// The main window of the test program.
class UISGTMainWindow : public QMainWindow,
                        public UIShellGUIQueryable {
  Q_OBJECT
  NO_OBJECT_COPIES(UISGTMainWindow);

public:      // data
  // Number of times `m_clickButton` has been clicked.
  int m_clickCount;

  // Widgets.  They are owned by their Qt parents.
  QWidget *m_central;
  QPushButton *m_clickButton;
  QLabel *m_clickCountLabel;

private:     // funcs
  // Text that `m_clickCountLabel` should have.
  QString clickCountLabelText() const;

public:      // funcs
  UISGTMainWindow();
  ~UISGTMainWindow();

  // Check that the labels agree with the state they display.  This is
  // the global self-check for the UI shell.
  void selfCheck() const;

  // UIShellQueryable methods.
  virtual gdv::GDValue uiShellQuery(std::string const &state) OVERRIDE;

public Q_SLOTS:
  void on_clickButtonClicked() NOEXCEPT;
};


#endif // EDITOR_UI_SHELL_GUI_TEST_H
