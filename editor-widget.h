// editor-widget.h
// EditorWidget class.

#ifndef EDITOR_WIDGET_H
#define EDITOR_WIDGET_H

// editor
#include "diagnostic-element-fwd.h"              // DiagnosticElement [n]
#include "editor-command.ast.gen.fwd.h"          // EditorCommand
#include "editor-global-fwd.h"                   // EditorGlobal
#include "editor-navigation-options.h"           // EditorNavigationOptions
#include "editor-settings-fwd.h"                 // EditorSettings
#include "editor-window-fwd.h"                   // EditorWindow
#include "event-replay.h"                        // EventReplayQueryable
#include "host-file-and-line-opt.h"              // HostFileAndLineOpt
#include "line-difference.h"                     // LineDifference
#include "lsp-manager-fwd.h"                     // LSPManager
#include "lsp-symbol-request-kind.h"             // LSPSymbolRequestKind
#include "lsp-version-number-fwd.h"              // LSP_VersionNumber [n]
#include "named-td-list.h"                       // NamedTextDocumentListObserver
#include "named-td-editor.h"                     // NamedTextDocumentEditor
#include "named-td.h"                            // NamedTextDocument
#include "styledb-fwd.h"                         // TextCategoryAndStyle
#include "td-editor.h"                           // TextDocumentEditor
#include "text-search.h"                         // TextSearch
#include "textcategory.h"                        // TextCategory, LineCategories
#include "vfs-connections.h"                     // VFS_Connections

// smqtutil
#include "smqtutil/qtbdffont-fwd.h"              // QtBDFFont
#include "smqtutil/qtguiutil.h"                  // unhandledExceptionMsgbox

// smbase
#include "smbase/exc.h"                          // smbase::XBase
#include "smbase/refct-serf.h"                   // RCSerf, SerfRefCount
#include "smbase/sm-noexcept.h"                  // NOEXCEPT
#include "smbase/std-memory-fwd.h"               // std::unique_ptr
#include "smbase/std-optional-fwd.h"             // std::optional
#include "smbase/std-string-fwd.h"               // std::string
#include "smbase/std-string-view-fwd.h"          // std::string_view

// Qt
#include <QRect>
#include <QWidget>

// libc++
#include <functional>                            // std::function
#include <memory>                                // std::unique_ptr
#include <optional>                              // std::optional


class QtBDFFont;                                 // qtbdffont.h
class SearchAndReplacePanel;                     // sar-panel.h
class StyleDB;                                   // styledb.h

