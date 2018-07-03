// keys-dialog.h
// Declare KeysDialog class.

#ifndef KEYS_DIALOG_H
#define KEYS_DIALOG_H

#include "modal-dialog.h"              // ModalDialog

#include <QString>

// Shows the current key bindings.
//
// In the future I plan to expand this so they are editable.
class KeysDialog : public ModalDialog {
public:      // funcs
  KeysDialog(QString keysText, QWidget *parent = NULL,
             Qt::WindowFlags f = Qt::WindowFlags());
  ~KeysDialog();
};


#endif // KEYS_DIALOG_H
