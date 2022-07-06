// sar-panel.cc
// code for sar-panel.h

#include "sar-panel.h"                 // this module

// editor
#include "debug-values.h"              // DEBUG_VALUES
#include "editor-widget.h"             // EditorWidget

// smqtutil
#include "qtutil.h"                    // toString, quoted(QString)

// smbase
#include "codepoint.h"                 // isUppercaseLetter
#include "sm-macros.h"                 // Restorer
#include "trace.h"                     // TRACE

// Qt
#include <QComboBox>
#include <QCompleter>
#include <QCheckBox>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPainter>
#include <QToolButton>
#include <QVBoxLayout>


// Horizontal space separating the control elements in the panel, in pixels.
int const SAR_PANEL_SPACING = 5;

// Horizontal and vertical space separating controls from the edges.
int const SAR_PANEL_MARGIN = 5;


SearchAndReplacePanel::SearchAndReplacePanel(QWidget *parent,
                                             Qt::WindowFlags f)
  : QWidget(parent, f),
    m_matchStatusLabel(NULL),
    m_findBox(NULL),
    m_replBox(NULL),
    m_regexCheckBox(NULL),
    m_helpButton(NULL),
    m_editorWidget(NULL),
    m_ignore_findEditTextChanged(false),
    m_handlingBroadcastChange(false)
{
  QVBoxLayout *vbox = new QVBoxLayout();
  this->setLayout(vbox);
  vbox->setSpacing(SAR_PANEL_SPACING);
  vbox->setContentsMargins(SAR_PANEL_MARGIN, SAR_PANEL_MARGIN,
                           SAR_PANEL_MARGIN, SAR_PANEL_MARGIN);

  {
    QHBoxLayout *hbox = new QHBoxLayout();
    vbox->addLayout(hbox);

    m_matchStatusLabel = new QLabel("");
    hbox->addWidget(m_matchStatusLabel);
    SET_QOBJECT_NAME(m_matchStatusLabel);

    // Reserve some space so things don't jump around too much.  But the
    // label will grow, causing the QComboBoxes to shrink, if it needs
    // to.
    m_matchStatusLabel->setMinimumWidth(70);

    QLabel *findLabel = new QLabel("Find:");
    hbox->addWidget(findLabel);
    SET_QOBJECT_NAME(findLabel);

    m_findBox = new QComboBox();
    hbox->addWidget(m_findBox, 1 /*stretch*/);
    SET_QOBJECT_NAME(m_findBox);
    m_findBox->setEditable(true);
    m_findBox->completer()->setCaseSensitivity(Qt::CaseSensitive);
    m_findBox->setInsertPolicy(QComboBox::NoInsert);
    m_findBox->installEventFilter(this);
    QObject::connect(m_findBox, &QComboBox::editTextChanged,
                     this, &SearchAndReplacePanel::slot_findEditTextChanged);

    QLabel *replLabel = new QLabel("Repl:");
    hbox->addWidget(replLabel);
    SET_QOBJECT_NAME(replLabel);

    m_replBox = new QComboBox();
    hbox->addWidget(m_replBox, 1 /*stretch*/);
    SET_QOBJECT_NAME(m_replBox);
    m_replBox->setEditable(true);
    m_replBox->completer()->setCaseSensitivity(Qt::CaseSensitive);
    m_replBox->setInsertPolicy(QComboBox::NoInsert);
    m_replBox->installEventFilter(this);
    QObject::connect(m_replBox, &QComboBox::editTextChanged,
                     this, &SearchAndReplacePanel::slot_replEditTextChanged);

    // This is called "E" because "R" is taken: Ctrl+R means "replace",
    // and Alt+R means "Run" command.
    m_regexCheckBox = new QCheckBox("E");
    hbox->addWidget(m_regexCheckBox);
    SET_QOBJECT_NAME(m_regexCheckBox);
    m_regexCheckBox->setChecked(false);
    QObject::connect(m_regexCheckBox, &QCheckBox::stateChanged,
                     this, &SearchAndReplacePanel::slot_regexStateChanged);

    // I can't seem to get QPushButton to be small, so use QToolButton.
    m_helpButton = new QToolButton();
    hbox->addWidget(m_helpButton);
    SET_QOBJECT_NAME(m_helpButton);
    m_helpButton->setText("?");
    QObject::connect(m_helpButton, &QToolButton::clicked,
                     this, &SearchAndReplacePanel::slot_help);
  }
}


