// editor.h
// widget for editing text

#ifndef EDITOR_H
#define EDITOR_H

#include <qwidget.h>        // QWidget
#include "bufferstate.h"    // BufferState

class QRangeControl;        // qrangecontrol.h


// control to edit the contents of a buffer; it's possible (and
// expected) to change which buffer a given Editor edits after
// creating the Editor object
class Editor : public QWidget {
  Q_OBJECT

public:      // data
  // ------ editing state -----
  // buffer whose text we're editing
  BufferState *buffer;           // (serf)

  // cursor position (0-based)
  int cursorLine;
  int cursorCol;

  // selection state: a location, and a flag to enable it
  int selectLine;
  int selectCol;
  bool selectEnabled;

  // the following fields are valid only after normalizeSelection(),
  // and before any subsequent modification to cursor or select
  int selLowLine, selLowCol;    // whichever of cursor/select comes first
  int selHighLine, selHighCol;  // whichever comes second

  // scrolling offset; must change via setView()
  int const firstVisibleLine, firstVisibleCol;

  // information about viewable area; these are set by the
  // updateView() routine and should be treated as read-only by other
  // code; by "visible", I mean the entire line or column is visible
  int lastVisibleLine, lastVisibleCol;

  // ------ rendering options ------
  // amount of blank space at top/left edge of widget
  int topMargin, leftMargin;

  // # of blank lines of pixels between lines of text ("leading")
  int interLineSpace;

  // colors
  QColor cursorColor;               // color of text cursor
  QColor normalFG, normalBG;        // normal text
  QColor selectFG, selectBG;        // selected text

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

  // ------ event model hacks ------
  // when this is true, we ignore the scrollToLine and scrollToCol
  // signals, to avoid recursion with the scroll bars
  bool ignoreScrollSignals;

private:     // funcs
  // fill buffer with whitespace, if necessary, so that the current
  // cursor position is on a valid character, i.e.:
  //   - cursorLine < buffer->numLines()
  //   - cursorCol <= buffer->lineLength(cursorLine)
  void fillToCursor();

  // given that the cursor is at the end of a line, join this line
  // with the next one
  void spliceNextLine();

  // true if the cursor is before (above/left) the select point
  bool cursorBeforeSelect() const;
  
  // set sel{Low,High}{Line,Col}
  void normalizeSelect();

  // ctrl-pageup/pagedown
  void cursorToTop();
  void cursorToBottom();

  // selection manipulation
  void turnOffSelection();
  void turnOnSelection();
  void turnSelection(bool on);
  void clearSelIfEmpty();

  // set cursorLine/cursorCol to the x/y derived from 'm'
  void setCursorToClickLoc(QMouseEvent *m);

  // redraw window, etc.; calls updateView() and viewChanged()
  void redraw();
       
  // draw text etc. on a QPainter
  void drawBufferContents(QPainter &paint);

  // debugging
  static string lineColStr(int line, int col);
  string firstVisStr() const { return lineColStr(firstVisibleLine, firstVisibleCol); }
  string cursorStr() const { return lineColStr(cursorLine, cursorCol); }

protected:   // funcs
  // QWidget funcs
  virtual void paintEvent(QPaintEvent *);
  virtual void keyPressEvent(QKeyEvent *k);
  virtual void resizeEvent(QResizeEvent *r);
  virtual void mousePressEvent(QMouseEvent *m);
  virtual void mouseMoveEvent(QMouseEvent *m);
  virtual void mouseReleaseEvent(QMouseEvent *m);

public:      // funcs
  Editor(BufferState *buf,
         QWidget *parent=NULL, const char *name=NULL);

  // QWidget funcs
  virtual void setFont(QFont &f);

  // change the current firstVisibleLine/Col (calls updateView());
  // does *not* move the cursor
  void setView(int newFirstLine, int newFirstCol);
  void setFirstVisibleLine(int L) { setView(L, firstVisibleCol); }
  void setFirstVisibleCol(int C) { setView(firstVisibleLine, C); }

  // move the view by a delta; automatically truncates at the low end
  void moveView(int deltaLine, int deltaCol);

  // recompute lastVisibleLine/Col, based on:
  //   - firstVisibleLine/Col
  //   - width(), height()
  //   - topMargin, leftMargin, interLineSpace
  //   - fontHeight, fontWidth
  void updateView();

  // set cursor and view to 0,0
  void resetView();

  // scroll the view the minimum amount so that the cursor line/col
  // is visible; if it's already visible, do nothing
  void scrollToCursor();

  // move the cursor and the view by a set increment, and repaint;
  // truncation at low end is automatic
  void moveViewAndCursor(int deltaLine, int deltaCol);

  // size of view in lines/cols
  int visLines() const { return lastVisibleLine-firstVisibleLine+1; }
  int visCols() const { return lastVisibleCol-firstVisibleCol+1; }

public slots:
  // slots to respond to scrollbars
  void scrollToLine(int line);
  void scrollToCol(int col);

  // edit menu
  void editCopy();
  void editCut();
  void editPaste();
  void editDelete();

signals:
  // emitted when the buffer view port or cursor changes
  void viewChanged();
};

#endif // EDITOR_H
