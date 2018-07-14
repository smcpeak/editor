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
    m_editorWidget(NULL),
    m_ignore_findEditTextChanged(false)
{
  QVBoxLayout *vbox = new QVBoxLayout();
  this->setLayout(vbox);
  vbox->setSpacing(SAR_PANEL_SPACING);
  vbox->setContentsMargins(SAR_PANEL_MARGIN, SAR_PANEL_MARGIN,
                           SAR_PANEL_MARGIN, SAR_PANEL_MARGIN);

  {
    QHBoxLayout *hbox = new QHBoxLayout();
    vbox->addLayout(hbox);

    QLabel *findLabel = new QLabel("Find:");
    hbox->addWidget(findLabel);

    m_findBox = new QComboBox();
    hbox->addWidget(m_findBox, 1 /*stretch*/);
    m_findBox->setEditable(true);
    m_findBox->setInsertPolicy(QComboBox::NoInsert);
    m_findBox->installEventFilter(this);

    QObject::connect(m_findBox, &QComboBox::editTextChanged,
                     this, &SearchAndReplacePanel::on_findEditTextChanged);

    QLabel *replLabel = new QLabel("Repl:");
    hbox->addWidget(replLabel);

    m_replBox = new QComboBox();
    hbox->addWidget(m_replBox, 1 /*stretch*/);
    m_replBox->setEditable(true);
    m_replBox->setInsertPolicy(QComboBox::NoInsert);
    m_replBox->installEventFilter(this);

    // I can't seem to get QPushButton to be small, so use QToolButton.
    QToolButton *helpButton = new QToolButton();
    hbox->addWidget(helpButton);
    helpButton->setText("?");
    QObject::connect(helpButton, &QToolButton::clicked,
                     this, &SearchAndReplacePanel::on_help);
  }
}


SearchAndReplacePanel::~SearchAndReplacePanel()
{}


void SearchAndReplacePanel::setEditorWidget(EditorWidget *w)
{
  m_editorWidget = w;
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

  this->updateEditorHitText(false /*scroll*/);
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
          if (!m_editorWidget->searchHitSelected()) {
            m_editorWidget->nextSearchHit(false /*reverse*/);
          }
          this->toggleSARFocus();
          return true;       // no further processing
        }
        break;

      case Qt::Key_Comma:
      case Qt::Key_Period:
        if (mods == Qt::ControlModifier) {
          bool reverse = (key == Qt::Key_Comma);
          m_editorWidget->nextSearchHit(reverse);
          return true;
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

  // Draw a divider line on the bottom edge.
  QPainter paint(this);
  paint.setPen(QColor(128, 128, 128));
  int w = this->width();
  int h = this->height();
  paint.drawLine(0, h-1, w-1, h-1);
}


void SearchAndReplacePanel::on_findEditTextChanged(QString const &)
{
  if (m_ignore_findEditTextChanged) {
    return;
  }

  this->updateEditorHitText(true /*scroll*/);
}


void SearchAndReplacePanel::on_help()
{
  QMessageBox mb(this);
  mb.setWindowTitle("Search and Replace Help");
  mb.setText(
    "Keys for Search and Replace (SAR):\n"
    "\n"
    "Ctrl+S: Toggle focus between SAR and main editor.\n"
    "Enter: Go to first match in editor, unless already on a match, "
      "in which case just go to editor.\n"
    "Tab: Toggle between Find and Repl boxes.\n"
    "Esc: Close the SAR panel.\n"
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
  );
  mb.exec();
}


// EOF