SearchAndReplacePanel::~SearchAndReplacePanel()
{
  // Disconnect the widget and the status label.
  this->setEditorWidget(NULL);

  // See doc/signals-and-dtors.txt.
  QObject::disconnect(m_findBox, NULL, this, NULL);
  QObject::disconnect(m_replBox, NULL, this, NULL);
  QObject::disconnect(m_regexCheckBox, NULL, this, NULL);
  QObject::disconnect(m_helpButton, NULL, this, NULL);
}


void SearchAndReplacePanel::setEditorWidget(EditorWidget *w)
{
  if (m_editorWidget) {
    QObject::disconnect(m_editorWidget, NULL,
                        m_matchStatusLabel, NULL);
  }

  m_editorWidget = w;
  m_matchStatusLabel->setText("");

  if (m_editorWidget) {
    QObject::connect(m_editorWidget, &EditorWidget::signal_searchStatusIndicator,
                     m_matchStatusLabel, &QLabel::setText);
  }
}


void SearchAndReplacePanel::toggleSARFocus()
{
  if (m_editorWidget->hasFocus()) {
    if (!this->isVisible()) {
      this->show();
    }

    // If there was text selected in the editor, then let that
    // initialize my Find box.  (Otherwise, leave it alone.)
    if (m_editorWidget->selectEnabled()) {
      if (m_editorWidget->searchHitSelected()) {
        // Exception: The selected text is already a search hit.  Then
        // I should not change the search string.  This matters when
        // using case-insensitive or regex search.
      }
      else {
        this->setFindText(toQString(m_editorWidget->getSelectedText()));
      }
    }

    // Let the user begin typing in the Find box.
    this->setFocusFindBox();
  }
  else {
    // Give focus back to the editor and return the scroll to near the
    // cursor.
    m_editorWidget->setFocus();
    m_editorWidget->scrollToCursor();

    // Additionally, remember find/repl strings in history now.
    this->rememberFindReplStrings();
  }
}


void SearchAndReplacePanel::editReplace(bool advanceOnReplace)
{
  if (!this->isVisible()) {
    // If the panel wasn't shown then no hits are shown, etc.  Just
    // enable and toggle to the panel.
    this->toggleSARFocus();
    return;
  }

  this->replaceOrNext(advanceOnReplace);
}


void SearchAndReplacePanel::replaceOrNext(bool advanceOnReplace)
{
  string s = toString(m_replBox->currentText());
  if (m_editorWidget->searchHitSelected()) {
    m_editorWidget->replaceSearchHit(s);
    if (advanceOnReplace) {
      TRACE("sar", "replace: advancing to next");
      m_editorWidget->nextSearchHit(false /*reverse*/);
    }
  }
  else {
    TRACE("sar", "replace: advancing to next");
    m_editorWidget->nextSearchHit(false /*reverse*/);
  }
}


static void rememberString(QComboBox *cbox, char const *which)
{
  QString currentString = cbox->currentText();
  if (currentString.isEmpty()) {
    // Ignore the empty string.
    return;
  }

  int index = cbox->findText(currentString);
  if (index == 0) {
    TRACE("sar", "remembering " << which << ": " << quoted(currentString) <<
                 " already at the top, nothing to do");
    return;
  }

  TRACE("sar", "remembering " << which << ": " << quoted(currentString));
  cbox->insertItem(0, currentString);

  // Make the inserted item "current" so I can remove the other copy.
  // Otherwise, the combo box would switch the current text to be the
  // index item in the list.
  cbox->setCurrentIndex(0);

  if (index > 0) {
    // Remove the second copy of the so it will only appear once.
    cbox->removeItem(index+1);
  }

  // Trim the list if it is too long.
  while (cbox->count() > 30) {
    cbox->removeItem(cbox->count()-1);
  }
}


