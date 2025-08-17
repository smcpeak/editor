// completions-dialog.h
// `CompletionsDialog`, allowing the user to choose an LSP completion.

// See license.txt for copyright and terms of use.

#ifndef EDITOR_COMPLETIONS_DIALOG_H
#define EDITOR_COMPLETIONS_DIALOG_H

#include "lsp-data-fwd.h"              // LSP_CompletionList, LSP_CompletionItem
#include "modal-dialog.h"              // ModalDialog

#include "smbase/sm-macros.h"          // NULLABLE
#include "smbase/std-optional-fwd.h"   // std::optional
#include "smbase/std-string-fwd.h"     // std::string

#include <memory>                      // std::shared_ptr
#include <vector>                      // std::vector

class QLineEdit;
class QListWidget;


// This dialog allows the user to choose an LSP completion.
//
// See `doc/completion-dialog-spec.html` for details.
//
class CompletionsDialog : public ModalDialog {
  Q_OBJECT

private:     // data
  // Sequence of completions to show.
  std::shared_ptr<LSP_CompletionList> m_completionList;

  // Map from an index into `m_listWidget` to an index into
  // `m_completionList`.
  std::vector<int> m_widgetIndexToListIndex;

  // Controls.
  QLineEdit *m_filterLineEdit;         // Filter text.
  QListWidget *m_listWidget;           // List of matching completions.

private:     // methods
  // True if `item` should be shown for `filterText`.
  static bool itemSatisfiesFilter(
    LSP_CompletionItem const &item,
    std::string const &filterText);

private Q_SLOTS:
  // Repopulate `m_listWidget` from `m_completionList` based on the
  // current `m_filterLineEdit` text.
  void populateListWidget() noexcept;

protected:   // methods
  virtual void keyPressEvent(QKeyEvent *event) override;

public:      // methods
  ~CompletionsDialog();

  // Create the dialog to show `completionList`.  Place its upper left
  // corner at the indicated coordinate relative to `parent` (unless it
  // is null).
  CompletionsDialog(
    std::shared_ptr<LSP_CompletionList> completionList,
    QPoint upperLeftCorner,
    QWidget * NULLABLE parent);

  // Index in `m_completionList` of the selected item, if any.
  //
  // This can be used after `exec()` to get the chosen index.
  std::optional<int> getSelectedItemIndex() const;
};


#endif // EDITOR_COMPLETIONS_DIALOG_H
