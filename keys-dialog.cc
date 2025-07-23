// keys-dialog.cc
// code for keys-dialog.h

#include "keys-dialog.h"               // this module

#include <QHBoxLayout>
#include <QFont>
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
  this->resize(900, h);

  {
    QVBoxLayout *vbox = new QVBoxLayout();

    // Get a fixed-width font since doc/keysbindings.txt has a
    // two-column format that assumes fixed width characters.
    QFont font;
    font.setFamily("Courier");
    font.setStyleHint(QFont::Monospace);
    font.setFixedPitch(true);
    font.setPointSize(12);

    QTextEdit *textEdit = new QTextEdit();
    textEdit->setReadOnly(true);
    textEdit->setPlainText(keysText);
    textEdit->setFont(font);
    vbox->addWidget(textEdit);

    this->createOkAndCancelHBox(vbox);
    this->setLayout(vbox);
  }
}


KeysDialog::~KeysDialog()
{}


// EOF
