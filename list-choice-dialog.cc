// list-choice-dialog.cc
// Code for `list-choice-dialog` module.

#include "list-choice-dialog.h"        // this module

#include "smqtutil/qtutil.h"           // SET_QOBJECT_NAME

#include "smbase/exc.h"                // GENERIC_CATCH_BEGIN

#include <QListWidget>
#include <QVBoxLayout>

#include <string>                      // std::string
#include <vector>                      // std::vector


ListChoiceDialog::~ListChoiceDialog()
{}


ListChoiceDialog::ListChoiceDialog(
  QString windowTitle,
  QWidget *parent)
:
  ModalDialog(parent)
{
  setObjectName("list_choice_dialog");
  setWindowTitle(windowTitle);

  QVBoxLayout *vbox = new QVBoxLayout();
  this->setLayout(vbox);

  {
    m_listWidget = new QListWidget();
    vbox->addWidget(m_listWidget);
    SET_QOBJECT_NAME(m_listWidget);
  }

  createOkAndCancelHBox(vbox);

  resize(400, 200);
}


void ListChoiceDialog::setChoices(
  std::vector<std::string> const &choices)
{
  // Populate the list.
  for (std::string const &choice : choices) {
    m_listWidget->addItem(toQString(choice));
  }

  if (!choices.empty()) {
    // Select the first item so the user can hit Enter immediately to
    // choose it.
    m_listWidget->setCurrentRow(0, QItemSelectionModel::Select);
  }
}


void ListChoiceDialog::accept() noexcept
{
  GENERIC_CATCH_BEGIN

  if (chosenItem() >= 0) {
    this->QDialog::accept();
  }
  else {
    // Nothing selected, ignore the button press.
  }

  GENERIC_CATCH_END

}


int ListChoiceDialog::chosenItem() const
{
  return m_listWidget->currentRow();
}


// EOF
