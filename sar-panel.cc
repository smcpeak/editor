// sar-panel.cc
// code for sar-panel.h

#include "sar-panel.h"                 // this module

// editor
#include "editor-widget.h"             // EditorWidget

// smqtutil
#include "qtutil.h"                    // toString

// smbase
#include "trace.h"                     // TRACE

// Qt
#include <QComboBox>
#include <QKeyEvent>
#include <QLabel>
#include <QPainter>
#include <QVBoxLayout>


SearchAndReplacePanel::SearchAndReplacePanel(QWidget *parent,
                                             Qt::WindowFlags f)
  : QWidget(parent, f),
    m_findBox(NULL),
    m_editorWidget(NULL)
{
  QVBoxLayout *vbox = new QVBoxLayout();
  this->setLayout(vbox);

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
  }
}


SearchAndReplacePanel::~SearchAndReplacePanel()
{}


void SearchAndReplacePanel::setEditorWidget(EditorWidget *w)
{
  m_editorWidget = w;
}


void SearchAndReplacePanel::setFocusFindBox()
{
  TRACE("sar", "focus on to Find box");
  m_findBox->setFocus();
}


bool SearchAndReplacePanel::eventFilter(QObject *watched, QEvent *event) NOEXCEPT
{
  if ((watched == m_findBox || watched == m_replBox) &&
      event->type() == QEvent::KeyPress) {
    QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);
    bool shift = (keyEvent->modifiers() & Qt::ShiftModifier);
    bool control = (keyEvent->modifiers() & Qt::ControlModifier);

    switch (keyEvent->key()) {
      case Qt::Key_Return:
      case Qt::Key_Enter: {
        TRACE("sar", "next/prev search hit");
        m_editorWidget->nextSearchHit(shift /*reverse*/);
        return true;       // no further processing
      }

      case Qt::Key_R:
        if (control) {
          string s = toString(m_replBox->currentText());
          TRACE("sar", "replace: " << s);
          if (m_editorWidget->selectEnabled()) {
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
        if (watched == m_replBox) {
          // Cycle back around to find.
          m_findBox->setFocus();
          return true;
        }
        break;

      case Qt::Key_Backtab:
        if (watched == m_findBox) {
          // Cycle around to repl.
          m_replBox->setFocus();
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


void SearchAndReplacePanel::on_findEditTextChanged(QString const &text)
{
  string s(toString(text));
  TRACE("sar", "Find text: " << s);
  m_editorWidget->setHitText(s);
}


// EOF
