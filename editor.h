// editor.h
// widget for editing text

#ifndef EDITOR_H
#define EDITOR_H

#include "inputproxy.h"                // InputProxy, InputPseudoKey
#include "owner.h"                     // Owner
#include "text-document-file.h"        // TextDocumentFile
#include "textcategory.h"              // TextCategory

#include <qwidget.h>                   // QWidget

class QLabel;                          // qlabel.h
class QRangeControl;                   // qrangecontrol.h
class QtBDFFont;                       // qtbdffont.h
class StatusDisplay;                   // status.h
class StyleDB;                         // styledb.h


// Widget to edit the contents of a text file; it's possible (and
// expected) to change which file a given Editor edits after
// creating the Editor object.
//
// This class embeds SavedEditingState.  This is different from
// 'docFile->savedState' so that it is possible to have two editor
// widgets both editing the same file but with different cursor
// locations, etc.
//
// It is weird to inherit a data class like this, but the reason
// for doing it is there is not a major, fundamental difference
// between the members of Editor and those of SavedEditingState.  The
// latter just happen to be the things I have chosen to save for
// hidden files, and that set might change over time.  Therefore,
// I use inheritance to avoid any difference in syntax between the
// two sets of members.
class Editor
  : public QWidget,               // I am a Qt widget.
    public SavedEditingState,     // Cursor location, etc.
    public TextDocumentObserver { // Watch my file while I don't have focus.
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
  // File we are editing; it can be set to NULL when
  // the ctor is called, but must if so, must be set to a non-NULL
  // value via setDocumentFile() before any drawing or editing is done.
  //
  // The Editor's EditingState gets saved to 'docFile->savedState'
  // when we switch to another file, and restored when we come
  // back.
  TextDocumentFile *docFile;     // (serf)

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

  // Fonts for (indexed by) each text category; all fonts must use the
  // same character size; some entries may be NULL, indicating we
  // do not expect to draw text with that category.
  ObjArrayStack<QtBDFFont> fontForCategory;

  // Font for drawing the character under the cursor, indexed by
  // the FontVariant (modulo FV_UNDERLINE) there.
  ObjArrayStack<QtBDFFont> cursorFontForFV;

  // Font containing miniature hexadecimal characters for use when
  // a glyph is missing.
  Owner<QtBDFFont> minihexFont;

  // When true, draw visible markers on whitespace characters.
  bool visibleWhitespace;

  // Value in [0,255], where 255 is fully opaque.
  int whitespaceOpacity;

  // Column number for a soft margin.  This is used for text
  // justification and optionally to draw a margin line.
  int softMarginColumn;

  // True to draw the soft margin.
  bool visibleSoftMargin;

  // Color of the line indicating the soft margin.
  QColor softMarginColor;

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
  //   - I'm a listener to my own file, to track the
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
  // fill document with whitespace, if necessary, so that the current
  // cursor position is on a valid character, i.e.:
  //   - cursorLine < docFile->numLines()
  //   - cursorCol <= docFile->lineLength(cursorLine)
  void fillToCursor();

  // given that the cursor is at the end of a line, join this line
  // with the next one
  void spliceNextLine();

  // Justify lines near the cursor.
  void justifyNearCursorLine();

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

  // Set 'curFont' and 'underline', plus the foreground
  // and background colors of 'paint', based on 'db' and 'cat'.
  void setDrawStyle(QPainter &paint,
                    QtBDFFont *&curFont, bool &underlining,
                    StyleDB *db, TextCategory cat);

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
  Editor(TextDocumentFile *docFile, StatusDisplay *status,
         QWidget *parent=NULL);
  ~Editor();

  // set fonts, given actual BDF description data (*not* file names)
  void setFonts(char const *normal, char const *italic, char const *bold);

  // Change which file this editor widget is editing.
  void setDocumentFile(TextDocumentFile *docFile);

  // cursor location
  int cursorLine() const                  { return docFile->line(); }
  int cursorCol() const                   { return docFile->col(); }

  // absolute cursor movement
  void cursorTo(int line, int col);

  // relative cursor movement
  void moveCursorBy(int dline, int dcol)  { docFile->moveRelCursor(dline, dcol); }
  void cursorLeftBy(int amt)              { moveCursorBy(0, -amt); }
  void cursorRightBy(int amt)             { moveCursorBy(0, +amt); }
  void cursorUpBy(int amt)                { moveCursorBy(-amt, 0); }
  void cursorDownBy(int amt)              { moveCursorBy(+amt, 0); }

  // set sel{Low,High}{Line,Col}
  void normalizeSelect(int cursorLine, int cursorCol);
  void normalizeSelect() { normalizeSelect(cursorLine(), cursorCol()); }

  // Select the entire line the cursor is on.
  void selectCursorLine();

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
  void cursorToEndOfNextLine(bool shift);

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

  // TextDocumentObserver funcs
  virtual void observeInsertLine(TextDocumentCore const &buf, int line) override;
  virtual void observeDeleteLine(TextDocumentCore const &buf, int line) override;
  virtual void observeInsertText(TextDocumentCore const &buf, TextCoord tc, char const *text, int length) override;
  virtual void observeDeleteText(TextDocumentCore const &buf, TextCoord tc, int length) override;

  // called by an input proxy when it detaches; I can
  // reset the mode pixmap then
  virtual void inputProxyDetaching();

  // Initial handling of pseudokeys.  First dispatches to the
  // proxy if any, and then handles what is not yet handled.
  void pseudoKeyPress(InputPseudoKey pkey);

  // We are about to edit the text in the file.  If we are going from
  // a "clean" to "dirty" state with respect to unsaved changes, do a
  // safety check for concurrent on-disk modifications, prompting the
  // user if necessary.  Return true if it is safe to proceed, false if
  // the edit should be aborted without doing anything (because the
  // user has indicated they want to cancel it).
  bool editSafetyCheck();

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
  // Emitted when the document editing viewport or cursor changes.
  void viewChanged();
};

#endif // EDITOR_H
