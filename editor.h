// editor.h
// widget for editing text

#ifndef EDITOR_H
#define EDITOR_H

#include <qwidget.h>        // QWidget

#include "position.h"       // Position

class Buffer;               // buffer.h
class QRangeControl;        // qrangecontrol.h


// control to edit the contents of a buffer; it's possible (and
// expected) to change which buffer a given Editor edits after
// creating the Editor object
class Editor : public QWidget {
  Q_OBJECT

public:      // data
  // ------ editing state -----
  // buffer whose text we're editing
  Buffer *buffer;           // (serf)

  // cursor position
  Position cursor;

  // scrolling offset
  int firstVisibleLine, firstVisibleCol;

  // information about viewable area; these are set by the
  // updateView() routine and should be treated as read-only by other
  // code; by "visible", I mean the entire line or column is visible
  int lastVisibleLine, lastVisibleCol;

  // ------ rendering options ------
  // amount of blank space at top/left edge of widget
  int topMargin, leftMargin;

  // # of blank lines of pixels between lines of text ("leading")
  int interLineSpace;

  // ------ input options ------
  // distance to move view for Ctrl-Shift-<arrow key>
  int ctrlShiftDistance;

  // ------ font metrics ------
  // these should be treated as read-only by all functions except
  // Editor::setFont()

  // number of pixels in a character cell that are above the
  // base line, including the base line itself
  int ascent;

  // number of pixels below the base line, not including the
  // base line itself
  int descent;

  // total # of pixels in each cell
  int fontHeight;
  int fontWidth;

protected:   // funcs
  // QWidget funcs
  virtual void paintEvent(QPaintEvent *);
  virtual void keyPressEvent(QKeyEvent *k);
  virtual void resizeEvent(QResizeEvent *r);
  virtual void mousePressEvent(QMouseEvent *m);

public:      // funcs
  Editor(Buffer *buf,
         QWidget *parent=NULL, const char *name=NULL);

  // QWidget funcs
  virtual void setFont(QFont &f);

  // recompute lastVisibleLine/Col
  void updateView();

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

public slots:
  // automatically calls updateView() and viewChanged()
  virtual void update();

  // slots to respond to scrollbars
  virtual void scrollToLine(int line);
  virtual void scrollToCol(int col);
  
signals:
  // emitted when the buffer view port or cursor changes
  void viewChanged();
};

#endif // EDITOR_H