void SearchAndReplacePanel::rememberFindReplStrings()
{
  rememberString(m_findBox, "Find");
  rememberString(m_replBox, "Repl");
}


void SearchAndReplacePanel::setFocusFindBox()
{
  TRACE("sar", "focus on to Find box");
  m_findBox->setFocus();
  m_findBox->lineEdit()->selectAll();

  // The editor widget clears its hit text when the SAR panel is hidden
  // in order to not show the search matches.  When the panel is shown,
  // we want to restore the widget's hit text to what the SAR panel
  // remembers.  Also, the panel's text might have just been changed due
  // to hitting Ctrl+S while text is selected, and the editor widget
  // will not have known about that string before.
  //
  // We do not scroll here because the user should be able to hit Ctrl+S
  // to freely toggle between the editor and SAR panel without
  // disrupting their view.  Only when they actively change the search
  // string will we scroll to matches.
  this->updateEditorHitText(false /*scroll*/);
}


bool SearchAndReplacePanel::findHasNonSelectedText() const
{
  QLineEdit *lineEdit = m_findBox->lineEdit();
  return !lineEdit->text().isEmpty() &&
         lineEdit->selectedText() != lineEdit->text();
}


void SearchAndReplacePanel::setFindText(QString const &text)
{
  // Calling 'setCurrentText' fires 'findEditTextChanged', which will
  // in turn cause scrolling.  Suppress that.
  Restorer<bool> restorer(m_ignore_findEditTextChanged, true);

  m_findBox->setCurrentText(text);
}


static bool hasUppercaseLetter(string const &t)
{
  for (char const *p = t.c_str(); *p; p++) {
    if (isUppercaseLetter(*p)) {
      return true;
    }
  }
  return false;
}

void SearchAndReplacePanel::updateEditorHitText(bool scrollToHit)
{
  string s(toString(m_findBox->currentText()));
  TRACE("sar", "update hit text: text=\"" << s << "\" scroll=" << scrollToHit);

  TextSearch::SearchStringFlags flags = TextSearch::SS_NONE;

  // Case-sensitive iff uppercase letter present.
  if (!hasUppercaseLetter(s)) {
    flags |= TextSearch::SS_CASE_INSENSITIVE;
  }

  if (m_regexCheckBox->isChecked()) {
    flags |= TextSearch::SS_REGEX;
  }

  m_editorWidget->setSearchStringParams(s, flags, scrollToHit);

  if (!m_handlingBroadcastChange) {
    Q_EMIT signal_searchPanelChanged(this);
  }
}


void SearchAndReplacePanel::searchPanelChanged(SearchAndReplacePanel *panel)
{
  if (this == panel) {
    // We originated this change; ignore it.
    return;
  }

  // Do not broadcast the changes resulting from receiving this.
  Restorer<bool> restorer(m_handlingBroadcastChange, true);

  // Is anything different?
  bool changed = false;

  if (m_regexCheckBox->isChecked() != panel->m_regexCheckBox->isChecked()) {
    m_regexCheckBox->setChecked(panel->m_regexCheckBox->isChecked());
    changed = true;
  }

  if (m_findBox->currentText() != panel->m_findBox->currentText()) {
    this->setFindText(panel->m_findBox->currentText());
    changed = true;
  }

  // Changes don't matter for this one because it does not affect the
  // editor's display.
  m_replBox->setCurrentText(panel->m_replBox->currentText());

  if (!changed) {
    TRACE("sar", "received params, but no visible changes");
  }
  else if (this->isVisible()) {
    // When this SAR panel is shown, have the associated editor show
    // its matches, but don't scroll its view.
    TRACE("sar", "received new params, updating editor");
    this->updateEditorHitText(false /*scroll*/);
  }
  else {
    // The SAR panel is not shown, so the editor isn't showing
    // matches.
    TRACE("sar", "received new params, but not updating editor");
  }
}


