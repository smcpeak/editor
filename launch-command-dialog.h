// launch-command-dialog.h
// LaunchCommandDialog class.

#ifndef LAUNCH_COMMAND_DIALOG_H
#define LAUNCH_COMMAND_DIALOG_H

#include "textinput.h"                 // TextInputDialog


class QCheckBox;


// Dialog to prompt user for parameters to use to launch a command
// whose output will appear in an editor document.
class LaunchCommandDialog : public TextInputDialog {
private:      // data
  // Checkbox: 'Prefix stderr output with "STDERR: ".'
  QCheckBox *m_prefixStderrLines;

public:       // funcs
  LaunchCommandDialog(QWidget *parent = NULL, Qt::WindowFlags f = Qt::WindowFlags());
  ~LaunchCommandDialog();

  // After the dialog runs, get the value of 'm_prefixStderrLines'.
  bool prefixStderrLines() const;
};


#endif // LAUNCH_COMMAND_DIALOG_H