class QImage;
class QLabel;
class QRangeControl;


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
      // Note: `TextDocumentObserver` inherits `SerfRefCount`, so that
      // allows `RCSerf`s to point to `EditorWidget`.
    public NamedTextDocumentListObserver,        // Watch the list of files.
    public EventReplayQueryable { // Respond to test queries.
  Q_OBJECT

  // Currently, SAR needs access to m_editor, and I am torn about
  // whether that is acceptable.
  friend class SearchAndReplacePanel;

public:      // types
  // Things we can do with one file and LSP.
  enum LSPFileOperation {
    LSPFO_OPEN_OR_UPDATE,    // Open or update the file.
    LSPFO_UPDATE_IF_OPEN,    // Update if it is open.
    LSPFO_CLOSE,             // Close the file.
  };

public:     // static data
  // Instances created minus instances destroyed.
  static int s_objectCount;

  // Have all editor widgets ignore change notifications.  This is used
  // when refreshing file contents to avoid disturbing the cursor
  // position.
  static bool s_ignoreTextDocumentNotificationsGlobally;

private:     // data
  // ------ widgets -----
  // Containing editor window, through which file system access is
  // performed.
  RCSerf<EditorWindow> m_editorWindow;

  // floating info box
  QLabel *m_infoBox;               // (nullable owner)

  // These are hidden labels that hold the search match text shown in
  // the corners.  Storing the text here allows the automated test
  // infrastructure easy access to it.  The objects are owned by 'this'
  // via the QObject parent system.

  // Contains text showing the number of matches offscreen above.
  QLabel *m_matchesAboveLabel;

  // Number offscreen below.
  QLabel *m_matchesBelowLabel;

  // ------ editing state -----
  // All of the editors associated with this widget.  An editor is
  // created on demand when this widget is told to edit its underlying
  // file, so the set of files here is in general a subset of
  // EditorGlobal::documentFiles.
  ObjList<NamedTextDocumentEditor> m_editorList;

  // Editor object for the file we are editing.  Never NULL.
  // This points at one of the elements of 'm_editors'.
  //
  // Now that this is an RCSerf, it is important that it be declared
  // after 'm_editorList' so it is destroyed first.
  RCSerf<NamedTextDocumentEditor> m_editor;

  // ------ file modification query ------
  // If not zero, the ID of the outstanding request for the file status
  // of the document being edited.
  VFS_Connections::RequestID m_fileStatusRequestID;

  // The editor whose status was requested, if any.
  //
  // I am choosing to maintain this as an extra layer of safety in case
  // somewhere I change 'm_editor' without canceling the request.
  //
  // Invariant: (m_fileStatusRequestID == 0) ==
  //            (m_fileStatusRequestEditor == nullptr)
  //
  // Invariant: m_fileStatusRequestEditor == nullptr ||
  //            m_fileStatusRequestEditor == m_editor
  RCSerf<NamedTextDocumentEditor> m_fileStatusRequestEditor;

  // ------ match highlight state ------
  // when nonempty, any buffer text matching this string will
  // be highlighted in the 'hit' style; match is carried out
  // under influence of 'hitTextFlags'
  string m_hitText;
  TextSearch::SearchStringFlags m_hitTextFlags;

  // Incremental search algorithm object.  Never null.
  //
  // Its 'searchText' and flags are kept in sync with 'm_hitText' and
  // 'm_hitTextFlags', and its document is always
  // 'm_editor->getDocumentCore()'.
  std::unique_ptr<TextSearch> m_textSearch;

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
  // a glyph is missing.  Never null.
  std::unique_ptr<QtBDFFont> m_minihexFont;

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

  // Compute and emit 'signal_searchStatusIndicator'.
  void emitSearchStatusIndicator();

  // Store into 'label' the text to show in the corner of the widget.
  void computeOffscreenMatchIndicator(QLabel *label, int numMatches);

  // Compute both corner labels.
  void computeOffscreenMatchIndicators();

  // Draw the digits in the corner that indicate the number of search
  // matches that are off the screen.
  void drawOffscreenMatchIndicators(QPainter &paint);

  // Draw one of the labels that goes into one of the corners.
  void drawOneCornerLabel(QPainter &paint, QtBDFFont *font,
                          bool isLeft, bool isTop, QString const &text);

  // Add the search hits for 'line' to 'categories.
  void addSearchMatchesToLineCategories(
    LineCategories &categories, LineIndex line);

  // Copy 'm_hitText' and 'm_hitTextFlags' into 'm_textSearch'.
  void setTextSearchParameters();

  // Scroll the display to the next/prev search it.  If found, return
  // true and, if also 'select', set the cursor/mark to select the
  // match.
  bool scrollToNextSearchHit(bool reverse, bool select);

  // Wait, possibly with an activity dialog, until `condition` is true.
  // Return false if the wait is canceled.
  bool lspSynchronouslyWaitUntil(
    std::function<bool()> condition,
    std::string const &activityDialogMessage);

  // Show the diagnostic details dialog, populated with `elts`.
  void showDiagnosticDetailsDialog(
    QVector<DiagnosticElement> &&elts);

  // An attempt to parse `gdvReply` in response to `lsrk` yielded
  // exception `x`.  Log the details and warn the user.
  void logAndWarnFailedLocationReply(
    gdv::GDValue const &gdvReply,
    LSPSymbolRequestKind lsrk,
    smbase::XBase &x);

  // The user is trying to make a change to a read-only document.
  // Prompt to override the flag.
  bool promptOverrideReadOnly();

  // True if we are currently ignoring document change notifications.
  bool ignoringChangeNotifications() const;

  // Get the `y` coordinate of the origin point for drawing characters
  // within a rectangle for one line of text.
  int getBaselineYCoordWithinLine() const;

  // Get the height of a "full" line in pixels, meaning the font height
  // plus the blank space inserted between lines.
  int getFullLineHeight() const;

protected:   // funcs
  // QWidget funcs
  virtual void resizeEvent(QResizeEvent *r) NOEXCEPT OVERRIDE;
  virtual void paintEvent(QPaintEvent *) NOEXCEPT OVERRIDE;
  virtual void keyPressEvent(QKeyEvent *k) NOEXCEPT OVERRIDE;
  virtual void keyReleaseEvent(QKeyEvent *k) NOEXCEPT OVERRIDE;
  virtual void mouseMoveEvent(QMouseEvent *m) NOEXCEPT OVERRIDE;
  virtual void mousePressEvent(QMouseEvent *m) NOEXCEPT OVERRIDE;
  virtual void mouseReleaseEvent(QMouseEvent *m) NOEXCEPT OVERRIDE;
  virtual void focusInEvent(QFocusEvent *e) NOEXCEPT OVERRIDE;
  virtual void focusOutEvent(QFocusEvent *e) NOEXCEPT OVERRIDE;

public:      // funcs
  EditorWidget(NamedTextDocument *docFile,
               EditorWindow *editorWindow);
  ~EditorWidget();

  // Assert internal invariants.
  void selfCheck() const;

  bool isReadOnly() const              { return m_editor->isReadOnly(); }
  void setReadOnly(bool readOnly);

  // Get the parent editor window.
  EditorWindow *editorWindow() const;

  // Get the global editor state.
  EditorGlobal *editorGlobal() const;

  // User settings.
  EditorSettings const &editorSettings() const;

  // Global LSP manager, read-only.  Writes have to go through
  // `editorGlobal()`.
  LSPManager const *lspManagerC() const;

  // Read the font choice stored in 'editorGlobal()' and set this
  // widget's editor fonts accordingly.
  void setFontsFromEditorGlobal();

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
  LineIndex cursorLine() const            { return m_editor->cursor().m_line; }
  int cursorCol() const                   { return m_editor->cursor().m_column; }

  // Return the cursor position as "<line>:<col>" where both are 1-based
  // for the user interface.
  std::string cursorPositionUIString() const;

  // absolute cursor movement
  void cursorTo(TextLCoord tc);

  // Things often bound to cursor, etc. keys; 'shift' indicates whether
  // the shift key is held (so the selection should be turned on, or
  // remain on).
  //
  // These, and some other, methods begin with "command" to indicate
  // that they create an `EditorCommand` object and dispatch that rather
  // than performing the operation directly.
  //
  void commandCursorLeft(bool shift);
  void commandCursorRight(bool shift);
  void commandCursorHome(bool shift);
  void commandCursorEnd(bool shift);
  void commandCursorUp(bool shift);
  void commandCursorDown(bool shift);
  void commandCursorPageUp(bool shift);
  void commandCursorPageDown(bool shift);
  void commandCursorToEndOfNextLine(bool shift);

  // Set up the scroll state and cursor position for a newly created
  // process output document.
  void initCursorForProcessOutput();

  // ----------------------------- mark ------------------------------
  TextLCoord mark() const                 { return m_editor->mark(); }
  bool selectEnabled() const              { return m_editor->markActive(); }
  void clearMark()                        { m_editor->clearMark(); }
  string getSelectedText() const          { return m_editor->getSelectedText(); }
  string getSelectedOrIdentifier() const  { return m_editor->getSelectedOrIdentifier(); }

  // --------------------------- scrolling -------------------------
  // Refactoring transition compatibility functions.
  LineIndex firstVisibleLine() const      { return m_editor->firstVisible().m_line; }
  int firstVisibleCol() const             { return m_editor->firstVisible().m_column; }
  LineIndex lastVisibleLine() const       { return m_editor->lastVisible().m_line; }
  int lastVisibleCol() const              { return m_editor->lastVisible().m_column; }

  // Move both the screen and cursor by the same amount.
  void commandMoveFirstVisibleAndCursor(
    LineDifference deltaLine, int deltaCol);

  // recompute lastVisibleLine/Col, based on:
  //   - firstVisibleLine/Col
  //   - width(), height()
  //   - topMargin, leftMargin, interLineSpace
  //   - fontHeight, fontWidth
  //
  // If we do not have font information, this does not set the size, and
  // return false.  Returns true otherwise.
  bool recomputeLastVisible();

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

  // Number of columns that are either fully or partially visible.  (We
  // just assume there is always one partially-visible column, since
  // being wrong just means a little wasted drawing time.)
  int visColsPlusPartial() const { return visCols() + 1; }

  // --------------------------- matches ----------------------------
  // Change the match string and flags.  If 'scrollToHit', also scroll
  // the view so the first match is visible.  But, do not change the
  // cursor.
  void setSearchStringParams(string const &searchString,
    TextSearch::SearchStringFlags flags, bool scrollToHit);

  // Move the cursor to the next/prev instance of matching text.  Return
  // true if we found one to move to.
  bool nextSearchHit(bool reverse);

  // Replace the currently selected match with the given replacement
  // specification string.  This should only be called when
  // 'searchHitSelected()' is true.
  void replaceSearchHit(string const &replaceSpec);

  // Return true if the currently selected text is a search hit.
  bool searchHitSelected() const;

  // Begin closing the SAR panel from the widget side, then emit a
  // signal that will finish the job from the window side.
  void doCloseSARPanel();

  // ---------------------- document changes ----------------------
  // Indent or unindent selected lines.
  void commandBlockIndent(int amt);

  // My standard indentation amounts.  TODO: Make configurable.
  void commandEditRigidIndent()   { this->commandBlockIndent(+2); }
  void commandEditRigidUnindent() { this->commandBlockIndent(-2); }

  // Justify paragraph the cursor is on or paragraphs that are selected.
  void editJustifyParagraph();

  // Insert current date/time at cursor.
  void editInsertDateTime();

  // Insert text at cursor, overwriting selection if active.
  void insertText(char const *text, int length);

  // Edit menu functions.
  void editUndo();
  void editRedo();
  void commandEditCut();
  void commandEditCopy();
  void commandEditPaste(bool cursorToStart);
  void commandEditDelete();
  void commandEditKillLine();
  void commandEditSelectEntireFile();
  void editGrepSource();

  // ------------------------------- LSP -------------------------------
  // Get the version number of the current document as an
  // `LSP_VersionNumber`, which is what we need for LSP.  If the version
  // number cannot be converted to that type (because it is too big),
  // pop up an error box if `wantErrors`, then return nullopt.
  std::optional<LSP_VersionNumber> lspGetDocVersionNumber(
    bool wantErrors) const;

  // If the LSP server is currently initializing, wait until that is not
  // the case (it succeeds or it fails), or the user cancels.  Return
  // true if the server changed state and false if the user canceled the
  // wait.  If the server is *not* initializing, just return true
  // immediately.
  bool lspWaitUntilNotInitializing();

  // Do `operation` with the current file.
  void lspDoFileOperation(LSPFileOperation operation);

  // If there are diagnostics associated with the current document, and
  // the cursor is in one of the marked ranges, show its message and
  // return nullopt.  Otherwise, return a string explaining the issue.
  //
  // The dialog that appears can be used to navigate to the relevant
  // lines; the navigation takes place in a widget that depends on
  // `opts`.
  std::optional<std::string> lspShowDiagnosticAtCursor(
    EditorNavigationOptions opts);

  // Move the cursor to the next or previous diagnostic message.  Do
  // nothing if we have no diagnostics or there are no more in the
  // indicated direction.
  void lspGoToAdjacentDiagnostic(bool next);

  // Go to a location related to the symbol at the cursor.
  void lspGoToRelatedLocation(
    LSPSymbolRequestKind lsrk,
    EditorNavigationOptions options = EditorNavigationOptions::ENO_NORMAL);

  // Handle the reply to a request for a location.
  void lspHandleLocationReply(
    gdv::GDValue const &gdvReply,
    LSPSymbolRequestKind lsrk);

  // Handle the reply to a request for symbol hover information.
  void lspHandleHoverInfoReply(
    gdv::GDValue const &gdvReply);

  // Handle the reply to a request for completion.
  void lspHandleCompletionReply(
    gdv::GDValue const &gdvReply);

  // -------------------- interaction with files ------------------
  // Get the connections interface object.
  VFS_Connections *vfsConnections() const;

  // nonfocus listening
  void startListening();
  void stopListening();

  // If we already have an editor for 'file', return it.  Otherwise,
  // make a new editor, add it to 'm_editors', and return that.
  NamedTextDocumentEditor *getOrMakeEditor(NamedTextDocument *file);

  // Change which file this editor widget is editing.
  void setDocumentFile(NamedTextDocument *file);

  // Asynchronously issue a request for the file status if the file
  // being edited in order to get an updated file modification time.
  void requestFileStatus();

  // Cancel any outstanding file status request.
  void cancelFileStatusRequestIfAny();

  // 'file' is going away.  Remove all references to it.  If it is the
  // open file, pick another from the document list.
  virtual void namedTextDocumentRemoved(
    NamedTextDocumentList const *documentList,
    NamedTextDocument *file) NOEXCEPT OVERRIDE;

  // Answer a query from the NamedTextDocumentList.
  virtual bool getNamedTextDocumentInitialView(
    NamedTextDocumentList const *documentList,
    NamedTextDocument *file,
    NamedTextDocumentInitialView /*OUT*/ &view) NOEXCEPT OVERRIDE;

  // Current document being edited.  This is 'const' because the file is
  // shared with other widgets in this process, so the constness of
  // this object does not propagate to it.
  NamedTextDocument *getDocument() const;

  // True if the document has a file name, and the host name is still
  // valid in 'vfsConnections()'.
  bool hasValidFileAndHostName() const;

  // Get what the user thinks of as the "current directory" for this
  // widget.  Normally that is the directory containing the current
  // document, although under some circumstances the process working
  // directory may be used.  This contains a final path separator.
  string getDocumentDirectory() const;

  // Like 'getDocumentDirectory()', but including the host name.
  HostAndResourceName getDocumentDirectoryHarn() const;

  // Editor associated with current file and this particular widget.
  // This is not 'const' because the editor is owned by this object.
  TextDocumentEditor *getDocumentEditor();

  // If the cursor is on a diagnostic, open its details.  Otherwise,
  // scan the text near the cursor to try to find the name of a file,
  // and open it in the editor.
  void openDiagnosticOrFileAtCursor(EditorNavigationOptions opts);

  // Navigate to `hostFileAndLine`, opening the file first if necessary.
  //
  // The awkward method name is due to avoiding the signal of the same
  // name that is used to implement this method.
  void doOpenOrSwitchToFileAtLineOpt(
    HostFileAndLineOpt const &hostFileAndLine);

  // Make the document shown in the widget be topmost in the global list
  // of open documents.
  void makeCurrentDocumentTopmost();

  // Navigate in this widget to local file `fname` at `lineOpt` and
  // `byteIndexOpt`.  The byte index is 0-based.
  //
  // TODO: The callers of this function, along with the function itself,
  // should be generalized to work with remote files too.
  void goToLocalFileAndLineOpt(
    std::string const &fname,
    std::optional<LineNumber> lineOpt,
    std::optional<int> byteIndexOpt);

  // ---------------------------- input -----------------------------
  // We are about to edit the text in the file.  If we are going from
  // a "clean" to "dirty" state with respect to unsaved changes, do a
  // safety check for concurrent on-disk modifications, prompting the
  // user if necessary.  Return true if it is safe to proceed, false if
  // the edit should be aborted without doing anything (because the
  // user has indicated they want to cancel it).
  bool editSafetyCheck();

  // TextDocumentObserver funcs
  virtual void observeInsertLine(TextDocumentCore const &buf, LineIndex line) NOEXCEPT OVERRIDE;
  virtual void observeDeleteLine(TextDocumentCore const &buf, LineIndex line) NOEXCEPT OVERRIDE;
  virtual void observeInsertText(TextDocumentCore const &buf, TextMCoord tc, char const *text, int length) NOEXCEPT OVERRIDE;
  virtual void observeDeleteText(TextDocumentCore const &buf, TextMCoord tc, int length) NOEXCEPT OVERRIDE;
  virtual void observeTotalChange(TextDocumentCore const &buf) NOEXCEPT OVERRIDE;
  virtual void observeMetadataChange(TextDocumentCore const &buf) NOEXCEPT OVERRIDE;

  // This is the same as 'keyPressEvent', but is meant to be callable by
  // other classes in order to pass on a key event that would otherwise
  // have been intercepted by something in the Qt infrastructure.  This
  // is *not* meant to be abused to simulate keystrokes!  Instead,
  // methods with higher-level purpose should be defined and used.
  void rescuedKeyPressEvent(QKeyEvent *k);

  // QObject methods.
  virtual bool eventFilter(QObject *watched, QEvent *event) NOEXCEPT OVERRIDE;

  // Perform `cmd`.
  void command(std::unique_ptr<EditorCommand> cmd);

  // Perform `cmd`, but do not steal or record it.  If the action cannot
  // be performed, return a string that explains to the user why not.
  std::optional<std::string> innerCommand(EditorCommand const *cmd);

  // Execute a named macro that is stored in `EditorGlobal`.
  void runMacro(std::string const &name);

  // -------------------------- output ----------------------------
  // intermediate paint steps
  void updateFrame(QPaintEvent *ev);
  void paintFrame(QPainter &winPaint);

  // Paint a single line of text.  The parameters are documented in
  // detail at the implementation site.
  void paintOneLine(
    QPainter &paint,
    int visibleLineCols,
    int startOfTrailingWhitespaceVisibleCol,
    LineCategories const &layoutCategories,
    ArrayStack<char> const &visibleText,
    TextDocumentEditor::LineIterator &&lineIter,
    TextCategoryAndStyle /*INOUT*/ &textCategoryAndStyle);

  // Draw an underline on `paint`, the canvas for one line, starting at
  // pixel `x` and continuing for `numCols` columns.
  void drawUnderline(QPainter &paint, int x, int numCols);

  // Map a byte index to a column index; and nullopt to nullopt.
  std::optional<int> byteIndexToLayoutColOpt(
    LineIndex line,
    std::optional<int> byteIndex) const;

  // Draw on `paint`, a one-line canvas, boxes around spans associated
  // with diagnostics for `line`.
  void drawDiagnosticBoxes(
    QPainter &paint,
    LineIndex line);

  // Draw to `paint`, the canvas for one line, a diagnostic box
  // surrounding the text starting at `startVisCol` and going up to but
  // *not* including `endVisCol`.
  void drawDiagnosticBox(
    QPainter &paint,
    int startVisCol,
    int endVisCol);

  // Assuming the cursor is on the current line, draw it onto `paint`,
  // the canvas for one line.
  //
  // `layoutCategories` contains the text rendering categories for each
  // character on the line.  This is used to redraw the character under
  // the cursor with the appropriate color.
  //
  // `lineGlyphColumns` is the total number of columns with glyphs on
  // this line, independent of window size or scroll position.
  //
  // `text` is the array of visible characters on the line.
  //
  void drawCursorOnLine(
    QPainter &paint,
    LineCategories const &layoutCategories,
    ArrayStack<char> const &text,
    int lineGlyphColumns);

  // If enabled, draw the soft margin on `paint`, which is the painting
  // area for a single line of text.
  void drawSoftMarginIndicator(QPainter &paint);

  // Paint a single character at the given location.
  void drawOneChar(QPainter &paint, QtBDFFont *font,
    QPoint const &pt, char c, bool withinTrailingWhitespace);

  // Return the style info for `cat`.
  TextCategoryAndStyle getTextCategoryAndStyle(TextCategory cat) const;

  // Get the widget-relative pixel rectangle where the cursor is
  // onscreen.  Since the cursor can be offscreen, this rectangle can be
  // too.
  QRect getCursorRect() const;

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

  // Get whether to update LSP continously.
  bool getLSPUpdateContinuously() const;

  // Toggle continuous update, returning the new state: true for
  // continuous update enabled.
  bool toggleLSPUpdateContinuously();

  // redraw widget, etc.; calls updateView() and viewChanged()
  void redraw();

  // Do `redraw` after emitting `signal_contentChange`.
  void redrawAfterContentChange();

  // Get a screenshot of the widget.
  QImage getScreenshot();

  void printUnhandled(smbase::XBase const &x)
    { unhandledExceptionMsgbox(this, x); }

  // Pop up a message related to a problem.
  void complain(std::string_view msg) const;

  // Pop up a message for general information.
  void inform(std::string_view msg) const;

  // ------------------------------ misc -------------------------------
  // Pass through to `NamedTextDocumentEditor`.
  string applyCommandSubstitutions(string const &orig) const;

  // -------------------------- testing ----------------------------
  // EventReplayQueryable methods.
  virtual string eventReplayQuery(string const &state) OVERRIDE;
  virtual QImage eventReplayImage(string const &what) OVERRIDE;
  virtual bool wantResizeEventsRecorded() OVERRIDE;

public Q_SLOTS:
  // slots to respond to scrollbars
  void scrollToLine(int line);
  void scrollToCol(int col);

  // Handlers for VFS_Connections.
  void on_vfsReplyAvailable(VFS_Connections::RequestID requestID) NOEXCEPT;
  void on_vfsConnectionFailed(HostName hostName, string reason) NOEXCEPT;

  // Handler for `DiagnosticDetailsDialog`.
  void on_jumpToDiagnosticLocation(DiagnosticElement const &element) NOEXCEPT;

Q_SIGNALS:
  // Emitted when some aspect of the document that is shown outside the
  // widget's own rectangle changes: scroll viewport, cursor location,
  // file name, whether there are unsaved changes, etc.  In practice,
  // this gets emitted on almost every keystroke because of the cursor
  // location change.
  void viewChanged();

  // Emitted when the user indicates, via some widget functionality,
  // that they want to go to a particular file, and possibly a specific
  // line.
  void signal_openOrSwitchToFileAtLineOpt(HostFileAndLineOpt hfl);

  // The user wants to close the search and replace panel if it is open.
  void closeSARPanel();

  // Emitted to indicate the number of search hits and the relative
  // position of the cursor and mark to them.
  void signal_searchStatusIndicator(QString const &status);

  // Emitted when the file metadata for the edited document changes.
  // Currently, this is relevant because it signals when new LSP
  // diagnostics are received, and the LSP status widget listens to
  // this.
  void signal_metadataChange();

  // Emitted when the content of the document changes.  This is *not*
  // emitted in response to scrolling or mere cursor movement.
  void signal_contentChange();
};


// Allow 'HostFileAndLineOpt' to be used as a queued signal parameter.
Q_DECLARE_METATYPE(HostFileAndLineOpt)


#endif // EDITOR_WIDGET_H
