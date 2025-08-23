// completions-dialog.cc
// Code for `completions-dialog` module.

#include "completions-dialog.h"        // this module

#include "lsp-data.h"                  // LSP_CompletionList, LSP_CompletionItem

#include "smqtutil/qtguiutil.h"        // removeWindowContextHelpButton, keysString, trueMoveWindow
#include "smqtutil/qtutil.h"           // SET_QOBJECT_NAME

#include "smbase/gdvalue-optional.h"   // gdv::toGDValue(std::optional)
#include "smbase/gdvalue.h"            // gdv::GDValue for TRACE1_GDVN_EXPRS
#include "smbase/sm-macros.h"          // IMEMBFP
#include "smbase/sm-trace.h"           // INIT_TRACE, etc.
#include "smbase/string-util.h"        // hasSubstring

#include <QKeyEvent>
#include <QLineEdit>
#include <QListWidget>
#include <QScrollBar>
#include <QVBoxLayout>

#include <optional>                    // std::{optional, nullopt}

using namespace gdv;


INIT_TRACE("completions-dialog");


// By how much do we scroll horizontally per keypress?
static int const HSCROLL_STEP = 100;


CompletionsDialog::~CompletionsDialog()
{
  QObject::disconnect(m_filterLineEdit, nullptr, nullptr, nullptr);
}


CompletionsDialog::CompletionsDialog(
  std::shared_ptr<LSP_CompletionList> completionList,
  QPoint upperLeftCorner,
  QWidget *parent)
  : ModalDialog(parent),
    IMEMBFP(completionList),
    m_widgetIndexToListIndex(),
    m_filterLineEdit(nullptr),
    m_listWidget(nullptr)
{
  setWindowTitle("Completions");
  setObjectName("Completions");

  resize(300, 200);
  removeWindowContextHelpButton(this);

  QVBoxLayout *vbox = new QVBoxLayout(this);

  // Eliminate margins on outer box so the table goes right to the
  // dialog edge.
  vbox->setContentsMargins(0, 0, 0, 0);

  // Have the controls touching each other.
  vbox->setSpacing(0);

  // Filter line edit.
  m_filterLineEdit = new QLineEdit(this);
  SET_QOBJECT_NAME(m_filterLineEdit);
  vbox->addWidget(m_filterLineEdit);

  // List widget.
  m_listWidget = new QListWidget(this);
  SET_QOBJECT_NAME(m_listWidget);
  vbox->addWidget(m_listWidget);

  QObject::connect(
    m_filterLineEdit, &QLineEdit::textChanged,
    this, &CompletionsDialog::populateListWidget);

  populateListWidget();

  // Set the position relative to parent.
  if (parent) {
    QRect parentRect = parent->geometry();
    QPoint parentTopLeft = parent->mapToGlobal(parentRect.topLeft());
    QPoint target = parentTopLeft + upperLeftCorner;
    TRACE1("parentRect=" << toString(parentRect) <<
           ", parentTopLeft=" << toString(parentTopLeft) <<
           ", target=" << toString(target));

    trueMoveWindow(this, target);
  }
}


/*static*/ bool CompletionsDialog::itemSatisfiesFilter(
  LSP_CompletionItem const &item,
  std::string const &filterText)
{
  if (filterText.empty()) {
    // No filtering.
    return true;
  }

  return hasSubstring(item.m_label, filterText,
    SubstringSearchFlags::SSF_CASE_INSENSITIVE);
}


void CompletionsDialog::scrollListHorizontallyBy(int delta)
{
  QScrollBar *sb = m_listWidget->horizontalScrollBar();
  sb->setValue(sb->value() + delta);
}


void CompletionsDialog::populateListWidget() noexcept
{
  GENERIC_CATCH_BEGIN

  // Get the previously selected item so we can keep it selected if
  // possible.
  std::optional<int> prevCompletionIndex = getSelectedItemIndex();

  // Clear the existing list.
  m_listWidget->clear();
  m_widgetIndexToListIndex.clear();

  // Filter to apply.
  std::string filterText = toString(m_filterLineEdit->text());

  TRACE1_GDVN_EXPRS("populateListWidget",
    prevCompletionIndex, filterText);

  std::optional<int> widgetIndexToSelect;

  int listIndex = 0;
  for (LSP_CompletionItem const &item : m_completionList->m_items) {
    if (itemSatisfiesFilter(item, filterText)) {
      m_widgetIndexToListIndex.push_back(listIndex);
      m_listWidget->addItem(toQString(item.m_label));

      if (prevCompletionIndex == listIndex) {
        widgetIndexToSelect = m_listWidget->count() - 1;
      }
    }

    ++listIndex;
  }

  if (widgetIndexToSelect) {
    int r = widgetIndexToSelect.value();
    TRACE1("preserving selection: setCurrentRow(" << r << ")");
    m_listWidget->setCurrentRow(r);
  }
  else if (prevCompletionIndex) {
    TRACE1("previous selection does not pass filter: setCurrentRow(0)");
    m_listWidget->setCurrentRow(0);
  }
  else {
    TRACE1("nothing was selected: setCurrentRow(0)");
    m_listWidget->setCurrentRow(0);
  }

  GENERIC_CATCH_END
}


void CompletionsDialog::keyPressEvent(QKeyEvent *event)
{
  GENERIC_CATCH_BEGIN

  TRACE1("keyPressEvent: " << keysString(*event));

  Qt::KeyboardModifiers mods = event->modifiers();

  if (mods == Qt::NoModifier) {
    switch (event->key()) {
      case Qt::Key_Enter:
      case Qt::Key_Return:
        if (getSelectedItemIndex()) {
          accept();
        }
        else {
          TRACE1("Ignoring Enter because nothing is selected.");
        }
        return;

      case Qt::Key_Escape:
        close();
        return;

      case Qt::Key_Down:
        if (focusWidget() == m_filterLineEdit) {
          m_listWidget->setFocus();

          if (m_listWidget->currentRow() == 0 &&
              m_listWidget->count() >= 2) {
            // When we down-arrow into the list, usually it is because
            // we want to pick an item other than the that would be
            // chosen by pressing Enter.  If that's the top item, move
            // to the next one right away.
            m_listWidget->setCurrentRow(1);
          }
          return;
        }
        break;

      case Qt::Key_Right:
        scrollListHorizontallyBy(+HSCROLL_STEP);
        break;

      case Qt::Key_Left:
        scrollListHorizontallyBy(-HSCROLL_STEP);
        break;
    }
  }

  QDialog::keyPressEvent(event);

  GENERIC_CATCH_END
}


std::optional<int> CompletionsDialog::getSelectedItemIndex() const
{
  int selectedIndex = m_listWidget->currentRow();
  if (selectedIndex >= 0) {
    int listIndex = m_widgetIndexToListIndex.at(selectedIndex);

    TRACE1_GDVN_EXPRS("getSelectedItemIndex", selectedIndex, listIndex);

    return listIndex;
  }
  else {
    TRACE1("getSelectedItemIndex: nothing selected");
    return std::nullopt;
  }
}


// EOF
