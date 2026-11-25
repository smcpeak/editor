// ui-shell-gui-command-handler.h
// `UIShellGUICommandHandler`, part of the UI shell.  See doc/ui-shell.txt.

// See license.txt for copyright and terms of use.

#ifndef EDITOR_UI_SHELL_GUI_COMMAND_HANDLER_H
#define EDITOR_UI_SHELL_GUI_COMMAND_HANDLER_H

// editor
#include "ui-shell-command-handler.h"  // UIShellCommandHandler
#include "ui-shell-queryable-fwd.h"    // UIShellQueryable [f]

// smbase
#include "smbase/array.h"              // ArrayStack
#include "smbase/gdvalue-fwd.h"        // gdv::GDValue [f]
#include "smbase/gdvalue-parser-fwd.h" // gdv::GDValueParser [f]
#include "smbase/sm-macros.h"          // NO_OBJECT_COPIES
#include "smbase/sm-override.h"        // OVERRIDE

// Qt
#include <QObject>
#include <QString>

// libc++
#include <string>                      // std::string

class QAbstractButton;
class QSize;
class QWidget;


// Executes the UI shell commands that synthesize input events for, or
// inspect the state of, the application's widgets.
class UIShellGUICommandHandler : public QObject,
                                 public UIShellCommandHandler {
  Q_OBJECT
  NO_OBJECT_COPIES(UIShellGUICommandHandler);

private:     // instance data
  // Queue of ordinary typing characters to execute before the current
  // command is considered finished.  These are the pending steps of a
  // FocusKeySequence command.
  ArrayStack<char> m_queuedFocusKeySequence;

private:     // funcs
  // Change the size of the top-level window containing 'widget' so that
  // the widget itself reaches the target size.  This assumes that the
  // widget expands or contracts the same amount as its parent windows.
  void resizeChildWidget(QWidget *widget, QSize const &targetSize);

  // Get the focus widget, throwing if there is none.  `funcName` is
  // the function we are performing, and used in a `TRACE` call.
  QWidget *getFocusWidget(QString const &funcName);

  // Do an extra sleep on Linux as a workaround for a bug.  See comments
  // on implementation.
  void checkFocusWorkaround();

  // Like `button->click()`, but posting the event rather than sending
  // it for immediate execution.  This ensures we don't get trapped in
  // a new event loop if the button, say, pops up a modal dialog.
  void postClickButton(QAbstractButton *button);

  // Execute a "Shortcut" event.
  void executeShortcut(
    std::string const &receiverPath,
    std::string const &keys);

  // Execute an ordinary typing event.
  void executeFocusKey(char c);

protected:   // funcs
  // UIShellCommandHandler methods.
  virtual gdv::GDValue executeDerivedCommand(
    std::string const &funcNameString,
    gdv::GDValueParser const &parser) OVERRIDE;

public:      // funcs
  UIShellGUICommandHandler();
  ~UIShellGUICommandHandler();

  // UIShellCommandHandler methods.
  virtual UIShellQueryable *getQueryableFromPath(
    std::string const &path) OVERRIDE;
  virtual bool hasPendingStep() const OVERRIDE;
  virtual void executePendingStep() OVERRIDE;
  virtual void discardPendingSteps() OVERRIDE;

Q_SIGNALS:
  // Internal: temporarily connected to a button's slot to enqueue a
  // click (ClickButton).  It is public only because Qt makes all
  // signals public.
  void signal_clickButton();
};


#endif // EDITOR_UI_SHELL_GUI_COMMAND_HANDLER_H
