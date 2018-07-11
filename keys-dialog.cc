// keys-dialog.cc
// code for keys-dialog.h

#include "keys-dialog.h"               // this module

#include <QHBoxLayout>
#include <QPushButton>
#include <QTextEdit>
#include <QVBoxLayout>

#include <algorithm>                   // std::min


KeysDialog::KeysDialog(QString keysText, QWidget *parent, Qt::WindowFlags f)
  : ModalDialog(parent, f)
{
  this->setWindowTitle("Editor Keys");

  int h = 700;
  if (parent) {
    // Limit height to that of the parent.
    h = std::min(h, parent->height());
  }
  this->resize(400, h);

  {
    QVBoxLayout *vbox = new QVBoxLayout();

    QTextEdit *textEdit = new QTextEdit();
    textEdit->setReadOnly(true);
    textEdit->setPlainText(keysText);
    vbox->addWidget(textEdit);

    this->createOkAndCancelHBox(vbox);
    this->setLayout(vbox);
  }
}


KeysDialog::~KeysDialog()
{}


// EOF
