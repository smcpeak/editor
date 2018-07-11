// filename-input.h
// FilenameInputDialog

#ifndef FILENAME_INPUT_H
#define FILENAME_INPUT_H

#include "modal-dialog.h"              // ModalDialog

class QLineEdit;


// Prompt for a file name.
//
// My immediate aim is simply to confirm the heuristic selection of a
// file name using the openFileAtCursor feature, but I could see this
// growing into a full-fledged file browser optimized for keyboard
// navigation.
class FilenameInputDialog : public ModalDialog {
  Q_OBJECT

private:     // data
  // Control with the current file name (full path) in it.  Owned by
  // 'this', but deallocated automatically by QObject infrastructure.
  QLineEdit *m_filenameEdit;

public:      // funcs
  FilenameInputDialog(QWidget *parent = NULL, Qt::WindowFlags f = Qt::WindowFlags());
  ~FilenameInputDialog();

  // Show the dialog and return the name of the file the user has
  // chosen, or "" if canceled.
  QString runDialog(QString initialChoice);
};


#endif // FILENAME_INPUT_H
