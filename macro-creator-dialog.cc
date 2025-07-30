// macro-creator-dialog.cc
// Code for macro-creator-dialog.h.

#include "macro-creator-dialog.h"      // this module

#include "editor-command.ast.gen.h"    // EditorCommand
#include "editor-global.h"             // EditorGlobal

#include "smqtutil/qstringb.h"         // qstringb
#include "smqtutil/qtutil.h"           // SET_QOBJECT_NAME, toQString
#include "smqtutil/sm-line-edit.h"     // SMLineEdit

#include <QLabel>
#include <QTextEdit>
#include <QVBoxLayout>


MacroCreatorDialog::MacroCreatorDialog(
  EditorGlobal *editorGlobal,
  QWidget *parent,
  Qt::WindowFlags f)
:
  ModalDialog(parent, f),
  m_editorGlobal(editorGlobal),
  m_chosenCommands(),
  m_numberOfCommands(nullptr),
  m_macroName(nullptr),
  m_commandList(nullptr)
{
  setObjectName("macro_creator_dialog");
  setWindowTitle("Macro Creator");

  QVBoxLayout *vbox = new QVBoxLayout();
  this->setLayout(vbox);

  {
    QHBoxLayout *hbox = new QHBoxLayout();
    vbox->addLayout(hbox);

    QLabel *label = new QLabel("&Number of commands:");
    hbox->addWidget(label);

    m_numberOfCommands = new SMLineEdit();
    label->setBuddy(m_numberOfCommands);
    hbox->addWidget(m_numberOfCommands);

    QObject::connect(m_numberOfCommands, &SMLineEdit::textEdited,
                     this, &MacroCreatorDialog::on_textEdited);
  }

  {
    QHBoxLayout *hbox = new QHBoxLayout();
    vbox->addLayout(hbox);

    QLabel *label = new QLabel("&Macro name:");
    hbox->addWidget(label);

    m_macroName = new SMLineEdit();
    label->setBuddy(m_macroName);
    hbox->addWidget(m_macroName);
  }

  m_commandList = new QTextEdit();
  vbox->addWidget(m_commandList);
  SET_QOBJECT_NAME(m_commandList);
  m_commandList->setReadOnly(true);
  m_commandList->setFocusPolicy(Qt::NoFocus);

  // Intercept Tab key.
  m_commandList->installEventFilter(this);

  createOkAndCancelHBox(vbox);

  updateCommandList();

  resize(1000, 800);
}


MacroCreatorDialog::~MacroCreatorDialog()
{
  // See doc/signals-and-dtors.txt.
  QObject::disconnect(m_numberOfCommands, nullptr, this, nullptr);
}


void MacroCreatorDialog::updateCommandList()
{
  m_chosenCommands.clear();

  QString text = m_numberOfCommands->text();

  int n = 0;

  if (text.isEmpty()) {
    n = 20;
  }
  else {
    bool ok = false;
    n = text.toInt(&ok);

    if (!ok) {
      m_commandList->setPlainText(qstringb(
        "error: Invalid number: " << doubleQuote(text)));
      return;
    }

    if (n < 1) {
      m_commandList->setPlainText(qstringb(
        "error: Must be positive: " << n));
      return;
    }
  }

  // Get the most recent up to `n` commands.
  m_chosenCommands = m_editorGlobal->getRecentCommands(n);

  // Serialize them into the text box.
  if (m_chosenCommands.empty()) {
    m_commandList->setPlainText("(No recent commands.)");
  }
  else {
    m_commandList->setPlainText(
      toQString(serializeECV(m_chosenCommands)));
  }
}


std::string MacroCreatorDialog::getMacroName() const
{
  return toString(m_macroName->text());
}


void MacroCreatorDialog::on_textEdited(QString const &) NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  updateCommandList();

  GENERIC_CATCH_END
}


void MacroCreatorDialog::accept() NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  if (!getMacroName().empty() &&
      !getChosenCommands().empty()) {
    this->QDialog::accept();
  }

  GENERIC_CATCH_END
}


// EOF
