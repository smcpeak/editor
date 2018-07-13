// textinput.cc
// code for textinput.h

#include "textinput.h"                 // this module

#include "qtutil.h"                    // toString(QString)
#include "trace.h"                     // TRACE
#include "xassert.h"                   // xassert

#include <QHBoxLayout>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>


TextInputDialog::TextInputDialog(QString title,
                                 QWidget *parent, Qt::WindowFlags f)
  : ModalDialog(parent, f),
    m_label(NULL),
    m_comboBox(NULL),
    m_history(),
    m_maxHistorySize(20),
    m_text()
{
  this->setWindowTitle(title);

  {
    QVBoxLayout *vbox = new QVBoxLayout();
    this->setLayout(vbox);

    m_label = new QLabel("Input:");
    vbox->addWidget(m_label);

    m_comboBox = new QComboBox();
    vbox->addWidget(m_comboBox);

    m_comboBox->setEditable(true);

    // I maintain the list elements myself.  (This flag does not really
    // matter since I clear them out every time.)
    m_comboBox->setInsertPolicy(QComboBox::NoInsert);

    // The default behavior of QComboBox is such that if Enter is
    // pressed while the entire edit text is selected, but that text
    // happens to exactly match one of the entries in the list, then
    // all that happens is the text becomes deselected.  You then have
    // to press Enter a second time to cause the dialog to accept and
    // close.  But if the selected text is not in the list, then one
    // Enter press suffices.  This of course seems like a bug in Qt.
    //
    // I think what is happening is the combo box is confusing the
    // case of Enter while the edit entry is selected with Enter while
    // the dropdown is open and the cursor is on one of the list items,
    // since in that case the behavior is to close the dropdown without
    // accepting the dialog.
    //
    // My solution is to connect the returnPressed signal from the
    // underlying QLineEdit to 'accept' of this dialog.
    QObject::connect(m_comboBox->lineEdit(), SIGNAL(returnPressed()),
                     this, SLOT(accept()));

    this->createOkAndCancelHBox(vbox);

    // This causes the dialog to start fairly wide, but at its minimum
    // height.  The 80 is a bit smaller than the real minimum, and the
    // layout should override that.  But I do not pass (e.g.) 0 in case
    // something goes wrong and the size gets actually used.
    this->resize(400, 80);
  }
}


TextInputDialog::~TextInputDialog()
{}


void TextInputDialog::setLabelText(QString labelText)
{
  m_label->setText(labelText);
}


void TextInputDialog::rememberInput(QString input)
{
  int existingIndex = m_history.indexOf(input);
  if (existingIndex >= 0) {
    // Move the existing item to the front.
    if (existingIndex != 0) {
      m_history.removeAt(existingIndex);
      m_history.insert(0, input);
    }
  }
  else {
    // Truncate history.
    xassert(m_maxHistorySize >= 1);
    while (m_history.size() >= m_maxHistorySize) {
      m_history.removeLast();
    }

    // Insert new item.
    m_history.insert(0, input);
  }
}


int TextInputDialog::exec()
{
  // Repopulate the combo box and give it the focus.
  m_comboBox->clear();
  if (!m_history.isEmpty() && m_history.at(0) == m_text) {
    // The text box automatically populates with the first element
    // of the history, which will be 'm_text'.
  }
  else {
    // Insert 'm_text' first so it becomes the initial text, and so
    // the first press of the Down key, which always goes to the
    // *second* element of the combo box, will go to the first element
    // of 'm_history'.
    m_comboBox->addItem(m_text);
  }
  for (int i=0; i < m_history.size(); i++) {
    m_comboBox->addItem(m_history.at(i));
  }
  m_comboBox->setFocus();

  int ret = QDialog::exec();

  TRACE("textinput", "TextInputDialog::exec returning: " << ret);
  if (ret) {
    TRACE("textinput", "text: " << toString(m_text));
  }
  return ret;
}


int TextInputDialog::runPrompt(QString prompt)
{
  // Safety check for an already-shown dialog.
  if (this->isVisible()) {
    QMessageBox::information(this, "Dialog Already Shown",
      qstringb("The \"" << toString(this->windowTitle()) << "\" dialog "
               "is already visible elsewhere.  There can only be one "
               "instance of that dialog open."));
    return 0;
  }

  this->setLabelText(prompt);
  return this->exec();
}


int TextInputDialog::runPrompt_nonEmpty(QString prompt)
{
  int ret = this->runPrompt(prompt);
  if (ret) {
    if (m_text.isEmpty()) {
      return 0;
    }
    else {
      this->rememberInput(m_text);
    }
  }
  return ret;
}


void TextInputDialog::accept()
{
  m_text = m_comboBox->currentText();
  TRACE("textinput", "accept: " << toString(m_text));
  QDialog::accept();
}


// EOF