void SearchAndReplacePanel::slot_findEditTextChanged(QString const &text) NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  if (m_ignore_findEditTextChanged) {
    return;
  }

  TRACE("sar", "slot_findEditTextChanged: " << toString(text));
  this->updateEditorHitText(true /*scroll*/);

  GENERIC_CATCH_END
}


void SearchAndReplacePanel::slot_replEditTextChanged(QString const &) NOEXCEPT
{
  GENERIC_CATCH_BEGIN
  if (!m_handlingBroadcastChange) {
    Q_EMIT signal_searchPanelChanged(this);
  }
  GENERIC_CATCH_END
}


void SearchAndReplacePanel::slot_regexStateChanged(int) NOEXCEPT
{
  GENERIC_CATCH_BEGIN
  if (!m_handlingBroadcastChange) {
    this->updateEditorHitText(true /*scroll*/);
    Q_EMIT signal_searchPanelChanged(this);
  }
  GENERIC_CATCH_END
}


bool SearchAndReplacePanel::eventFilter(QObject *watched, QEvent *event) NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  if ((watched == m_findBox || watched == m_replBox) &&
      event->type() == QEvent::KeyPress) {
    QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);
    Qt::KeyboardModifiers mods = keyEvent->modifiers();
    bool shift   = (mods & Qt::ShiftModifier);
    bool control = (mods & Qt::ControlModifier);
    bool alt     = (mods & Qt::AltModifier);
    int key = keyEvent->key();

    switch (key) {
      case Qt::Key_Return:
      case Qt::Key_Enter:
        if (mods == Qt::NoModifier) {
          // Go to first hit if we are not already on one, then switch
          // back to the editor widget.  This allows Ctrl+S, <word>,
          // Enter to go to the first hit.
          //
          // If there is no next, go to previous.
          if (!m_editorWidget->searchHitSelected()) {
            m_editorWidget->nextSearchHit(false /*reverse*/) ||
              m_editorWidget->nextSearchHit(true /*reverse*/);
          }
          this->toggleSARFocus();
          return true;       // no further processing
        }
        break;

      case Qt::Key_Tab:
        if (mods == Qt::NoModifier) {
          if (watched == m_findBox) {
            // Skip over the "E" checkbox, go to Repl.
            m_replBox->setFocus();
            return true;
          }
          else if (watched == m_replBox) {
            // Cycle back around to find.
            m_findBox->setFocus();
            return true;
          }
        }
        break;

      case Qt::Key_Backtab:
        // I see Backtab with ShiftModifier, but I could imagine that
        // changing in another version of Qt.
        if (mods == Qt::NoModifier || mods == Qt::ShiftModifier) {
          if (watched == m_findBox) {
            // Cycle around to repl.
            m_replBox->setFocus();
            return true;
          }
        }
        break;

      case Qt::Key_Backspace:
        if (alt && !control) {
          // Rather than undo/redo applying to the text in the find
          // and repl boxes, apply it to the main editor.
          if (shift) {
            m_editorWidget->editRedo();
          }
          else {
            m_editorWidget->editUndo();
          }
          return true;
        }
        break;

      case Qt::Key_Escape:
        if (mods == Qt::NoModifier) {
          m_editorWidget->doCloseSARPanel();
          return true;
        }
        break;

      case Qt::Key_W:
        if (mods == Qt::ControlModifier) {
          if (!m_editorWidget->searchHitSelected() &&
              this->findHasNonSelectedText()) {
            // The Find box does not agree with what is currently
            // selected.  First go to a hit, and we will extend from
            // there.
            m_editorWidget->nextSearchHit(false /*reverse*/);
          }

          TextDocumentEditor *ed = m_editorWidget->m_editor;
          ed->normalizeCursorGTEMark();
          TextLCoord tc = ed->cursor();
          string word = ed->getWordAfter(tc);
          TRACE("sar", "extend sel by: " << word);
          if (!word.isempty()) {
            // Extend or start a selection to include this word.
            if (!ed->markActive()) {
              ed->setMark(tc);
            }
            ed->walkCursorBytes(word.length());

            // Hack: I want these swapped.  TODO: I should turn things
            // around so SAR creates selections with cursor >= mark,
            // which elsewhere I call "normal".
            ed->swapCursorAndMark();

            // Now set the find text to match the selection.
            this->setFindText(toQString(ed->getSelectedText()));
            this->updateEditorHitText(false /*scroll*/);

            m_editorWidget->update();
          }
          return true;
        }
        break;

      case Qt::Key_E:
        if (mods == Qt::ControlModifier) {
          m_regexCheckBox->toggle();
          return true;
        }
        break;
    }
  }

  return false;

  GENERIC_CATCH_END_RET(false)
}


