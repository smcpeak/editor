// list-choice-dialog-test.cc
// Non-automated test harness for `list-choice-dialog` module.

#include "list-choice-dialog.h"        // module under test

#include "smbase/stringb.h"            // stringb

#include <QApplication>
#include <QMessageBox>
#include <QPoint>

#include <iostream>                    // std::cout
#include <string>                      // std::string
#include <vector>                      // std::vector


// Called from gui-tests.cc.
int list_choice_dialog_test(QApplication &app)
{
  std::vector<std::string> choices;
  for (int i=0; i < 3; ++i) {
    choices.push_back(stringb("choice " << i));
  }

  ListChoiceDialog dlg(
    "Test of List Choice",
    nullptr /*parent*/);
  dlg.setChoices(choices);

  if (dlg.exec()) {
    int selIndex = dlg.chosenItem();
    std::cout << "Choice: index " << selIndex << ": "
              << choices.at(selIndex) << "\n";
  }
  else {
    std::cout << "Canceled\n";
  }

  return 0;
}


// EOF
