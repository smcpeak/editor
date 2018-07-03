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


void ModalDialog::createOkAndCancel(QBoxLayout *layout)
{
  QHBoxLayout *hbox = new QHBoxLayout();

  hbox->addStretch(1);

  QPushButton *okButton = new QPushButton("Ok");
  okButton->setDefault(true);
  QObject::connect(okButton, SIGNAL(clicked()),
                   this, SLOT(accept()));
  hbox->addWidget(okButton);

  QPushButton *cancelButton = new QPushButton("Cancel");
  QObject::connect(cancelButton, SIGNAL(clicked()),
                   this, SLOT(reject()));
  hbox->addWidget(cancelButton);

  layout->addLayout(hbox);
}


// EOF
