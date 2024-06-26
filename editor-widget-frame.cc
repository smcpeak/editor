// editor-widget-frame.cc
// Code for editor-widget-frame.h.

#include "editor-widget-frame.h"       // this module

// editor
#include "editor-global.h"             // EditorGlobal
#include "editor-widget.h"             // EditorWidget
#include "editor-window.h"             // EditorWindow

// smqtutil
#include "qhboxframe.h"                // QHBoxFrame
#include "qtutil.h"                    // disconnectSignalSender

// smbase
#include "objcount.h"                  // CHECK_OBJECT_COUNT

// qt
#include <QGridLayout>
#include <QScrollBar>

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

  m_editorWidget = new EditorWidget(initFile,
    &(m_editorWindow->editorGlobal()->m_documentList),
    m_editorWindow);
  m_editorWidget->setObjectName("m_editorWidget");
  editorFrame->addWidget(m_editorWidget);

  // Route signals from widget to window.
  QObject::connect(m_editorWidget, &EditorWidget::viewChanged,
                   m_editorWindow, &EditorWindow::editorViewChanged);
  QObject::connect(m_editorWidget, &EditorWidget::closeSARPanel,
                   m_editorWindow, &EditorWindow::on_closeSARPanel);

  // See EditorWidget::fileOpenAtCursor for why this is a
  // QueuedConnection.
  QObject::connect(
    m_editorWidget, &EditorWidget::openFilenameInputDialogSignal,
    m_editorWindow, &EditorWindow::on_openFilenameInputDialogSignal,
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


void EditorWidgetFrame::setScrollbarRangesAndValues()
{
  RCSerf<TextDocumentEditor> tde = editorWidget()->getDocumentEditor();

  // Set the scrollbars.  In both dimensions, the range includes the
  // current value so we can scroll arbitrarily far beyond the nominal
  // size of the file contents.  Also, it is essential to set the range
  // *before* setting the value, since otherwise the scrollbar's value
  // will be clamped to the old range.
  if (m_horizScroll) {
    m_horizScroll->setRange(0, std::max(tde->maxLineLengthColumns(),
                                        editorWidget()->firstVisibleCol()));
    m_horizScroll->setValue(editorWidget()->firstVisibleCol());
    m_horizScroll->setSingleStep(1);
    m_horizScroll->setPageStep(editorWidget()->visCols());
  }

  if (m_vertScroll) {
    m_vertScroll->setRange(0, std::max(tde->numLines(),
                                       editorWidget()->firstVisibleLine()));
    m_vertScroll->setValue(editorWidget()->firstVisibleLine());
    m_vertScroll->setSingleStep(1);
    m_vertScroll->setPageStep(editorWidget()->visLines());
  }
}


// EOF
