// editor-widget-frame.cc
// Code for editor-widget-frame.h.

#include "editor-widget-frame.h"       // this module

// editor
#include "editor-global.h"             // EditorGlobal
#include "editor-widget.h"             // EditorWidget
#include "editor-window.h"             // EditorWindow

// smqtutil
#include "smqtutil/qhboxframe.h"       // QHBoxFrame
#include "smqtutil/qtutil.h"           // disconnectSignalSender

// smbase
#include "smbase/objcount.h"           // CHECK_OBJECT_COUNT

// qt
#include <QGridLayout>
#include <QScrollBar>
#include <QSignalBlocker>

// libc++
#include <algorithm>                   // std::max


int EditorWidgetFrame::s_objectCount = 0;

CHECK_OBJECT_COUNT(EditorWidgetFrame);


EditorWidgetFrame::EditorWidgetFrame(EditorWindow *editorWindow,
                                     NamedTextDocument *initFile)
  : QWidget(editorWindow),
    m_editorWindow(editorWindow),
    m_editorWidget(nullptr),
    m_vertScroll(nullptr),
    m_horizScroll(nullptr)
{
  xassert(editorWindow);
  xassert(initFile);

  // The layout tree within this frame is:
  //
  //   QGridLayout editArea
  //     QHBoxFrame editorFrame row=0 col=0
  //       EditorWidget m_editorWidget
  //     QScrollBar m_vertScroll row=0 col=1
  //
  // See doc/editor-window-layout.ded.
  QGridLayout *editArea = new QGridLayout();
  editArea->setObjectName("editArea");
  editArea->setHorizontalSpacing(0); //?
  editArea->setVerticalSpacing(0); //?
  editArea->setContentsMargins(0,0,0,0);

  // Put a black one-pixel frame around the editor widget.
  QHBoxFrame *editorFrame = new QHBoxFrame();
  editorFrame->setObjectName("editorFrame");
  editorFrame->setFrameStyle(QFrame::Box);
  editArea->addWidget(editorFrame, 0 /*row*/, 0 /*col*/);

  m_editorWidget = new EditorWidget(initFile, m_editorWindow);
  m_editorWidget->setObjectName("m_editorWidget");
  editorFrame->addWidget(m_editorWidget);

  // Route signals from widget to window.
  QObject::connect(m_editorWidget, &EditorWidget::viewChanged,
                   m_editorWindow, &EditorWindow::editorViewChanged);
  QObject::connect(m_editorWidget, &EditorWidget::closeSARPanel,
                   m_editorWindow, &EditorWindow::on_closeSARPanel);

  // See EditorWidget::openDiagnosticOrFileAtCursor for why this is a
  // QueuedConnection.
  QObject::connect(
    m_editorWidget, &EditorWidget::signal_openOrSwitchToFileAtLineOpt,
    m_editorWindow, &EditorWindow::slot_openOrSwitchToFileAtLineOpt,
    Qt::QueuedConnection);

  // Delegate focus to the actual editor.
  setFocusProxy(m_editorWidget);

  m_vertScroll = new QScrollBar(Qt::Vertical);
  m_vertScroll->setObjectName("m_vertScroll");
  editArea->addWidget(m_vertScroll, 0 /*row*/, 1 /*col*/);
  QObject::connect(m_vertScroll, &QScrollBar::valueChanged,
                   m_editorWidget, &EditorWidget::scrollToLine);

  // disabling horiz scroll for now...
  //m_horizScroll = new QScrollBar(QScrollBar::Horizontal, editArea, "horizontal scrollbar");
  //QObject::connect(m_horizScroll, &QScrollBar::valueChanged, editor, &EditorWidget::scrollToCol);

  this->setLayout(editArea);

  EditorWidgetFrame::s_objectCount++;
}


EditorWidgetFrame::~EditorWidgetFrame()
{
  EditorWidgetFrame::s_objectCount--;

  disconnectSignalSender(m_editorWidget);
  disconnectSignalSender(m_vertScroll);
}


void EditorWidgetFrame::selfCheck() const
{
  xassert(m_editorWindow);
  xassert(m_editorWidget);

  m_editorWidget->selfCheck();
}


void EditorWidgetFrame::setScrollbarRangesAndValues()
{
  RCSerf<TextDocumentEditor> tde = editorWidget()->getDocumentEditor();

  // Set the scrollbars.  In both dimensions, the range includes the
  // current value so we can scroll arbitrarily far beyond the nominal
  // size of the file contents.  Also, it is essential to set the range
  // *before* setting the value, since otherwise the scrollbar's value
  // will be clamped to the old range.
  if (m_horizScroll) {
    // The purpose of this function is to set the scrollbar data based
    // on the current widget data.  But there is a signal that
    // communicates in the opposite direction (to allow the user to
    // scroll the view).  Suppress that; otherwise, the call to
    // `setRange` will clamp the value, which will then get sent to the
    // widget, altering its `firstVisible`.
    QSignalBlocker blocker(m_horizScroll);

    m_horizScroll->setRange(0, std::max(tde->maxLineLengthColumns().get(),
                                        editorWidget()->firstVisibleCol().get()));
    m_horizScroll->setValue(editorWidget()->firstVisibleCol().get());
    m_horizScroll->setSingleStep(1);
    m_horizScroll->setPageStep(editorWidget()->visCols().get());
  }

  if (m_vertScroll) {
    // As above, but for the vertical scrollbar.
    QSignalBlocker blocker(m_vertScroll);

    m_vertScroll->setRange(0, std::max(tde->numLines().get(),
                                       editorWidget()->firstVisibleLine().get()));
    m_vertScroll->setValue(editorWidget()->firstVisibleLine().get());
    m_vertScroll->setSingleStep(1);
    m_vertScroll->setPageStep(editorWidget()->visLines());
  }
}


// EOF
