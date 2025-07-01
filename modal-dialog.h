// modal-dialog.h
// Declare ModalDialog class.

#ifndef EDITOR_MODAL_DIALOG_H
#define EDITOR_MODAL_DIALOG_H

#include "smbase/sm-macros.h"          // NULLABLE
#include "smbase/sm-noexcept.h"        // NOEXCEPT

#include <QDialog>

class QBoxLayout;
class QHBoxLayout;
class QPushButton;

// This is a base class containing some common functionality for my
// modal dialogs.
class ModalDialog : public QDialog {
  Q_OBJECT

protected:   // data
  // The buttons, which might be nullptr if they have not been added.
  QPushButton * NULLABLE m_helpButton;
  QPushButton * NULLABLE m_okButton;
  QPushButton * NULLABLE m_cancelButton;

  // The layout containing those buttons, if we created it.
  QHBoxLayout * NULLABLE m_buttonHBox;

public:      // data
  // The default `on_helpPressed` will display this.  It is initially
  // empty.
  QString m_helpText;

protected:   // funcs
  // Create the standard Ok and Cancel buttons in an hbox and add them
  // to 'vbox'.
  void createOkAndCancelHBox(QBoxLayout *vbox);

  // Create just the Ok and Cancel buttons.
  void createOkAndCancelButtons(QBoxLayout *hbox);

  // Assuming we have already createad the Ok and Cancel buttons and
  // their hbox, create a Help button to their left.
  void createHelpButton();

public:      // funcs
  ModalDialog(QWidget *parent = NULL, Qt::WindowFlags f = Qt::WindowFlags());
  ~ModalDialog();

  // Like 'exec', but dialog is centered on the target window.  (This
  // is useful when the dialog has been created with a NULL parent.
  // Note that simply changing the parent does not work.)
  //
  // If 'target' is NULL, then just call exec.
  int execCentered(QWidget *target);

public Q_SLOTS:
  // React to the Help button being pressed.  By default, this pops up
  // a dialog showing `m_helpText`.
  virtual void on_helpPressed() NOEXCEPT;
};

#endif // EDITOR_MODAL_DIALOG_H
