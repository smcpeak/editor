// editor-widget.h
// EditorWidget class.

#ifndef EDITOR_WIDGET_H
#define EDITOR_WIDGET_H

#include "inputproxy.h"                // InputProxy, InputPseudoKey
#include "td-editor.h"                 // TextDocumentEditor
#include "file-td.h"                   // FileTextDocument
#include "file-td-list.h"              // FileTextDocumentListObserver
#include "textcategory.h"              // TextCategory

// smqtutil
#include "qtguiutil.h"                 // unhandledExceptionMsgbox

// smbase
#include "owner.h"                     // Owner

// Qt
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
    public TextDocumentObserver,  // Watch my file while I don't have focus.
    public FileTextDocumentListObserver {        // Watch the list of files.
  Q_OBJECT

  // TODO: I currently need to let IncSearch access private members
  // 'docFile', 'selLowLine', etc.  I think IncSearch should be able
  // to operate on top of TextFileEditor instead of EditorWidget.
  friend class IncSearch;

public:     // static data
  // Instances created minus instances destroyed.
  static int s_objectCount;

private:     // types
  // For this EditorWidget, and for a given FileTextDocument, this is
  // the editing state for that file.  This state is *not* shared with
  // other widgets in the editor application, although it contains a
  // pointer to a FileTextDocument, which *is* shared.
  class FileTextDocumentEditor : public TextDocumentEditor {
  public:    // data
    // Process-wide record of the open file.  Not an owner pointer.
    // Must not be null.
    FileTextDocument *m_fileDoc;

  public:
    FileTextDocumentEditor(FileTextDocument *f) :
      TextDocumentEditor(f),
      m_fileDoc(f)
    {}
  };

private:     // data
  // ------ child widgets -----
  // floating info box
  QLabel *m_infoBox;               // (nullable owner)

  // access to the status display
  StatusDisplay *m_status;         // (serf)

  // ------ editing state -----
  // Editor object for the file we are editing.  Never NULL.
  // This points at one of the elements of 'm_editors'.
  FileTextDocumentEditor *m_editor;

  // All of the editors associated with this widget.  An editor is
  // created on demand when this widget is told to edit its underlying
  // file, so the set of files here is in general a subset of
  // GlobalState::documentFiles.
  ObjList<FileTextDocumentEditor> m_editorList;

  // The list of documents we can potentially switch among.  This is
  // needed so I can switch to a different file when the open file gets
  // closed by another widget operating on the same document list.
  FileTextDocumentList *m_documentList;

  // ----- match highlight state -----
  // when nonempty, any buffer text matching this string will
  // be highlighted in the 'hit' style; match is carried out
  // under influence of 'hitTextFlags'
  string m_hitText;
  TextDocumentEditor::FindStringFlags m_hitTextFlags;

public:      // data
  // ------ rendering options ------
  // amount of blank space at top/left edge of widget
  int m_topMargin, m_leftMargin;

  // # of blank lines of pixels between lines of text ("leading")
  int m_interLineSpace;

  // colors
  QColor m_cursorColor;                // color of text cursor

  // Fonts for (indexed by) each text category; all fonts must use the
  // same character size; some entries may be NULL, indicating we
  // do not expect to draw text with that category.
  ObjArrayStack<QtBDFFont> m_fontForCategory;

  // Font for drawing the character under the cursor, indexed by
  // the FontVariant (modulo FV_UNDERLINE) there.
  ObjArrayStack<QtBDFFont> m_cursorFontForFV;

  // Font containing miniature hexadecimal characters for use when
  // a glyph is missing.
  Owner<QtBDFFont> m_minihexFont;

  // When true, draw visible markers on whitespace characters.
  bool m_visibleWhitespace;

  // Value in [0,255], where 255 is fully opaque.
  int m_whitespaceOpacity;

  // True to draw trailing whitespace with 'm_trailingWhitespaceBgColor'.
  bool m_highlightTrailingWhitespace;

  // Overlay color for trailing whitespace, including its alpha.
  QColor m_trailingWhitespaceBgColor;

  // Column number for a soft margin.  This is used for text
  // justification and optionally to draw a margin line.
  int m_softMarginColumn;

  // True to draw the soft margin.
  bool m_visibleSoftMargin;

  // Color of the line indicating the soft margin.
  QColor m_softMarginColor;

  // ------ input options ------
  // current input proxy, if any
  InputProxy *m_inputProxy;           // (nullable serf)

  // ------ font metrics ------
  // these should be treated as read-only by all functions except
  // EditorWidget::setFonts()

  // number of pixels in a character cell that are above the
  // base line, including the base line itself
  int m_fontAscent;

  // number of pixels below the base line, not including the
  // base line
  int m_fontDescent;

  // total # of pixels in each cell
  int m_fontHeight;
  int m_fontWidth;

  // ------ stuff for when I don't have focus ------
  // True if I've registered myself as a listener.  There
  // are a few cases where I don't have the focus, but
  // I also am not a listener (like initialization).
  bool m_listening;

  // ------ event model hacks ------
  // when this is true, we ignore the scrollToLine and scrollToCol
  // signals, to avoid recursion with the scroll bars
  bool m_ignoreScrollSignals;

private:     // funcs
  // set fonts, given actual BDF description data (*not* file names)
  void setFonts(char const *normal, char const *italic, char const *bold);

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
  EditorWidget(FileTextDocument *docFile,
               FileTextDocumentList *documentList,
               StatusDisplay *status,
               QWidget *parent=NULL);
  ~EditorWidget();

  // Assert internal invariants.
  void selfCheck() const;

  // ---------------------------- cursor -----------------------------
  // set cursorLine/cursorCol to the x/y derived from 'm'
  void setCursorToClickLoc(QMouseEvent *m);

  // These methods are here to help transition the code from having
  // cursor and mark in EditorWidget to having them in
  // TextDocumentEditor.
  //
  // TODO: I think I should remove these in favor of code directly
  // calling the editor object.  Anyway, most of the code that invokes
  // these should be moved into TextDocumentEditor.
  int cursorLine() const                  { return m_editor->cursor().line; }
  int cursorCol() const                   { return m_editor->cursor().column; }

  // absolute cursor movement
  void cursorTo(TextCoord tc);

  // relative cursor movement
  void moveCursorBy(int dline, int dcol)  { m_editor->moveCursorBy(dline, dcol); }
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
  void turnOffSelection()                 { m_editor->clearMark(); }
  void turnOnSelection()                  { m_editor->turnOnSelection(); }
  void turnSelection(bool on)             { m_editor->turnSelection(on); }

  TextCoord mark() const                  { return m_editor->mark(); }
  void setMark(TextCoord tc)              { m_editor->setMark(tc); }
  bool selectEnabled() const              { return m_editor->markActive(); }
  void clearMark()                        { m_editor->clearMark(); }

  string getSelectedText() const          { return m_editor->getSelectedText(); }

  void selectCursorLine()                 { m_editor->selectCursorLine(); }

  // --------------------------- scrolling -------------------------
  // Refactoring transition compatibility functions.
  int firstVisibleLine() const            { return m_editor->firstVisible().line; }
  int firstVisibleCol() const             { return m_editor->firstVisible().column; }
  int lastVisibleLine() const             { return m_editor->lastVisible().line; }
  int lastVisibleCol() const              { return m_editor->lastVisible().column; }

  void setFirstVisible(TextCoord fv)      { m_editor->setFirstVisible(fv); }
  void setFirstVisibleLine(int L)         { m_editor->setFirstVisibleLine(L); }
  void setFirstVisibleCol(int C)          { m_editor->setFirstVisibleCol(C); }

  // This one calls redraw, whereas the preceding does not, simply
  // because its call sites all want that right after.
  void moveFirstVisibleAndCursor(int deltaLine, int deltaCol)
    { m_editor->moveFirstVisibleAndCursor(deltaLine, deltaCol); redraw(); }

  // recompute lastVisibleLine/Col, based on:
  //   - firstVisibleLine/Col
  //   - width(), height()
  //   - topMargin, leftMargin, interLineSpace
  //   - fontHeight, fontWidth
  void recomputeLastVisible();

  // scroll the view the minimum amount so that the cursor line/col
  // is visible; if it's already visible, do nothing; 'edgeGap' says
  // how many lines/cols of extra space on the far side of the cursor
  // we want; if 'edgeGap' is -1, then if the cursor isn't already visible,
  // center the viewport
  void scrollToCursor(int edgeGap=0);

  // Number of fully visible lines/columns.  Part of the next
  // line/col may also be visible.
  int visLines() const { return m_editor->visLines(); }
  int visCols() const { return m_editor->visColumns(); }

  // -------------------- reformatting whitespace ---------------------
  // Indent or unindent selected lines.
  void blockIndent(int amt);

  // -------------------- interaction with files ------------------
  // nonfocus listening
  void startListening();
  void stopListening();

  // If we already have an editor for 'file', return it.  Otherwise,
  // make a new editor, add it to 'm_editors', and return that.
  FileTextDocumentEditor *getOrMakeEditor(FileTextDocument *file);

  // Change which file this editor widget is editing.
  void setDocumentFile(FileTextDocument *file);

  // The editor has just acquired focus or switched to a new file.
  // Check if the file has been edited while we were away.
  void checkForDiskChanges();

  // 'file' is going away.  Remove all references to it.  If it is the
  // open file, pick another from the document list.
  void fileTextDocumentRemoved(
    FileTextDocumentList *documentList, FileTextDocument *file) override;

  // Current file being edited.  This is 'const' because the file is
  // shared with other widgets in this process.
  FileTextDocument *getDocumentFile() const;

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
  virtual void observeInsertLine(TextDocumentCore const &buf, int line) noexcept override;
  virtual void observeDeleteLine(TextDocumentCore const &buf, int line) noexcept override;
  virtual void observeInsertText(TextDocumentCore const &buf, TextCoord tc, char const *text, int length) noexcept override;
  virtual void observeDeleteText(TextDocumentCore const &buf, TextCoord tc, int length) noexcept override;
  virtual void observeTotalChange(TextDocumentCore const &buf) noexcept override;
  virtual void observeUnsavedChangesChange(TextDocument const *doc) noexcept override;

  // called by an input proxy when it detaches; I can
  // reset the mode pixmap then
  virtual void inputProxyDetaching();

  // -------------------------- output ----------------------------
  // intermediate paint step
  void updateFrame(QPaintEvent *ev);

  // Paint a single character at the given location.
  void drawOneChar(QPainter &paint, QtBDFFont *font,
    QPoint const &pt, char c, bool withinTrailingWhitespace);

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
  int lineHeight() const { return m_fontHeight+m_interLineSpace; }

  // redraw widget, etc.; calls updateView() and viewChanged()
  void redraw();

  void printUnhandled(xBase const &x)
    { unhandledExceptionMsgbox(this, x); }

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
  // Emitted when some aspect of the document that is shown outside the
  // widget's own rectangle changes: scroll viewport, cursor location,
  // file name, whether there are unsaved changes, etc.  In practice,
  // this gets emitted on almost every keystroke because of the cursor
  // location change.
  void viewChanged();
};

#endif // EDITOR_WIDGET_H
