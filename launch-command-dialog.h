// launch-command-dialog.h
// LaunchCommandDialog class.

#ifndef LAUNCH_COMMAND_DIALOG_H
#define LAUNCH_COMMAND_DIALOG_H

#include "launch-command-dialog-fwd.h" // fwds for this module

#include "textinput.h"                 // TextInputDialog


class QCheckBox;


// Dialog to prompt user for parameters to use to launch a command
// whose output will appear in an editor document.
class LaunchCommandDialog : public TextInputDialog {
private:      // data
  // Checkbox to enable command line substitutions.
  QCheckBox *m_enableSubstitutionCheckbox;

  // Checkbox: 'Prefix stderr output with "STDERR: "'.  This is null if
  // `withPrefixCheckbox` was false in the constructor call.
  QCheckBox * NULLABLE m_prefixStderrLines;

public:       // funcs
  // Create a dialog with the given title.  If `withPrefixCheckbox` is
  // true, then this acts as the prompt dialog for Alt+R (Run command).
  // Otherwise it acts as the prompt dialog for Alt+A (Apply command).
  LaunchCommandDialog(
    QString title,
    bool withPrefixCheckbox,
    QWidget *parent = NULL,
    Qt::WindowFlags f = Qt::WindowFlags());

  ~LaunchCommandDialog();

  // After the dialog runs, get the checkbox values.
  bool enableSubstitution() const;
  bool prefixStderrLines() const;
};


#endif // LAUNCH_COMMAND_DIALOG_H
