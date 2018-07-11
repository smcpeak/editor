// modal-dialog.cc
// code for modal-dialog.h

#include "modal-dialog.h"              // this module

#include <qtcoreversion.h>             // QTCORE_VERSION

#include <QBoxLayout>
#include <QPushButton>


ModalDialog::ModalDialog(QWidget *parent, Qt::WindowFlags f)
  : QDialog(parent, f)
{
#if QTCORE_VERSION >= 0x050900
  // Remove the "?" button in the title bar.  I use help buttons
  // next the controls instead.
  this->setWindowFlag(Qt::WindowContextHelpButtonHint, false /*on*/);
#endif
}


ModalDialog::~ModalDialog()
{}


void ModalDialog::createOkAndCancelHBox(QBoxLayout *vbox)
{
  QHBoxLayout *hbox = new QHBoxLayout();
  vbox->addLayout(hbox);

  hbox->addStretch(1);

  this->createOkAndCancelButtons(hbox);
}


void ModalDialog::createOkAndCancelButtons(QBoxLayout *hbox)
{
  QPushButton *okButton = new QPushButton("Ok");
  hbox->addWidget(okButton);
  okButton->setDefault(true);
  QObject::connect(okButton, &QPushButton::clicked,
                   this, &ModalDialog::accept);

  QPushButton *cancelButton = new QPushButton("Cancel");
  hbox->addWidget(cancelButton);
  QObject::connect(cancelButton, &QPushButton::clicked,
                   this, &ModalDialog::reject);
}


// EOF
