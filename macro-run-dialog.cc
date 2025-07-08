// macro-run-dialog.cc
// Code for macro-run-dialog.h.

#include "macro-run-dialog.h"          // this module

#include "editor-global.h"             // EditorGlobal

#include "smqtutil/qtutil.h"           // SET_QOBJECT_NAME, toQString

#include "smbase/dev-warning.h"        // DEV_WARNING
#include "smbase/trace.h"              // TRACE

#include <QLabel>
#include <QListWidget>
#include <QPushButton>
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
    std::set<std::string> macroNames =
      m_editorGlobal->getSettings().getMacroNames();
    int curItem = 0;
    int selItem = 0;         // Used if there is no recent macro.
    for (auto const &name : macroNames) {
      m_macroList->addItem(toQString(name));

      if (name == m_editorGlobal->getSettings().getMostRecentlyRunMacroC()) {
        selItem = curItem;
      }

      ++curItem;
    }

    // Select the most recently run macro (or 0 if there isn't one) so I
    // can immediately use arrow keys within the list to choose that or
    // another item.
    if (!macroNames.empty()) {
      m_macroList->setCurrentRow(selItem, QItemSelectionModel::Select);
    }
  }

  createOkAndCancelHBox(vbox);

  // Change the name from "Cancel" to "Close".  The name "Cancel"
  // implies that any changes made will be discarded, but Delete takes
  // effect immediately and is not undone by closing the dialog.
  m_cancelButton->setText("Close");

  QPushButton *deleteButton = new QPushButton("&Delete");
  m_buttonHBox->insertWidget(0, deleteButton);
  SET_QOBJECT_NAME(deleteButton);
  QObject::connect(deleteButton, &QPushButton::clicked,
                   this, &MacroRunDialog::on_deletePressed);


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


void MacroRunDialog::on_deletePressed() NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  if (QListWidgetItem *item = m_macroList->currentItem()) {
    std::string name = toString(item->text());
    if (m_editorGlobal->settings_deleteMacro(this, name)) {
      // Evidently, removing `item` is somewhat complicated.
      int row = m_macroList->row(item);
      xassert(row >= 0);
      QListWidgetItem *removed = m_macroList->takeItem(row);
      xassert(removed == item);
      delete item;
    }
    else {
      DEV_WARNING("No macro called " << doubleQuote(name) << "?");
    }
  }

  GENERIC_CATCH_END
}


// EOF
