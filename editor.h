// editor.h
// widget for editing text

#ifndef EDITOR_H
#define EDITOR_H

#include <qwidget.h>        // QWidget

#include "position.h"       // Position

class Buffer;               // buffer.h


// control to edit the contents of a buffer; it's possible (and
// expected) to change which buffer a given Editor edits after
// creating the Editor object
class Editor : public QWidget {
  Q_OBJECT

public:      // data
  // buffer whose text we're editing
  Buffer *buffer;           // (serf)

  // ------ editing state -----
  // cursor position
  Position cursor;

  // scrolling offset
  int firstVisibleLine, firstVisibleCol;

  // information about viewable area; these are set by the paint()
  // routine and should be treated as read-only by other code; by
  // "visible", I mean the entire line or column is visible
  int lastVisibleLine, lastVisibleCol;

  // ------ rendering options ------
  // amount of blank space at top/left edge of widget
  int topMargin, leftMargin;

  // # of blank lines of pixels between lines of text ("leading")
  int interLineSpace;

  // ------ input options ------
  // distance to move view for Ctrl-Shift-<arrow key>
  int ctrlShiftDistance;

protected:   // funcs
  // respond to a paint request by repainting the control
  virtual void paintEvent(QPaintEvent *);

  // handle keyboard input
  virtual void keyPressEvent(QKeyEvent *k);

public:      // funcs
  Editor(Buffer *buf,
         QWidget *parent=NULL, const char *name=NULL);
                   
  // set cursor and view to 0,0
  void resetView();
  
  // scroll the view the minimum amount so that the cursor line/col
  // is visible; if it's already visible, do nothing
  void scrollToCursor();

  // move the cursor and the view by a set increment, and repaint
  void moveView(int deltaLine, int deltaCol);

  // size of view in lines/cols
  int visLines() const { return lastVisibleLine-firstVisibleLine+1; }
  int visCols() const { return lastVisibleCol-firstVisibleCol+1; }
};

#endif // EDITOR_H
