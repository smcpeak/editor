// editor.h
// widget for editing text

#ifndef EDITOR_H
#define EDITOR_H

#include <qwidget.h>        // QWidget
#include "bufferstate.h"    // BufferState
#include "style.h"          // Style

class QRangeControl;        // qrangecontrol.h
class QLabel;               // qlabel.h
class StyleDB;              // styledb.h
class InputProxy;           // inputproxy.h
class StatusDisplay;        // status.h


// control to edit the contents of a buffer; it's possible (and
// expected) to change which buffer a given Editor edits after
// creating the Editor object
class Editor
  : public QWidget,          // I am a Qt widget
    public EditingState,     // view offset, selection info, etc.
    public BufferObserver {  // to watch my file while I don't have focus
  Q_OBJECT

private:     // data
  // floating info box
  QLabel *infoBox;               // (nullable owner)

  // access to the status display
  StatusDisplay *status;         // (nullable serf)

public:      // data
  // ------ editing state -----
  // buffer whose text we're editing; it can be set to NULL when
  // the ctor is called, but must if so, must be set to a non-NULL
  // value via setBuffer() before any drawing or editing is done
  BufferState *buffer;           // (serf)

  #if 0      // stored in EditingState
  // cursor position (0-based)
  int cursorLine;
  int cursorCol;

  // selection state: a location, and a flag to enable it
  int selectLine;
  int selectCol;
  bool selectEnabled;

  // scrolling offset; must change via setView()
  int const firstVisibleLine, firstVisibleCol;

  // information about viewable area; these are set by the
  // updateView() routine and should be treated as read-only by other
  // code; by "visible", I mean the entire line or column is visible
  int lastVisibleLine, lastVisibleCol;

  // when nonempty, any buffer text matching this string will
  // be highlighted in the 'hit' style; match is carried out
  // under influence of 'hitTextFlags'
  string hitText;
  Buffer::FindStringFlags hitTextFlags;
  #endif // 0

  // the following fields are valid only after normalizeSelect(),
  // and before any subsequent modification to cursor or select
  int selLowLine, selLowCol;     // whichever of cursor/select comes first
  int selHighLine, selHighCol;   // whichever comes second

  // ------ rendering options ------
  // amount of blank space at top/left edge of widget
  int topMargin, leftMargin;

  // # of blank lines of pixels between lines of text ("leading")
  int interLineSpace;

  // colors
  QColor cursorColor;               // color of text cursor
  //QColor normalFG, normalBG;        // normal text
  //QColor selectFG, selectBG;        // selected text

  // fonts, corresponding to 'enum FontVariant' (styledb.h); note
  // that these fonts all need to use the same character size
  QFont normalFont, italicFont, boldFont;

  // ------ input options ------
  // distance to move view for Ctrl-Shift-<arrow key>
  int ctrlShiftDistance;

  // current input proxy, if any
  InputProxy *inputProxy;           // (nullable serf)

  // ------ font metrics ------
  // these should be treated as read-only by all functions except
  // Editor::setFont()

  // number of pixels in a character cell that are above the
  // base line, including the base line itself
  int ascent;

  // number of pixels below the base line, not including the
  // base line
  int descent;

  // total # of pixels in each cell
  int fontHeight;
  int fontWidth;

  // ------ stuff for when I don't have focus ------
  // when I don't have the focus, two things are different:
  //   - I'm a listener to my own buffer, to track the
  //     changes made in another window.
  //   - nonfocusCursorLine/Col are valid/relevant/correct
  
  // true if I've registered myself as a listener; there
  // are a few cases where I don't have the focus, but
  // I also am not a listener (like initialization)
  bool listening;

  // location of the cursor when I left it
  int nonfocusCursorLine, nonfocusCursorCol;

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

  // ctrl-pageup/pagedown
  void cursorToTop();
  void cursorToBottom();

  // selection manipulation
  void turnOffSelection();
  void turnOnSelection();
  void turnSelection(bool on);
  void clearSelIfEmpty();

  // true if the cursor is before (above/left) the select point
  bool cursorBeforeSelect() const;

  // set cursorLine/cursorCol to the x/y derived from 'm'
  void setCursorToClickLoc(QMouseEvent *m);
  
  // intermediate paint step
  void updateFrame(QPaintEvent *ev, int cursorLine, int cursorCol);

  // draw text etc. on a QPainter
  //void drawBufferContents(QPainter &paint, int cursorLine, int cursorCol);
  void setDrawStyle(QPainter &paint, bool &underlining,
                    StyleDB *db, Style s);

  // offset from one line to the next
  int lineHeight() const { return fontHeight+interLineSpace; }

  // nonfocus listening
  void startListening();
  void stopListening();

  // debugging
  static string lineColStr(int line, int col);
  string firstVisStr() const { return lineColStr(firstVisibleLine, firstVisibleCol); }
  string cursorStr() const { return lineColStr(cursorLine(), cursorCol()); }

protected:   // funcs
  // QWidget funcs
  virtual bool event(QEvent *e);
  virtual void paintEvent(QPaintEvent *);
  virtual void keyPressEvent(QKeyEvent *k);
  virtual void keyReleaseEvent(QKeyEvent *k);
  virtual void resizeEvent(QResizeEvent *r);
  virtual void mousePressEvent(QMouseEvent *m);
  virtual void mouseMoveEvent(QMouseEvent *m);
  virtual void mouseReleaseEvent(QMouseEvent *m);
  virtual void focusInEvent(QFocusEvent *e);
  virtual void focusOutEvent(QFocusEvent *e);

public:      // funcs
  Editor(BufferState *buf, StatusDisplay *status,
         QWidget *parent=NULL, const char *name=NULL);
  ~Editor();

  // QWidget funcs
  virtual void setFont(QFont &f);

  // change which buffer this editor widget is editing
  void setBuffer(BufferState *buf);

  // cursor location
  int cursorLine() const                  { return buffer->line(); }
  int cursorCol() const                   { return buffer->col(); }

  // absolute cursor movement
  void cursorTo(int line, int col);

  // relative cursor movement
  void moveCursorBy(int dline, int dcol)  { buffer->moveRelCursor(dline, dcol); }
  void cursorLeftBy(int amt)              { moveCursorBy(0, -amt); }
  void cursorRightBy(int amt)             { moveCursorBy(0, +amt); }
  void cursorUpBy(int amt)                { moveCursorBy(-amt, 0); }
  void cursorDownBy(int amt)              { moveCursorBy(+amt, 0); }

  // set sel{Low,High}{Line,Col}
  void normalizeSelect(int cursorLine, int cursorCol);
  void normalizeSelect() { normalizeSelect(cursorLine(), cursorCol()); }

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
  // is visible; if it's already visible, do nothing; 'edgeGap' says
  // how many lines/cols of extra space on the far side of the cursor
  // we want
  void scrollToCursor(int edgeGap=0);
  void scrollToCursor_noRedraw(int edgeGap=0);

  // redraw window, etc.; calls updateView() and viewChanged()
  void redraw();

  // move the cursor and the view by a set increment, and repaint;
  // truncation at low end is automatic
  void moveViewAndCursor(int deltaLine, int deltaCol);

  // size of view in lines/cols
  int visLines() const { return lastVisibleLine-firstVisibleLine+1; }
  int visCols() const { return lastVisibleCol-firstVisibleCol+1; }

  // show the info box near the cursor
  void showInfo(char const *info);

  // hide the info box if it's visible
  void hideInfo();

  // things often bound to cursor, etc. keys; 'shift' indicates
  // whether the shift key is held (so the selection should be turned
  // on, or remain on)
  void cursorLeft(bool shift);
  void cursorRight(bool shift);
  void cursorHome(bool shift);
  void cursorEnd(bool shift);
  void cursorUp(bool shift);
  void cursorDown(bool shift);
  void cursorPageUp(bool shift);
  void cursorPageDown(bool shift);

  // some other commands...
  void deleteCharAtCursor();
  void blockIndent(int amt);

  // insert or delete small amounts of text (not guaranteed
  // to be efficient for large amounts of text)
  void insertAtCursor(char const *text);
  void deleteAtCursor(int numChars);    // deletes to the left of cursor

  // get selected text, or "" if nothing selected
  string getSelectedText();

  // BufferObserver funcs
  virtual void observeInsertLine(BufferCore const &buf, int line);
  virtual void observeDeleteLine(BufferCore const &buf, int line);
  virtual void observeInsertText(BufferCore const &buf, int line, int col, char const *text, int length);
  virtual void observeDeleteText(BufferCore const &buf, int line, int col, int length);

  // called by an input proxy when it detaches; I can
  // reset the mode pixmap then
  virtual void inputProxyDetaching();

public slots:
  // slots to respond to scrollbars
  void scrollToLine(int line);
  void scrollToCol(int col);

  // edit menu
  void editUndo();
  void editRedo();
  void editCut();
  void editCopy();
  void editPaste();
  void editDelete();

signals:
  // emitted when the buffer view port or cursor changes
  void viewChanged();
};

#endif // EDITOR_H
