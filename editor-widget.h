// editor-widget.h
// EditorWidget class.

#ifndef EDITOR_WIDGET_H
#define EDITOR_WIDGET_H

#include "td-editor.h"                 // TextDocumentEditor
#include "named-td.h"                  // NamedTextDocument
#include "named-td-list.h"             // NamedTextDocumentListObserver
#include "textcategory.h"              // TextCategory

// smqtutil
#include "qtguiutil.h"                 // unhandledExceptionMsgbox

// smbase
#include "owner.h"                     // Owner
#include "refct-serf.h"                // RCSerf

// Qt
#include <qwidget.h>                   // QWidget

class QtBDFFont;                       // qtbdffont.h
class SearchAndReplacePanel;           // sar-panel.h
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
//
// This class indirectly inherits SerfRefCount through each of the
// Observer interfaces.  The inheritance is virtual, so there is just
// one copy of the reference count.
class EditorWidget
  : public QWidget,               // I am a Qt widget.
    public TextDocumentObserver,  // Watch my file while I don't have focus.
    public NamedTextDocumentListObserver {        // Watch the list of files.
  Q_OBJECT

  // Currently, SAR needs access to m_editor, and I am torn about
  // whether that is acceptable.
  friend class SearchAndReplacePanel;

public:     // static data
  // Instances created minus instances destroyed.
  static int s_objectCount;

private:     // types
  // For this EditorWidget, and for a given NamedTextDocument, this is
  // the editing state for that document.  This state is *not* shared with
  // other widgets in the editor application, although it contains a
  // pointer to a NamedTextDocument, which *is* shared.
  class NamedTextDocumentEditor : public TextDocumentEditor {
  public:    // data
    // Process-wide record of the open file.  Not an owner pointer.
    // Must not be null.
    RCSerf<NamedTextDocument> m_namedDoc;

  public:
    NamedTextDocumentEditor(NamedTextDocument *d) :
      TextDocumentEditor(d),
      m_namedDoc(d)
    {}
  };

private:     // data
  // ------ widgets -----
  // floating info box
  QLabel *m_infoBox;               // (nullable owner)

  // access to the status display
  StatusDisplay *m_status;         // (serf)

  // ------ editing state -----
  // All of the editors associated with this widget.  An editor is
  // created on demand when this widget is told to edit its underlying
  // file, so the set of files here is in general a subset of
  // GlobalState::documentFiles.
  ObjList<NamedTextDocumentEditor> m_editorList;

  // Editor object for the file we are editing.  Never NULL.
  // This points at one of the elements of 'm_editors'.
  //
  // Now that this is an RCSerf, it is important that it be declared
  // after 'm_editorList' so it is destroyed first.
  RCSerf<NamedTextDocumentEditor> m_editor;

  // The list of documents we can potentially switch among.  This is
  // needed so I can switch to a different file when the open file gets
  // closed by another widget operating on the same document list.
  RCSerf<NamedTextDocumentList> m_documentList;

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

  // Overlay color for trailing whitespace, including its alpha.
  //
  // This is only used when drawing a document whose
  // 'm_highlightTrailingWhitespace' flag is true.
  QColor m_trailingWhitespaceBgColor;

  // Column number for a soft margin.  This is used for text
  // justification and optionally to draw a margin line.
  int m_softMarginColumn;

  // True to draw the soft margin.
  bool m_visibleSoftMargin;

  // Color of the line indicating the soft margin.
  QColor m_softMarginColor;

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
  //
  // TODO: This should be removed.  I have discovered that the Qt focus
  // notifications are unreliable, so I should not use them for such an
  // important function.
  bool m_listening;

  // When true, ignore any TextDocumentObserver notifications because I
  // initiated the change.
  bool m_ignoreTextDocumentNotifications;

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
  EditorWidget(NamedTextDocument *docFile,
               NamedTextDocumentList *documentList,
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
  //
  // Hmmm, m_editor is private.  Maybe I just want to expose the same
  // interface then?
  int cursorLine() const                  { return m_editor->cursor().line; }
  int cursorCol() const                   { return m_editor->cursor().column; }

  // absolute cursor movement
  void cursorTo(TextCoord tc);

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
  TextCoord mark() const                  { return m_editor->mark(); }
  bool selectEnabled() const              { return m_editor->markActive(); }
  string getSelectedText() const          { return m_editor->getSelectedText(); }

  // --------------------------- scrolling -------------------------
  // Refactoring transition compatibility functions.
  int firstVisibleLine() const            { return m_editor->firstVisible().line; }
  int firstVisibleCol() const             { return m_editor->firstVisible().column; }
  int lastVisibleLine() const             { return m_editor->lastVisible().line; }
  int lastVisibleCol() const              { return m_editor->lastVisible().column; }

  // Move both the screen and cursor by the same amount.
  void moveFirstVisibleAndCursor(int deltaLine, int deltaCol);

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

  // --------------------------- matches ----------------------------
  // Change the match string.  If 'scrollToHit', also scroll the view
  // so the first match is visible.  But, do not change the cursor.
  void setHitText(string const &t, bool scrollToHit);

  // Move the cursor to the next/prev instance of matching text.
  void nextSearchHit(bool reverse);

  // Replace the current match with the given text.
  void replaceSearchHit(string const &t);

  // Return true if the currently selected text is a search hit.
  bool searchHitSelected() const;

  // Begin closing the SAR panel from the widget side, then emit a
  // signal that will finish the job from the window side.
  void doCloseSARPanel();

  // ---------------------- document changes ----------------------
  // Indent or unindent selected lines.
  void blockIndent(int amt);

  // Insert text at cursor, overwriting selection if active.
  void insertText(char const *text, int length);

  // -------------------- interaction with files ------------------
  // nonfocus listening
  void startListening();
  void stopListening();

  // If we already have an editor for 'file', return it.  Otherwise,
  // make a new editor, add it to 'm_editors', and return that.
  NamedTextDocumentEditor *getOrMakeEditor(NamedTextDocument *file);

  // Change which file this editor widget is editing.
  void setDocumentFile(NamedTextDocument *file);

  // The editor has just acquired focus or switched to a new file.
  // Check if the file has been edited while we were away.
  void checkForDiskChanges();

  // 'file' is going away.  Remove all references to it.  If it is the
  // open file, pick another from the document list.
  virtual void namedTextDocumentRemoved(
    NamedTextDocumentList *documentList, NamedTextDocument *file) noexcept override;

  // Answer a query from the NamedTextDocumentList.
  virtual bool getNamedTextDocumentInitialView(
    NamedTextDocumentList *documentList, NamedTextDocument *file,
    NamedTextDocumentInitialView /*OUT*/ &view) noexcept override;

  // Current file being edited.  This is 'const' because the file is
  // shared with other widgets in this process, so the constness of
  // this object does not propagate to it.
  NamedTextDocument *getDocumentFile() const;

  // Get what the user thinks of as the "current directory" for this
  // widget.  Normally that is the directory containing the current
  // document, although under some circumstances the process working
  // directory may be used.  This contains a final path separator.
  string getDocumentDirectory() const;

  // Editor associated with current file and this particular widget.
  // This is not 'const' because the editor is owned by this object.
  TextDocumentEditor *getDocumentEditor();

  // Scan the text near the cursor to try to find the name of a file,
  // and open it in the editor.
  void fileOpenAtCursor();

  // ---------------------------- input -----------------------------
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

  // This is the same as 'keyPressEvent', but is meant to be callable by
  // other classes in order to pass on a key event that would otherwise
  // have been intercepted by something in the Qt infrastructure.  This
  // is *not* meant to be abused to simulate keystrokes!  Instead,
  // methods with higher-level purpose should be defined and used.
  void rescuedKeyPressEvent(QKeyEvent *k);

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

  // True if trailing whitespace should be shown.
  bool highlightTrailingWhitespace() const;

  // Toggle whether trailing whitespace should be shown for this document.
  void toggleHighlightTrailingWhitespace();

  // redraw widget, etc.; calls updateView() and viewChanged()
  void redraw();

  void printUnhandled(xBase const &x)
    { unhandledExceptionMsgbox(this, x); }

  // Edit menu functions.
  void editUndo();
  void editRedo();
  void editCut();
  void editCopy();
  void editPaste();
  void editDelete();
  void editKillLine();

public Q_SLOTS:
  // slots to respond to scrollbars
  void scrollToLine(int line);
  void scrollToCol(int col);

signals:
  // Emitted when some aspect of the document that is shown outside the
  // widget's own rectangle changes: scroll viewport, cursor location,
  // file name, whether there are unsaved changes, etc.  In practice,
  // this gets emitted on almost every keystroke because of the cursor
  // location change.
  void viewChanged();

  // Emitted when the user indicates, via some widget functionality,
  // that they want to open a particular file.  It should open a dialog
  // to allow the user to further edit the name.  If 'line' is non-zero,
  // it indicates where in 'filename' we want to go, as a 1-based line
  // number.
  void openFilenameInputDialogSignal(QString const &filename, int line);

  // The user wants to close the search and replace panel if it is open.
  void closeSARPanel();
};

#endif // EDITOR_WIDGET_H
