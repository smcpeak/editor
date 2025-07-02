// macro-run-dialog.cc
// Code for macro-run-dialog.h.

#include "macro-run-dialog.h"          // this module

#include "editor-global.h"             // EditorGlobal

#include "smqtutil/qtutil.h"           // SET_QOBJECT_NAME, toQString

#include "smbase/trace.h"              // TRACE

#include <QLabel>
#include <QListWidget>
#include <QVBoxLayout>


MacroRunDialog::MacroRunDialog(
  EditorGlobal *editorGlobal,
  QWidget *parent,
  Qt::WindowFlags f)
:
  ModalDialog(parent, f),
  m_editorGlobal(editorGlobal),
  m_chosenMacroName(),
  m_macroList(nullptr)
{
  setObjectName("macro_run_dialog");
  setWindowTitle("Run Macro");

  QVBoxLayout *vbox = new QVBoxLayout();
  this->setLayout(vbox);

  {
    QLabel *label = new QLabel("&Macros:");
    vbox->addWidget(label);

    m_macroList = new QListWidget();
    label->setBuddy(m_macroList);
    vbox->addWidget(m_macroList);
    SET_QOBJECT_NAME(m_macroList);

    // Populate the list.
    std::set<std::string> macroNames = m_editorGlobal->getMacroNames();
    for (auto const &name : macroNames) {
      m_macroList->addItem(toQString(name));
    }

    // Select the first item so I can immediately use arrow keys within
    // the list to choose that or another item.
    if (!macroNames.empty()) {
      m_macroList->setCurrentRow(0, QItemSelectionModel::Select);
    }
  }

  createOkAndCancelHBox(vbox);

  resize(600, 600);
}


MacroRunDialog::~MacroRunDialog()
{}


void MacroRunDialog::accept() NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  if (QListWidgetItem *item = m_macroList->currentItem()) {
    m_chosenMacroName = toString(item->text());
    this->QDialog::accept();
  }

  GENERIC_CATCH_END
}


// EOF
