// completions-dialog-test.cc
// Non-automated test harness for `completions-dialog` module.

#include "completions-dialog.h"        // module under test

#include "lsp-data.h"                  // LSP_CompletionList

#include "smbase/list-util.h"          // smbase::listAtC
#include "smbase/sm-macros.h"          // smbase_loopi
#include "smbase/stringb.h"            // stringb
#include "smbase/string-util.h"        // join

#include <QApplication>
#include <QMessageBox>
#include <QPoint>

#include <iostream>                    // std::cout
#include <memory>                      // std::shared_ptr
#include <utility>                     // std::move

using namespace smbase;


// Called from gui-tests.cc.
int completions_dialog_test(QApplication &app)
{
  // Dummy edit to reuse for all the items since the dialog never looks
  // at them.
  LSP_TextEdit const textEdit(
    LSP_Range(
      LSP_Position(1, 2),
      LSP_Position(1, 2)
    ),
    "newText"
  );

  std::list<LSP_CompletionItem> items;
  smbase_loopi(25) {
    std::string label = stringb("completion " << (i+1));
    items.push_back(
      LSP_CompletionItem(
        // Make the some of the text items very long so I can experiment
        // with horizontal scrolling.
        join(std::vector<std::string>(i+1, label), " "),
        textEdit
      ));
  }

  std::shared_ptr<LSP_CompletionList> completionList =
    std::make_shared<LSP_CompletionList>(
      false /*isIncomplete*/,
      std::move(items)
    );

  CompletionsDialog dlg(
    completionList,
    QPoint(),
    nullptr /*parent*/);

  if (dlg.exec()) {
    if (auto selIndex = dlg.getSelectedItemIndex()) {
      std::cout << "Choice: index " << *selIndex << ": "
                << listAtC(completionList->m_items, *selIndex).m_label
                << "\n";
    }
    else {
      std::cout << "Dialog accepted, but choice is absent!\n";
    }
  }
  else {
    std::cout << "Canceled\n";
  }

  return 0;
}


// EOF
