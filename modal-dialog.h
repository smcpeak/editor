// modal-dialog.h
// Declare ModalDialog class.

#ifndef MODAL_DIALOG_H
#define MODAL_DIALOG_H

#include <QDialog>

class QBoxLayout;
class QPushButton;

// This is a base class containing some common functionality for my
// modal dialogs.
class ModalDialog : public QDialog {
  Q_OBJECT

protected:   // data
  // The "Ok" and "Cancel" buttons.
  QPushButton *m_okButton;
  QPushButton *m_cancelButton;

protected:   // funcs
  // Create the standard Ok and Cancel buttons in an hbox and add them
  // to 'vbox'.
  void createOkAndCancelHBox(QBoxLayout *vbox);

  // Create just the Ok and Cancel buttons.
  void createOkAndCancelButtons(QBoxLayout *hbox);

public:      // funcs
  ModalDialog(QWidget *parent = NULL, Qt::WindowFlags f = Qt::WindowFlags());
  ~ModalDialog();

  // Like 'exec', but dialog is centered on the target window.  (This
  // is useful when the dialog has been created with a NULL parent.
  // Note that simply changing the parent does not work.)
  //
  // If 'target' is NULL, then just call exec.
  int execCentered(QWidget *target);
};

#endif // MODAL_DIALOG_H
