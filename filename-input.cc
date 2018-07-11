// filename-input.cc
// code for filename-input.h

#include "filename-input.h"

#include <QLineEdit>
#include <QVBoxLayout>


FilenameInputDialog::FilenameInputDialog(QWidget *parent, Qt::WindowFlags f)
  : ModalDialog(parent, f),
    m_filenameEdit(NULL)
{
  this->setWindowTitle("Filename Input");

  QVBoxLayout *vbox = new QVBoxLayout();
  this->setLayout(vbox);

  m_filenameEdit = new QLineEdit();
  vbox->addWidget(m_filenameEdit);

  this->createOkAndCancel(vbox);

  // Start with 400 width and minimum height.
  this->resize(400, 80);
}


FilenameInputDialog::~FilenameInputDialog()
{}


QString FilenameInputDialog::runDialog(QString initialChoice)
{
  m_filenameEdit->setText(initialChoice);

  if (this->exec()) {
    return m_filenameEdit->text();
  }
  else {
    return "";
  }
}


// EOF
