// editor-widget-frame.h
// EditorWidgetFrame class.

#ifndef EDITOR_EDITOR_WIDGET_FRAME_H
#define EDITOR_EDITOR_WIDGET_FRAME_H

// editor
#include "editor-window-fwd.h"         // EditorWindow
#include "editor-widget-fwd.h"         // EditorWidget
#include "named-td-fwd.h"              // NamedTextDocument

// qt
#include <QWidget>

class QScrollBar;


// This class is a fairly thin wrapper around EditorWidget that provides
// a thin border and the scroll bar(s).  It provides the standard
// widgets that are part of the editor window that is associated with a
// specific editor widget, but not the widget itself; the widget itself
// is a leaf, with a custom paint routine, and is not composed of
// further widgets (although it does make use of a few labels to layer
// additional information over the main area in an ad-hoc way).
//
// Logically, its parent is an EditorWindow and its primary child an
// EditorWidget.
class EditorWidgetFrame : public QWidget {
  Q_OBJECT

public:      // static data
  static int s_objectCount;

private:     // data
  // Containing window.
  EditorWindow *m_editorWindow;

  // Contained editor widget, owned by this object.
  EditorWidget *m_editorWidget;

  // Scrollbars for navigating within the editor widget, also owned.
  QScrollBar *m_vertScroll, *m_horizScroll;

public:      // methods
  EditorWidgetFrame(EditorWindow *editorWindow,
                    NamedTextDocument *initFile);
  virtual ~EditorWidgetFrame() override;

  EditorWindow *editorWindow() const { return m_editorWindow; }
  EditorWidget *editorWidget() const { return m_editorWidget; }

  // Update the scrollbar ranges and values to agree with the contents
  // and first visible location in 'm_editorWidget'.
  void setScrollbarRangesAndValues();
};


#endif // EDITOR_EDITOR_WIDGET_FRAME_H
