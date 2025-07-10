// modal-dialog.cc
// code for modal-dialog.h

#include "modal-dialog.h"              // this module

#include "smqtutil/qtguiutil.h"        // centerWindowOnWindow, messageBox
#include "smqtutil/qtutil.h"           // SET_OBJECT_NAME

#include "smbase/xassert.h"            // xassert

#include <qtcoreversion.h>             // QTCORE_VERSION

#include <QBoxLayout>
#include <QPushButton>



ModalDialog::ModalDialog(QWidget *parent, Qt::WindowFlags f)
  : QDialog(parent, f),
    m_helpButton(nullptr),
    m_okButton(nullptr),
    m_cancelButton(nullptr),
    m_buttonHBox(nullptr),
    m_helpText()
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
  if (m_helpButton) {
    QObject::disconnect(m_helpButton, nullptr, this, nullptr);
  }
  if (m_okButton) {
    QObject::disconnect(m_okButton, nullptr, this, nullptr);
  }
  if (m_cancelButton) {
    QObject::disconnect(m_cancelButton, nullptr, this, nullptr);
  }
}


int ModalDialog::execCentered(QWidget *target)
{
  if (target) {
    centerWindowOnWindow(this, target);
  }
  return this->exec();
}


void ModalDialog::createOkAndCancelHBox(QBoxLayout *vbox)
{
  // We should not have already created them.
  xassert(m_buttonHBox == nullptr);

  m_buttonHBox = new QHBoxLayout();
  vbox->addLayout(m_buttonHBox);

  // Push the buttons to the right side.
  m_buttonHBox->addStretch(1);

  this->createOkAndCancelButtons(m_buttonHBox);
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


void ModalDialog::createHelpButton()
{
  xassert(m_buttonHBox != nullptr);
  xassert(m_helpButton == nullptr);

  m_helpButton = new QPushButton("&Help");
  m_buttonHBox->insertWidget(0, m_helpButton);
  SET_QOBJECT_NAME(m_helpButton);
  QObject::connect(m_helpButton, &QPushButton::clicked,
                   this, &ModalDialog::on_helpPressed);

  // Make the Tab order be Help -> Ok -> Cancel.  (Otherwise, it goes
  // Ok -> Cancel -> Help since Help was added last.)
  QWidget::setTabOrder(m_helpButton, m_okButton);
  QWidget::setTabOrder(m_okButton, m_cancelButton);
}


void ModalDialog::on_helpPressed() NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  messageBox(this, "Help", m_helpText);

  GENERIC_CATCH_END
}

// EOF
