// editor.h
// widget for editing text

#ifndef EDITOR_H
#define EDITOR_H

#include "bufferstate.h"    // BufferState
#include "inputproxy.h"     // InputProxy, InputPseudoKey
#include "style.h"          // Style

#include <qwidget.h>        // QWidget

class QLabel;               // qlabel.h
class QRangeControl;        // qrangecontrol.h
class QtBDFFont;            // qtbdffont.h
class StatusDisplay;        // status.h
class StyleDB;              // styledb.h


// Widget to edit the contents of a buffer; it's possible (and
// expected) to change which buffer a given Editor edits after
// creating the Editor object.
//
// This class embeds SavedEditingState.  This is different from
// 'buffer->savedState' so that it is possible to have two editor
// widgets both editing the same file but with different cursor
// locations, etc.
//
// It is weird to inherit a data class like this, but the reason
// for doing it is there is not a major, fundamental difference
// between the members of Editor and those of EditorState.  The
// latter just happen to be the things I have chosen to save for
// hidden buffers, and that set might change over time.  Therefore,
// I use inheritance to avoid any difference in syntax between the
// two sets of members.
class Editor
  : public QWidget,            // I am a Qt widget.
    public SavedEditingState,  // Cursor location, etc.
    public BufferObserver {    // Watch my file while I don't have focus.
  Q_OBJECT

public:     // static data
  // Instances created minus instances destroyed.
  static int objectCount;

private:     // data
  // floating info box
  QLabel *infoBox;               // (nullable owner)

  // access to the status display
  StatusDisplay *status;         // (nullable serf)

public:      // data
  // ------ editing state -----
  // Buffer whose text we're editing; it can be set to NULL when
  // the ctor is called, but must if so, must be set to a non-NULL
  // value via setBuffer() before any drawing or editing is done.
  //
  // The Editor's EditingState gets saved to 'buffer->savedState'
  // when we switch to another buffer, and restored when we come
  // back.
  BufferState *buffer;           // (serf)

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

  // fonts for (indexed by) each text style; all fonts must use the
  // same character size; some entries may be NULL, indicating we
  // do not expect to draw text with that style
  ObjArrayStack<QtBDFFont> fontForStyle;
  
  // Font for drawing the character under the cursor, indexed by
  // the FontVariant (modulo FV_UNDERLINE) there.
  ObjArrayStack<QtBDFFont> cursorFontForFV;

  // ------ input options ------
  // distance to move view for Ctrl-Shift-<arrow key>
  int ctrlShiftDistance;

  // current input proxy, if any
  InputProxy *inputProxy;           // (nullable serf)

  // ------ font metrics ------
  // these should be treated as read-only by all functions except
  // Editor::setFonts()

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

  // Paint a single character at the given location.
  void drawOneChar(QPainter &paint, QtBDFFont *font, QPoint const &pt, char c);

  // set 'curFont' and 'underline', plus the foreground
  // and background colors of 'paint', based on 'db' and 's'
  void setDrawStyle(QPainter &paint,
                    QtBDFFont *&curFont, bool &underlining,
                    StyleDB *db, Style s);

  // offset from one line to the next
  int lineHeight() const { return fontHeight+interLineSpace; }
  
  // helper
  int stcHelper(int firstVis, int lastVis, int cur, int gap);

  // nonfocus listening
  void startListening();
  void stopListening();

  // debugging
  static string lineColStr(int line, int col);
  string firstVisStr() const
    { return lineColStr(this->firstVisibleLine,
                        this->firstVisibleCol); }
  string cursorStr() const
    { return lineColStr(this->cursorLine(), this->cursorCol()); }

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
         QWidget *parent=NULL);
  ~Editor();

  // set fonts, given actual BDF description data (*not* file names)
  void setFonts(char const *normal, char const *italic, char const *bold);

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

  // Change the current firstVisibleLine/Col (calls updateView());
  // does *not* move the cursor.
  void setView(int newFirstLine, int newFirstCol);
  void setFirstVisibleLine(int L)
    { this->setView(L, this->firstVisibleCol); }
  void setFirstVisibleCol(int C)
    { this->setView(this->firstVisibleLine, C); }

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
  // we want; if 'edgeGap' is -1, then if the cursor isn't already visible,
  // center the viewport
  void scrollToCursor(int edgeGap=0);
  void scrollToCursor_noRedraw(int edgeGap=0);

  // redraw window, etc.; calls updateView() and viewChanged()
  void redraw();

  // move the cursor and the view by a set increment, and repaint;
  // truncation at low end is automatic
  void moveViewAndCursor(int deltaLine, int deltaCol);

  // Number of fully visible lines/columns.  Part of the next
  // line/col may also be visible.
  int visLines() const
    { return this->lastVisibleLine - this->firstVisibleLine + 1; }
  int visCols() const
    { return this->lastVisibleCol - this->firstVisibleCol + 1; }

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

  // Delete the character to the left of the cursor.  Or, if some
  // text is selected, delete that.
  void deleteLeftOfCursor();

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

  // Initial handling of pseudokeys.  First dispatches to the
  // proxy if any, and then handles what is not yet handled.
  void pseudoKeyPress(InputPseudoKey pkey);
  
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