void SearchAndReplacePanel::paintEvent(QPaintEvent *event)
{
  QWidget::paintEvent(event);

  // Draw a divider line on the bottom edge.  (I would have done this by
  // inheriting QFrame, but it wants to draw a box around the whole
  // thing, whereas I just want this one line.)
  QPainter paint(this);
  paint.setPen(QColor(128, 128, 128));
  int w = this->width();
  int h = this->height();
  paint.drawLine(0, h-1, w-1, h-1);
}


void SearchAndReplacePanel::slot_help() NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  QMessageBox mb(this);
  mb.setWindowTitle("Search and Replace Help");
  mb.setTextFormat(Qt::RichText);     // For hyperlink to PCRE docs.
  mb.setText(
    "Keys and help for Search and Replace (SAR):<br>\n"
    "<br>\n"
    "Ctrl+S: Toggle focus between SAR and main editor.<br>\n"
    "Enter: Go to first match in editor and return focus to it.<br>\n"
    "Tab: Toggle between Find and Repl boxes.<br>\n"
    "Esc: Close the SAR panel, stop highlighting matches in the editor.<br>\n"
    "<br>\n"
    "Ctrl+E: Toggle regEx.  The syntax is "
      "<a href=\"https://www.pcre.org/original/doc/html/pcrepattern.html\">"
      "Perl-compatible regular expression</a>.<br>\n"
    "<br>\n"
    "Ctrl+Period or Ctrl+Comma: Move to next/prev match.<br>\n"
    "Ctrl+R: Replace match with Repl text; "
      "if not on a match, move to next match.<br>\n"
    "Shift+Ctrl+R: Replace and move to next match.<br>\n"
    "In regEx mode, replace string can have \\0 through \\9 to insert "
      "capture groups where \\0 is the whole matched string, \\n, \\t, "
      "and \\t to insert newline, tab, and CR respectively, and "
      "\\&lt;other&gt; to insert any other.<br>\n"
    "<br>\n"
    "Alt+(Shift+)Backspace: Undo/redo in main editor, "
      "including SAR changes.<br>\n"
    "Ctrl+W: Add next word at editor cursor to Find box.<br>\n"
    "<br>\n"
    "If there is an uppercase letter in the Find box, search is "
      "case-sensitive, otherwise not.<br>\n"
    "<br>\n"
    "The numbers in the lower left show the total number of matches "
    "and the relation of the cursor and selection to them.  For "
    "example, \"&lt;5 / 9\" means the cursor is to the left of the 5th "
    "of 9 matches.  Square brackets, like \"[5] / 9\", mean the 5th "
    "match is selected, and hence can be replaced (with Ctrl+R).<br>\n"
    "<br>\n"
    "If the total ends with '+', it means the match limit of 1000 was hit.<br>\n"
    "<br>\n"
    "A regEx syntax error results in status \"Err @ N\" where N is the "
      "character number in the Find box where the error is.<br>\n"
  );
  mb.exec();

  GENERIC_CATCH_END
}


// EOF
