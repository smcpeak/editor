// sar-panel.cc
// code for sar-panel.h

#include "sar-panel.h"                 // this module

// editor
#include "editor-widget.h"             // EditorWidget

// smqtutil
#include "qtutil.h"                    // toString

// smbase
#include "macros.h"                    // Restorer
#include "trace.h"                     // TRACE

// Qt
#include <QComboBox>
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
    m_findBox(NULL),
    m_replBox(NULL),
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

    // Reserve some space so things don't jump around too much.  But the
    // label will grow, causing the QComboBoxes to shrink, if it needs
    // to.
    m_matchStatusLabel->setMinimumWidth(70);

    QLabel *findLabel = new QLabel("Find:");
    hbox->addWidget(findLabel);

    m_findBox = new QComboBox();
    hbox->addWidget(m_findBox, 1 /*stretch*/);
    m_findBox->setEditable(true);
    m_findBox->setInsertPolicy(QComboBox::NoInsert);
    m_findBox->installEventFilter(this);
    QObject::connect(m_findBox, &QComboBox::editTextChanged,
                     this, &SearchAndReplacePanel::slot_findEditTextChanged);

    QLabel *replLabel = new QLabel("Repl:");
    hbox->addWidget(replLabel);

    m_replBox = new QComboBox();
    hbox->addWidget(m_replBox, 1 /*stretch*/);
    m_replBox->setEditable(true);
    m_replBox->setInsertPolicy(QComboBox::NoInsert);
    m_replBox->installEventFilter(this);
    QObject::connect(m_replBox, &QComboBox::editTextChanged,
                     this, &SearchAndReplacePanel::slot_replEditTextChanged);

    // I can't seem to get QPushButton to be small, so use QToolButton.
    m_helpButton = new QToolButton();
    hbox->addWidget(m_helpButton);
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
      this->setFindText(toQString(m_editorWidget->getSelectedText()));
    }

    // Let the user begin typing in the Find box.
    this->setFocusFindBox();
  }
  else {
    // Give focus back to the editor and return the scroll to near the
    // cursor.
    m_editorWidget->setFocus();
    m_editorWidget->scrollToCursor();
  }
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


void SearchAndReplacePanel::updateEditorHitText(bool scrollToHit)
{
  string s(toString(m_findBox->currentText()));
  TRACE("sar", "update hit text: text=\"" << s << "\" scroll=" << scrollToHit);
  m_editorWidget->setHitText(s, scrollToHit);

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

  if (m_findBox->currentText() != panel->m_findBox->currentText()) {
    // Remember this new string.
    QString searchString(panel->m_findBox->currentText());
    this->setFindText(searchString);

    if (this->isVisible()) {
      // When this SAR panel is shown, have the associated editor show
      // its matches, but don't scroll its view.
      TRACE("sar", "received new search string, updating editor: " <<
        toString(searchString));
      this->updateEditorHitText(false /*scroll*/);
    }
    else {
      // The SAR panel is not shown, so the editor isn't showing
      // matches.
      TRACE("sar", "received new search string, but not updating editor: " <<
        toString(searchString));
    }
  }

  m_replBox->setCurrentText(panel->m_replBox->currentText());
}


void SearchAndReplacePanel::slot_findEditTextChanged(QString const &)
{
  if (m_ignore_findEditTextChanged) {
    return;
  }

  this->updateEditorHitText(true /*scroll*/);
}


void SearchAndReplacePanel::slot_replEditTextChanged(QString const &)
{
  if (!m_handlingBroadcastChange) {
    Q_EMIT signal_searchPanelChanged(this);
  }
}


bool SearchAndReplacePanel::eventFilter(QObject *watched, QEvent *event) NOEXCEPT
{
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

      case Qt::Key_R:
        if (control && !alt) {
          string s = toString(m_replBox->currentText());
          TRACE("sar", "replace: " << s);
          if (m_editorWidget->searchHitSelected()) {
            m_editorWidget->replaceSearchHit(s);
            if (shift) {
              m_editorWidget->nextSearchHit(false /*reverse*/);
            }
          }
          else {
            m_editorWidget->nextSearchHit(false /*reverse*/);
          }
          return true;
        }
        break;

      case Qt::Key_Tab:
        if (mods == Qt::NoModifier) {
          if (watched == m_replBox) {
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
          TextCoord tc = ed->cursor();
          string word = ed->getWordAfter(tc);
          TRACE("sar", "extend sel by: " << word);
          if (!word.isempty()) {
            // Extend or start a selection to include this word.
            if (!ed->markActive()) {
              ed->setMark(tc);
            }
            ed->walkCursor(word.length());

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
    }
  }

  return false;
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


void SearchAndReplacePanel::slot_help()
{
  QMessageBox mb(this);
  mb.setWindowTitle("Search and Replace Help");
  mb.setText(
    "Keys for Search and Replace (SAR):\n"
    "\n"
    "Ctrl+S: Toggle focus between SAR and main editor.\n"
    "Enter: Go to first match in editor and return focus to it.\n"
    "Tab: Toggle between Find and Repl boxes.\n"
    "Esc: Close the SAR panel, stop highlighting matches in the editor.\n"
    "\n"
    "Ctrl+Period or Ctrl+Comma: Move to next/prev match.\n"
    "Ctrl+R: Replace match with Repl text; "
      "if not on a match, move to next match.\n"
    "Shift+Ctrl+R: Replace and move to next match.\n"
    "\n"
    "Alt+(Shift+)Backspace: Undo/redo in main editor, "
      "including SAR changes.\n"
    "Ctrl+W: Add next word at editor cursor to Find box.\n"
    "\n"
    "If there is a capital letter in the Find box, search is "
      "case-sensitive, otherwise not.\n"
    "\n"
    "The numbers in the lower left show the total number of matches "
    "and the relation of the cursor and selection to them.  For "
    "example, \"<5 / 9\" means the cursor is to the left of the 5th "
    "of 9 matches.  Square brackets, like \"[5] / 9\", mean the 5th "
    "match is selected, and hence can be replaced.\n"
  );
  mb.exec();
}


// EOF
