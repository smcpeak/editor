// modal-dialog.cc
// code for modal-dialog.h

#include "modal-dialog.h"              // this module

#include "qtutil.h"                    // SET_OBJECT_NAME

#include <qtcoreversion.h>             // QTCORE_VERSION

#include <QBoxLayout>
#include <QPushButton>


ModalDialog::ModalDialog(QWidget *parent, Qt::WindowFlags f)
  : QDialog(parent, f),
    m_okButton(NULL),
    m_cancelButton(NULL)
{
#if QTCORE_VERSION >= 0x050900
  // Remove the "?" button in the title bar.  I use help buttons
  // instead.
  this->setWindowFlag(Qt::WindowContextHelpButtonHint, false /*on*/);
#endif
}


ModalDialog::~ModalDialog()
{
  // See doc/signals-and-dtors.txt.
  if (m_okButton) {
    QObject::disconnect(m_okButton, NULL, this, NULL);
  }
  if (m_cancelButton) {
    QObject::disconnect(m_cancelButton, NULL, this, NULL);
  }
}


void ModalDialog::createOkAndCancelHBox(QBoxLayout *vbox)
{
  QHBoxLayout *hbox = new QHBoxLayout();
  vbox->addLayout(hbox);

  hbox->addStretch(1);

  this->createOkAndCancelButtons(hbox);
}


void ModalDialog::createOkAndCancelButtons(QBoxLayout *hbox)
{
  m_okButton = new QPushButton("Ok");
  hbox->addWidget(m_okButton);
  SET_QOBJECT_NAME(m_okButton);
  m_okButton->setDefault(true);
  QObject::connect(m_okButton, &QPushButton::clicked,
                   this, &ModalDialog::accept);

  m_cancelButton = new QPushButton("Cancel");
  hbox->addWidget(m_cancelButton);
  SET_QOBJECT_NAME(m_cancelButton);
  QObject::connect(m_cancelButton, &QPushButton::clicked,
                   this, &ModalDialog::reject);
}


// EOF
