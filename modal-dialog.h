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
};

#endif // MODAL_DIALOG_H
