// modal-dialog.h
// Declare ModalDialog class.

#ifndef MODAL_DIALOG_H
#define MODAL_DIALOG_H

#include <QDialog>

class QBoxLayout;

// This is a base class containing some common functionality for my
// modal dialogs.
class ModalDialog : public QDialog {
  Q_OBJECT

protected:   // funcs
  // Create the standard Ok and Cancel buttons and add them to 'layout'.
  void createOkAndCancelHBox(QBoxLayout *layout);

public:      // funcs
  ModalDialog(QWidget *parent = NULL, Qt::WindowFlags f = Qt::WindowFlags());
  ~ModalDialog();
};

#endif // MODAL_DIALOG_H
