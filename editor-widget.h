// editor-widget.h
// EditorWidget class.

#ifndef EDITOR_H
#define EDITOR_H

#include "inputproxy.h"                // InputProxy, InputPseudoKey
#include "owner.h"                     // Owner
#include "td-editor.h"                 // TextDocumentEditor
#include "td-file.h"                   // TextDocumentFile
#include "textcategory.h"              // TextCategory

#include <qwidget.h>                   // QWidget

class IncSearch;                       // incsearch.h
class QtBDFFont;                       // qtbdffont.h
class StatusDisplay;                   // status.h
class StyleDB;                         // styledb.h

class QLabel;                          // qlabel.h
class QRangeControl;                   // qrangecontrol.h


// Widget to edit the contents of text files.  The widget shows and
// edits one file at a time, but remembers state about other files.
//
// This class is intended to be mostly concerned with connecting the
// user to the TextDocumentEditor API; the latter should do the actual
// text manipulation.
class EditorWidget
  : public QWidget,               // I am a Qt widget.
    public TextDocumentObserver { // Watch my file while I don't have focus.
  Q_OBJECT

  // TODO: I currently need to let IncSearch access private members
  // 'docFile', 'selLowLine', etc.  I think IncSearch should be able
  // to operate on top of TextFileEditor instead of EditorWidget.
  friend class IncSearch;

public:     // static data
  // Instances created minus instances destroyed.
  static int objectCount;

private:     // types
  // For this EditorWidget, and for a given TextDocumentFile, this is
  // the editing state for that file.  This state is *not* shared with
  // other widgets in the editor application, although it contains a
  // pointer to a TextDocumentFile, which *is* shared.
  class TextDocumentFileEditor : public TextDocumentEditor {
  public:    // data
    // Process-wide record of the open file.  Not an owner pointer.
    // Must not be null.
    TextDocumentFile *file;

  public:
    TextDocumentFileEditor(TextDocumentFile *f) :
      TextDocumentEditor(f),
      file(f)
    {}
  };

private:     // data
  // ------ child widgets -----
  // floating info box
  QLabel *infoBox;               // (nullable owner)

  // access to the status display
  StatusDisplay *status;         // (serf)

  // ------ editing state -----
  // Editor object for the file we are editing; it can be set to NULL when
  // the ctor is called, but must if so, must be set to a non-NULL
  // value via setDocumentFile() before any drawing or editing is done.
  //
  // When non-NULL, this points at one of the elements of 'm_editors'.
  TextDocumentFileEditor *editor;

  // All of the editors associated with this widget.  An editor is
  // created on demand when this widget is told to edit its underlying
  // file, so the set of files here is in general a subset of
  // GlobalState::documentFiles.
  ObjList<TextDocumentFileEditor> m_editors;

  // ----- match highlight state -----
  // when nonempty, any buffer text matching this string will
  // be highlighted in the 'hit' style; match is carried out
  // under influence of 'hitTextFlags'
  string hitText;
  TextDocumentEditor::FindStringFlags hitTextFlags;

public:      // data
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
  // current input proxy, if any
  InputProxy *inputProxy;           // (nullable serf)

  // ------ font metrics ------
  // these should be treated as read-only by all functions except
  // EditorWidget::setFonts()

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
  //     changes made in another widget.
  //   - nonfocusCursorLine/Col are valid/relevant/correct

  // true if I've registered myself as a listener; there
  // are a few cases where I don't have the focus, but
  // I also am not a listener (like initialization)
  bool listening;

  // Location of the cursor when I left it.
  //
  // Years later, I don't know what this was for...
  TextCoord nonfocusCursor;

  // ------ event model hacks ------
  // when this is true, we ignore the scrollToLine and scrollToCol
  // signals, to avoid recursion with the scroll bars
  bool ignoreScrollSignals;

private:     // funcs
  // set fonts, given actual BDF description data (*not* file names)
  void setFonts(char const *normal, char const *italic, char const *bold);

  // debugging
  static string lineColStr(TextCoord tc);
  string firstVisStr() const
    { return lineColStr(this->editor->firstVisible()); }
  string cursorStr() const
    { return lineColStr(this->editor->cursor()); }

protected:   // funcs
  // QWidget funcs
  virtual bool event(QEvent *e) override;
  virtual void paintEvent(QPaintEvent *) override;
  virtual void keyPressEvent(QKeyEvent *k) override;
  virtual void keyReleaseEvent(QKeyEvent *k) override;
  virtual void resizeEvent(QResizeEvent *r) override;
  virtual void mousePressEvent(QMouseEvent *m) override;
  virtual void mouseMoveEvent(QMouseEvent *m) override;
  virtual void mouseReleaseEvent(QMouseEvent *m) override;
  virtual void focusInEvent(QFocusEvent *e) override;
  virtual void focusOutEvent(QFocusEvent *e) override;

public:      // funcs
  EditorWidget(TextDocumentFile *docFile, StatusDisplay *status,
               QWidget *parent=NULL);
  ~EditorWidget();

  // Assert internal invariants.
  void selfCheck() const;

  // ---------------------------- cursor -----------------------------
  // ctrl-pageup/pagedown
  void cursorToTop();
  void cursorToBottom();

  // set cursorLine/cursorCol to the x/y derived from 'm'
  void setCursorToClickLoc(QMouseEvent *m);

  // Text cursor location (as opposed to mouse cursor image).
  TextCoord textCursor() const            { return editor->cursor(); }

  // These methods are here to help transition the code from having
  // cursor and mark in EditorWidget to having them in
  // TextDocumentEditor.
  //
  // TODO: I think I should remove these in favor of code directly
  // calling the editor object.  Anyway, most of the code that invokes
  // these should be moved into TextDocumentEditor.
  int cursorLine() const                  { return textCursor().line; }
  int cursorCol() const                   { return textCursor().column; }

  // absolute cursor movement
  void cursorTo(TextCoord tc);

  // relative cursor movement
  void moveCursorBy(int dline, int dcol)  { editor->moveRelCursor(dline, dcol); }
  void cursorLeftBy(int amt)              { moveCursorBy(0, -amt); }
  void cursorRightBy(int amt)             { moveCursorBy(0, +amt); }
  void cursorUpBy(int amt)                { moveCursorBy(-amt, 0); }
  void cursorDownBy(int amt)              { moveCursorBy(+amt, 0); }

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

  // ----------------------------- mark ------------------------------
  // selection manipulation
  void turnOffSelection();
  void turnOnSelection();
  void turnSelection(bool on);
  void clearSelIfEmpty();

  TextCoord mark() const                  { return editor->mark(); }
  void setMark(TextCoord tc)              { editor->setMark(tc); }
  bool selectEnabled() const              { return editor->markActive(); }
  void clearMark()                        { editor->clearMark(); }

  // get selected text, or "" if nothing selected
  string getSelectedText() const;

  // Select the entire line the cursor is on.
  void selectCursorLine();

  // --------------------------- scrolling -------------------------
  // helper
  int stcHelper(int firstVis, int lastVis, int cur, int gap);

  int firstVisibleLine() const            { return editor->firstVisible().line; }
  int firstVisibleCol() const             { return editor->firstVisible().column; }
  int lastVisibleLine() const             { return editor->lastVisible().line; }
  int lastVisibleCol() const              { return editor->lastVisible().column; }

  // Change the current firstVisibleLine/Col (calls updateView());
  // does *not* move the cursor.
  void setView(TextCoord newFirstVisible);
  void setFirstVisibleLine(int L)
    { this->setView(TextCoord(L, editor->firstVisible().column)); }
  void setFirstVisibleCol(int C)
    { this->setView(TextCoord(editor->firstVisible().line, C)); }

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

  // move the cursor and the view by a set increment, and repaint;
  // truncation at low end is automatic
  void moveViewAndCursor(int deltaLine, int deltaCol);

  // Number of fully visible lines/columns.  Part of the next
  // line/col may also be visible.
  int visLines() const { return this->editor->visLines(); }
  int visCols() const { return this->editor->visColumns(); }

  // --------------------------- insertion ---------------------------
  void insertAtCursor(char const *text);

  // fill document with whitespace, if necessary, so that the current
  // cursor position is on a valid character, i.e.:
  //   - cursorLine < docFile->numLines()
  //   - cursorCol <= docFile->lineLength(cursorLine)
  void fillToCursor();

  // --------------------------- deletion ----------------------------
  // given that the cursor is at the end of a line, join this line
  // with the next one
  void spliceNextLine();

  // Delete one character to the right of the cursor.
  void deleteCharAtCursor();

  // Delete any number of characters to the left of cursor, but not
  // more than exist in the document between its start and the cursor.
  void deleteAtCursor(int numChars);

  // Delete the character to the left of the cursor.  Or, if some
  // text is selected, delete that.
  void deleteLeftOfCursor();

  // -------------------- reformatting whitespace ---------------------
  // Indent or unindent selected lines.
  void blockIndent(int amt);

  // Justify lines near the cursor.
  void justifyNearCursorLine();

  // -------------------- interaction with files ------------------
  // nonfocus listening
  void startListening();
  void stopListening();

  // If we already have an editor for 'file', return it.  Otherwise,
  // make a new editor, add it to 'm_editors', and return that.
  TextDocumentFileEditor *getOrMakeEditor(TextDocumentFile *file);

  // Change which file this editor widget is editing.
  void setDocumentFile(TextDocumentFile *file);

  // 'file' is going away.  Remove all references to it.  If it is the
  // open file, pick another from the global list.
  void forgetAboutFile(TextDocumentFile *file);

  // Current file being edited.  This is 'const' because the file is
  // shared with other widgets in this process.
  TextDocumentFile *getDocumentFile() const;

  // Editor associated with current file and this particular widget.
  // This is not 'const' because the editor is owned by this object.
  TextDocumentEditor *getDocumentEditor();

  // ---------------------------- input -----------------------------
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

  // TextDocumentObserver funcs
  virtual void observeInsertLine(TextDocumentCore const &buf, int line) override;
  virtual void observeDeleteLine(TextDocumentCore const &buf, int line) override;
  virtual void observeInsertText(TextDocumentCore const &buf, TextCoord tc, char const *text, int length) override;
  virtual void observeDeleteText(TextDocumentCore const &buf, TextCoord tc, int length) override;

  // called by an input proxy when it detaches; I can
  // reset the mode pixmap then
  virtual void inputProxyDetaching();

  // -------------------------- output ----------------------------
  // intermediate paint step
  void updateFrame(QPaintEvent *ev, TextCoord drawnCursor);

  // Paint a single character at the given location.
  void drawOneChar(QPainter &paint, QtBDFFont *font, QPoint const &pt, char c);

  // Set 'curFont' and 'underline', plus the foreground
  // and background colors of 'paint', based on 'db' and 'cat'.
  void setDrawStyle(QPainter &paint,
                    QtBDFFont *&curFont, bool &underlining,
                    StyleDB *db, TextCategory cat);

  // show the info box near the cursor
  void showInfo(char const *info);

  // hide the info box if it's visible
  void hideInfo();

  // Offset from one baseline to the next, in pixels.
  int lineHeight() const { return fontHeight+interLineSpace; }

  // redraw widget, etc.; calls updateView() and viewChanged()
  void redraw();

  // ------------------------- clipboard --------------------------
  // Copy selected text to clipboard.  Clear the mark if
  // 'clearSelection'.
  void innerEditCopy(bool clearSelection);

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
