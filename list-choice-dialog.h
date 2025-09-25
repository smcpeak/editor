// list-choice-dialog.h
// `ListChoiceDialog`, letting the user choose an item from a list.

// See license.txt for copyright and terms of use.

#ifndef EDITOR_LIST_CHOICE_DIALOG_H
#define EDITOR_LIST_CHOICE_DIALOG_H

#include "list-choice-dialog-fwd.h"    // fwds for this module

#include "modal-dialog.h"              // ModalDialog

#include "smbase/std-string-fwd.h"     // std::string [n]
#include "smbase/std-vector-fwd.h"     // stdfwd::vector [n]

class QListWidget;
class QWidget;


class ListChoiceDialog : public ModalDialog {
  Q_OBJECT

private:     // data
  // List of items to choose from.
  QListWidget *m_listWidget = nullptr;

public:      // methods
  ~ListChoiceDialog();

  ListChoiceDialog(QString windowTitle, QWidget *parent = nullptr);

  // Populate the list with `choices`.
  void setChoices(stdfwd::vector<std::string> const &choices);

public Q_SLOTS:
  // Called when "Ok" is pressed.
  virtual void accept() noexcept override;

  // Call after `exec` returns true to get the index in `choices` of the
  // chosen item.
  int chosenItem() const;
};


#endif // EDITOR_LIST_CHOICE_DIALOG_H
