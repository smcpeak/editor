// td-editor.h
// TextDocumentEditor class.

#ifndef TD_EDITOR_H
#define TD_EDITOR_H

// editor
#include "td.h"                        // TextDocument
#include "textcategory.h"              // LineCategories
#include "textlcoord.h"                // TextLCoord

// smbase
#include "smbase/datetime.h"           // DateTimeProvider
#include "smbase/refct-serf.h"         // RCSerf, SerfRefCount


// This class is a "stateful metaphor UI API".  That is, it
// encapsulates an aspect of a stateful user interface for manipulating
// a document as a set of callable methods.  These methods correspond
// to functions that are exposed in the real UI, with associated UI
// triggers like keybindings and buttons; but this class does not know
// about any of that.
//
// This is a novel design pattern to me, so I do not have or know of a
// particularly good name for it
//
// By far the most important stateful metaphor for a text editor is the
// notion of a "cursor", and this class has the cursor.  It also has
// the "mark", which is the other endpoint (besides the cursor) in a
// "selection".
//
// Both the cursor and mark are *layout* coordinates.  This class
// translates between layout and model coordinates; it is the class that
// knows how to lay out a text document on a regular 2D grid of cells.
class TextDocumentEditor : public SerfRefCount {
  // Copying would not necessarily be a problem, but for the moment I
  // want to ensure I do not do it accidentally.
  NO_OBJECT_COPIES(TextDocumentEditor);

public:      // types
  // Flags to control the behavior of 'insertText'.
  enum InsertTextFlags {
    // Default behavior.
    ITF_NONE                 = 0x0,

    // When set, select the just-inserted text, specifically placing the
    // mark at the start and the cursor at the end.
    ITF_SELECT_AFTERWARD     = 0x1,

    // When set, instead of putting the cursor at the end of the
    // inserted text, leave it at the start.  This flag is incompatible
    // with `ITF_SELECT_AFTERWARD`.
    ITF_CURSOR_AT_START      = 0x2,
  };

public:      // static data
  // Constructions minus destructions, for bug detection.
  static int s_objectCount;

private:     // data
  // The document we are editing.  This is the true "document" in the
  // sense of representing an abstract mathematical object of interest
  // that is persistently stored when the editor is not running.
  //
  // This is not an owner pointer.  It is the client's responsibility
  // to ensure the TextDocumentEditor is destroyed before the
  // TextDocument.
  RCSerf<TextDocument> m_doc;

  // Cursor, the point around which editing revolves.  In the UI
  // design of this class, the cursor can go anywhere--it is not
  // constrained to the current document bounds.  (But the coordinates
  // must be non-negative.)
  TextLCoord m_cursor;

  // Mark, the "other" point.  When it is active, we have a
  // "selection", a region of text on which we can operate.  Like the
  // cursor, it can be anything non-negative.
  bool m_markActive;
  TextLCoord m_mark;

  // At any time, only a portion of the document is visible within a
  // scrollable editing region.  These are coordinates of the cells in
  // the top-left and bottom-right corners.  In a pixel-level GUI,
  // some cells may be only partially visible, but everything in the
  // rectangle bounded by these points (inclusive) is fully visible.
  // We assume that if some text is within this rectangle, the user
  // can see it.
  //
  // These coordinates are in units of fixed-size grid cells, meaning
  // we assume that the document as a whole is presented that way.
  //
  // Invariants:
  //   * m_firstVisible.line <= m_lastVisible.line
  //   * m_firstVisible.column <= m_lastVisible.column
  TextLCoord m_firstVisible;
  TextLCoord m_lastVisible;

  // Width of a tab column.  Must be positive.  Initially 8.
  int m_tabWidth;

private:     // funcs
  // Helper for 'toMCoord'.
  TextMCoord innerToMCoord(TextLCoord lc) const;

public:      // funcs
  explicit TextDocumentEditor(TextDocument *doc);
  ~TextDocumentEditor();

  // Check class invariants, failing an assertion if violated.
  void selfCheck() const;

  // Get the document that was passed to the constructor.  My
  // preference is to avoid using this, instead going through the
  // TextDocumentEditor interface when possible for uniformity.
  //
  // However, there are some algorithms, especially syntax
  // highlighting, that are defined to operate on a TextDocument
  // (actually TextDocumentCore) since they do not modify the text,
  // and for that we need the underlying document.
  TextDocument const *getDocument() const { return m_doc; }

  // Similarly for the core.  This is not meant for ordinary application
  // code to use, rather it is meant to allow connecting components that
  // explicitly depend on lower-level elements.
  TextDocumentCore const *getDocumentCore() const
    { return &(m_doc->getCore()); }

  // Status of associated process, if any.
  DocumentProcessStatus documentProcessStatus() const
    { return m_doc->documentProcessStatus(); }

  // Get/set read-only flag.
  bool isReadOnly() const              { return m_doc->isReadOnly(); }
  void setReadOnly(bool readOnly)      { m_doc->setReadOnly(readOnly); }

  // Get/set tab width.  Always positive.
  int tabWidth() const                 { return m_tabWidth; }
  void setTabWidth(int tabWidth);

  // ------------------- query model dimensions --------------------
  // Number of lines in the document.  Always positive.
  int numLines() const                 { return m_doc->numLines(); }

  bool isEmptyLine(int line) const     { return m_doc->isEmptyLine(line); }

  // Length of the line in bytes, not including the newline.  'line'
  // must be in [0,numLines()-1].
  int lineLengthBytes(int line) const  { return m_doc->lineLengthBytes(line); }

  int maxLineLengthBytes() const       { return m_doc->maxLineLengthBytes(); }

  // Begin/end model coordinates.
  TextMCoord beginMCoord() const       { return m_doc->beginCoord(); }
  TextMCoord endMCoord() const         { return m_doc->endCoord(); }

  TextMCoord lineBeginMCoord(int line) const { return m_doc->lineBeginCoord(line); }
  TextMCoord lineEndMCoord(int line) const   { return m_doc->lineEndCoord(line); }

  bool validMCoord(TextMCoord mc) const{ return m_doc->validCoord(mc); }

  // ------------------- coordinate transformation -----------------
  // Convert the given layout coordinate to a valid model coordinate.
  // This is the first coordinate that is at-or-after 'tc', or
  // 'endMCoord()' if none is.
  TextMCoord toMCoord(TextLCoord lc) const;

  // Convert the given model coordinate to the layout coordinate of the
  // first grid cell the identified code point occupies, or would occupy
  // (for an "ordinary" character) if 'mc' is at EOL or EOF.  'mc' must
  // be a valid coordinate.
  TextLCoord toLCoord(TextMCoord mc) const;

  // Convert coordinate ranges.
  TextMCoordRange toMCoordRange(TextLCoordRange const &range) const;
  TextLCoordRange toLCoordRange(TextMCoordRange const &range) const;

  // ------------------- query layout dimensions -------------------
  // Number of populated cells on the given line, in columns.  This does
  // not count the newline.  If 'line' is outside [0,numLines()-1],
  // returns 0.
  int lineLengthColumns(int line) const;

  // Length of line containing cursor.
  int cursorLineLengthColumns() const  { return lineLengthColumns(cursor().m_line); }

  // Length of the longest line.  Note: This may overestimate.
  int maxLineLengthColumns() const;

  // First position in the file.
  TextLCoord beginLCoord() const         { return TextLCoord(0,0); }

  // Position right after last character in file.
  TextLCoord endLCoord() const;

  // Range for the entire document.
  TextLCoordRange documentLRange() const
    { return TextLCoordRange(beginLCoord(), endLCoord()); }

  // Position at end of specified line, which may be beyond EOF.
  TextLCoord lineEndLCoord(int line) const;

  // Walk the given coordinate forwards (right, then down, when
  // distance>0) or backwards (left, then up, when distance<0) through
  // the layout coordinates of the file.  If distance is negative, we
  // silently stop at the document start even if the magnitude would
  // take us into negative line numbers.
  void walkLCoordColumns(TextLCoord &tc, int distance) const;

  // Walk forward or backward the given number of bytes.
  void walkLCoordBytes(TextLCoord &tc, int distance) const;
  void walkMCoordBytes(TextMCoord &tc, int distance) const;

  // ---------------------------- cursor ---------------------------
  // Current cursor position.  Always non-negative, but may be beyond
  // the end of its line or the entire file.
  TextLCoord cursor() const             { return m_cursor; }

  // True if the cursor is at the end of its line.
  bool cursorAtLineEnd() const;

  // True if the cursor is at the very last valid position.
  bool cursorAtEnd() const;

  // Set the cursor position.  Asserts is non-negative.
  //
  // This does *not* automatically scroll the visible region.  But
  // a client can call 'scrollToCursor()' afterward.
  void setCursor(TextLCoord newCursor);

  // Set the cursor column, keeping its line.
  void setCursorColumn(int newCol);

  // Cursor motion; line/col are relative if their respective 'rel'
  // flag is true.  The result must be non-negative.
  void moveCursor(bool relLine, int line, bool relCol, int col);

  // move by relative line/col
  void moveCursorBy(int deltaLine, int deltaCol);

  // line++, col=0.  Ok to move beyond EOF.
  void moveToNextLineStart();

  // line--, col=length(line-1).  Ok to start beyond EOF.  When it hits
  // BOF, will stop at end of first line.
  void moveToPrevLineEnd();

  // Move to the beginning of the first or last line in the file.
  void moveCursorToTop();
  void moveCursorToBottom();

  // Move cursor position one character forward or backward, wrapping
  // to the next/prev line at line edges.  Stops at the start of the
  // file, but will advance indefinitely at the end.
  //
  // If beyond EOL, going forward will wrap to the start of the next
  // line, while going backward will move left one column at a time.
  void advanceWithWrap(bool backwards);

  // Move the cursor by the minimum amount necessary so it becomes
  // inside the visible region.
  void confineCursorToVisible();

  // Walk the cursor forward or backward by 'distance' bytes.  It must
  // be possible to do so and remain in the valid area.
  void walkCursorBytes(int distance);

  // True if converting the cursor's location to a model coordinate and
  // back is the identity function.
  bool cursorOnModelCoord() const;

  // Get the nearest model coordinate to the cursor.
  TextMCoord cursorAsModelCoord() const;

  // ------------------------- mark ------------------------------
  // Current mark location.  The mark is the counterpart to the cursor
  // for defining a selection region.  This asserts that the mark is
  // active.
  TextLCoord mark() const;

  // True if the mark is "active", meaning the UI shows a visible
  // selection region.
  bool markActive() const              { return m_markActive; }

  // Set the mark and make it active.
  void setMark(TextLCoord m);

  // Make the mark inactive.
  void clearMark() { m_mark = TextLCoord(); m_markActive = false; }

  // Move the mark by the indicated amount, clamping at 0.  Requires
  // markActive().
  void moveMarkBy(int deltaLine, int deltaCol);

  // ----------------- cursor+mark = selection --------------------
  // If the mark is inactive, activate it at the cursor.
  void turnOnSelection();

  // Turn selection on or off depending on 'on'.
  void turnSelection(bool on) { on? turnOnSelection() : clearMark(); }

  // If the mark is active but equal to cursor, turn it off.
  void turnOffSelectionIfEmpty();

  // Select the entire line containing the cursor.
  void selectCursorLine();

  // Select the entire file.
  void selectEntireFile();

  // Get a range whose start is the lower of 'cursor' and 'mark', and
  // whose end is the higher.  If the mark is not active, set both to
  // 'cursor'.
  TextLCoordRange getSelectLayoutRange() const;

  // Same, but as a model range.
  TextMCoordRange getSelectModelRange() const;

  // Set 'cursor' to the start and 'mark' to the end of 'range'.
  void setSelectRange(TextLCoordRange const &range);

  // Get selected text, or "" if nothing selected.
  string getSelectedText() const;

  // Get selected text, or if nothing is selected, the identifier the
  // cursor is on, or "" if the cursor is not on an identifier.
  string getSelectedOrIdentifier() const;

  // Swap the cursor and mark.  Does nothing if the mark is inactive.
  void swapCursorAndMark();

  // If the mark is active and greater than the cursor, swap them
  // so the mark is the lesser of the two.
  void normalizeCursorGTEMark();

  // ---------------- visible region and scrolling -----------------
  // Upper-left grid cell that is visible.
  TextLCoord firstVisible() const       { return m_firstVisible; }

  // Lower-right grid cell fully visible (not partial).
  TextLCoord lastVisible() const        { return m_lastVisible; }

  int visLines() const
    { return m_lastVisible.m_line - m_firstVisible.m_line + 1; }
  int visColumns() const
    { return m_lastVisible.m_column - m_firstVisible.m_column + 1; }

  // Scroll so 'fv' is the first visible coordinate, retaining the
  // visible region size.
  void setFirstVisible(TextLCoord fv);

  // Scroll in one dimension.
  void setFirstVisibleLine(int L)
    { this->setFirstVisible(TextLCoord(L, m_firstVisible.m_column)); }
  void setFirstVisibleCol(int C)
    { this->setFirstVisible(TextLCoord(m_firstVisible.m_line, C)); }

  // Move the view by a relative amount.  Any attempt to go negative
  // is treated as a move to zero.
  void moveFirstVisibleBy(int deltaLine, int deltaCol);

  // First, scroll the view, if necessary, so the cursor is in the
  // visible region.  Then, behave like 'moveFirstVisibleBy', but also
  // move the cursor as necessary so its position on the screen stays
  // the same.
  void moveFirstVisibleAndCursor(int deltaLine, int deltaCol);

  // Like 'moveFirstVisibleBy', but after scrolling, adjust the cursor
  // by the minimum amount so it is onscreen.
  void moveFirstVisibleConfineCursor(int deltaLine, int deltaCol)
    { moveFirstVisibleBy(deltaLine, deltaCol); confineCursorToVisible(); }

  // Set the lower-right corner, preserving 'firstVisible'.  This will
  // silently force it to be greater than or equal to 'firstVisible'.
  void setLastVisible(TextLCoord lv);

  // Adjust the visible region size, preserving 'firstVisible'.  This
  // will silently ensure both sizes are positive.
  void setVisibleSize(int lines, int columns);

  // Scroll the view the minimum amount so that 'tc' is visible
  // and at least 'edgeGap' lines/columns from the edge (except that
  // it can always get to the left and top edges of the document).
  //
  // If there isn't enough room to leave that large a gap, center the
  // view on 'tc', except do not go past the left/top edge.
  //
  // If 'edgeGap' is -1, then if 'tc' isn't already visible,
  // center the visible region on it.
  void scrollToCoord(TextLCoord tc, int edgeGap=0);

  // Same, but for the cursor.
  void scrollToCursor(int edgeGap=0)
    { scrollToCoord(cursor(), edgeGap); }

  // Scroll vertically so the cursor line is in the center of the
  // visible region if possible (it might not be possible due to the
  // cursor being too near the top of the file).  Also scroll so the
  // visible region is as far to the left as possible while keeping
  // the cursor in view.
  void centerVisibleOnCursorLine();

  // If the cursor is below the bottom edge of the visible area, but
  // within `howFar` lines of being visible, and within the horizontal
  // bounds, then scroll to make the cursor visible with `edgeGap`
  // between the cursor and the edge.
  void scrollToCursorIfBarelyOffscreen(int howFar, int edgeGap);

  // ---------------- general text queries -------------------
  // If character 'c' is displayed at column 'col', what column do we
  // end up in right after that?  Usually it is 'c+1' but if 'c' is the
  // tab character then it can be different.
  //
  // 'c' must not be newline.
  int layoutColumnAfter(int col, int c) const;

  // Get a range of text from a line, but if the position is outside
  // the defined range, pretend the line exists (if necessary) and
  // that there are space characters up to 'col+destLen' (if necessary).
  //
  // This is effectively computing the layout for a fragment of the
  // document and expressing it as a sequence of grid contents.  Each
  // grid cell's content is, for now, expressed as one byte.  Tacitly,
  // the bytes are interpreted as Latin-1 code points.
  void getLineLayout(TextLCoord tc,
                     ArrayStack<char> /*INOUT*/ &dest, int destLen) const;

  // Retrieve the text between two positions, as in a text editor where
  // the positions are the selection endpoints and the user wants a
  // string to put in the clipboard.  The range must be rectified.  If
  // start==end, returns "".  Characters outside the document area are
  // taken to be whitespace.
  void getTextForLRange(TextLCoordRange const &range,
                        ArrayStack<char> /*INOUT*/ &dest) const;
  void getTextForLRange(TextLCoord const &start, TextLCoord const &end,
                        ArrayStack<char> /*INOUT*/ &dest) const
    { return getTextForLRange(TextLCoordRange(start, end), dest); }

  // Versions that yield strings.
  string getTextForLRangeString(TextLCoordRange const &range) const;
  string getTextForLRangeString(TextLCoord const &start, TextLCoord const &end) const
    { return getTextForLRangeString(TextLCoordRange(start, end)); }

  // Get a complete line.  Returns "" when beyond EOF.  'line' must
  // be non-negative.
  void getWholeLine(int line, ArrayStack<char> /*INOUT*/ &dest) const;
  string getWholeLineString(int line) const;

  // get the word following the given coordinate, including any non-word
  // characters that precede that word; stop at end of line
  string getWordAfter(TextLCoord tc) const;

  // Given the character style info in 'modelCategories', compute
  // column style info in 'layoutCategories'.
  void modelToLayoutSpans(int line,
    LineCategories /*OUT*/ &layoutCategories,
    LineCategories /*IN*/ const &modelCategories);

  // ---------------- whitespace text queries -------------------
  // For the given line, count the number of whitespace characters
  // before either a non-ws character or EOL.  Beyond EOF, return 0.
  int countLeadingSpacesTabs(int line) const;

  // Count them from the end instead.
  int countTrailingSpacesTabsColumns(int line) const;

  // On a particular line, get # of whitespace chars before first
  // non-ws char, or -1 if there are no non-ws chars.  Lines beyond
  // EOF return -1 (as if they are entirely whitespace).
  //
  // This is almost the same as 'countLeadingSpacesTabs', except that
  // it returns -1 instead of lineLength for blank lines, and it returns
  // a count of columns instead of bytes.
  //
  // This also sets 'indText' to the actual indentation string.
  int getIndentationColumns(int line, string /*OUT*/ &indText) const;

  // Starting at 'line', including that line, search up until we find a
  // line that is not entirely blank (whitespace), and return the number
  // of whitespace columns to the left of the first non-whitespace
  // character.  Lines beyond EOF are treated as entirely whitespace.
  // If we hit BOF, return 0.
  //
  // If the return is non-negative, also set 'indText'.
  int getAboveIndentationColumns(int line, string /*OUT*/ &indText) const;

  // ------------------- general text insertion ------------------
  // 1. If the mark is active, deleteSelection().
  //
  // 2. If the cursor is beyond EOL or EOF, fill with whitespace.
  //
  // 3. Insert text, which might have newlines, at cursor.  Place the
  // cursor at the end of the inserted text, scrolling if necessary to
  // ensure the cursor is in visible region afterward.
  //
  // 'textLen' is measured in bytes, not characters.
  void insertText(char const *text, int textLen,
                  InsertTextFlags flags = ITF_NONE);

  // Same, but using a 'string' object.
  //
  // This is not called 'insertText' because 'string' has an implicit
  // conversion from char*, which would make it easy to call the wrong
  // one if they were overloaded.
  void insertString(string const &text,
                    InsertTextFlags flags = ITF_NONE);

  // Same, but assuming NUL termination.  Potentially dangerous!
  void insertNulTermText(char const *text,
                         InsertTextFlags flags = ITF_NONE)
    { insertText(text, strlen(text), flags); }

  // ------------------- general text deletion ------------------
  // Delete at cursor.  'left' or 'right' refers to which side of
  // the cursor has the text to be deleted.  This can delete newline
  // characters.
  void deleteLRColumns(bool left, int columnCount);
  void deleteLRBytes(bool left, int byteCount);
  void deleteLRAbsCharacters(bool left, int characterCount);

  void deleteTextBytes(int len)  { deleteLRBytes(false /*left*/, len); }

  void deleteChar()              { deleteLRAbsCharacters(false /*left*/, 1); }

  // Delete the characters between the range start and end.  Both are
  // truncated to ensure validity (this implies it does *not* fill to
  // the start initially).  Requires 'range.isRectified()'.  Final
  // cursor is left at range start.  Clears the mark.
  void deleteTextLRange(TextLCoordRange const &range);
  void deleteTextLRange(TextLCoord const &start, TextLCoord const &end)
    { deleteTextLRange(TextLCoordRange(start, end)); }

  // Same, but for a range of model coordinates.
  void deleteTextMRange(TextMCoordRange const &range);

  // If the selection start is not beyond EOF, fill with whitespace to
  // the start, then delete the selected text.  Requires markActive().
  // Cursor is left at the low end of the selection, mark is cleared.
  // Scrolls to the cursor afterward.
  void deleteSelection();

  // Same as 'deleteSelection', but a no-op if not markActive().
  void deleteSelectionIf() { if (m_markActive) { deleteSelection(); } }

  // Do what Backspace should do: If text is selected, delete it.
  // Otherwise, delete one character to the left of the cursor,
  // except if the cursor is beyond EOL or EOF, just move one space
  // left if possible, otherwise up, without adding whitespace to
  // the file.
  void backspaceFunction();

  // Do what Delete should do: If text is selected, delete it.
  // Otherwise, delete one character to the right of the cursor.
  // If we are beyond EOL but not EOF, fill with spaces and then
  // delete the newline to splice the next line.  If beyond EOF,
  // do nothing.
  void deleteKeyFunction();

  // ---------------------- adding whitespace ----------------------
  // Add minimum whitespace near 'tc' to ensure it is not beyond EOL
  // or EOF.
  //
  // The horizontal whitespace inserted by this function is most often
  // just spaces, but if the 'tc' line is currently blank, and the
  // nearest preceding non-blank line is indented with tabs or a mix of
  // tabs and spaces, this function will insert as much of that same
  // indentation as possible and needed to get to the desired column,
  // filling any remainder with spaces.
  void fillToCoord(TextLCoord const &tc);

  void fillToCursor() { fillToCoord(cursor()); }

  void insertSpace() { insertNulTermText(" "); }
  void insertSpaces(int howMany);

  // split 'line' into two, putting everything after cursor column
  // into the next line; if 'col' is beyond the end of the line,
  // spaces are *not* appended to 'line' before inserting a blank line
  // after it; the function returns with cursor line incremented by 1
  // and cursor col==0
  void insertNewline();

  // This is what my Enter key does:
  //   - If beyond EOL, move back to EOL.
  //   - If beyond EOF, append newlines to meet cursor.
  //   - Insert the newline.
  //   - Scroll to left edge.
  //   - Auto-indent.
  //
  // Automatic indentation means moving the cursor into the same column
  // as the first non-whitespace character in the first non-blank line
  // found by searching upward from the newly inserted line.
  //
  // Actually inserting indentation is delayed until some text is added
  // to the line, at which point 'fillToCursor' does it.
  void insertNewlineAutoIndent();

  // indent (or un-indent, if ind<0) the line range
  // [start,start+lines-1] by some # of spaces; if unindenting, but
  // there are not enough spaces, then the line is unindented as much
  // as possible w/o removing non-ws chars; the cursor is left in its
  // original position at the end
  void indentLines(int start, int lines, int ind);

  // Do 'indentLines' for the span of lines corresponding to the
  // current selection.  Every line that has at least one selected
  // character is indented.  If nothing is selected, return false
  // without doing anything.
  bool blockIndent(int amt);

  // Call ::justifyNearLine on the cursor line, then scroll if needed
  // to keep the cursor in view.
  bool justifyNearCursor(int desiredWidth);

  // --------------------- other insertion ------------------------
  // Insert, at cursor, the local date/time as "YYYY-MM-DD hh:ss".
  void insertDateTime(DateTimeProvider *provider = NULL);

  // ------------------------ clipboard ---------------------------
  // The functions here are part of a clipboard implementation, as
  // they manipulate the document but not the clipboard itself.  The
  // client is expected to do the clipboard half.

  // If text is selected, return it and turn off the selection.
  // Otherwise, return an empty string.
  //
  // The reason for turning off the selection is it lets the user see
  // a visible effect of the operation so they know it happened.  (In
  // Eclipse, which does not do that, I occasionally press Ctrl+C too
  // lightly and fail to copy the text.)
  string clipboardCopy();

  // Like 'clipboardCopy', but also delete the selected text.
  string clipboardCut();

  // Delete any current selection, then insert 'text', which can be
  // empty, although clients should normally avoid that.
  //
  // If `cursorToStart`, the cursor is left at the start of the inserted
  // text, and at the end otherwise.
  void clipboardPaste(char const *text, int textLen,
                      bool cursorToStart = false);

  // -------------------- undo/redo -----------------------
  // True if we can go back another step in the undo history.
  bool canUndo() const                 { return m_doc->canUndo(); }

  // True if we can go forward, redoing undone changes.
  bool canRedo() const                 { return m_doc->canRedo(); }

  // Undo a document changes.  Requires 'canUndo()'.
  void undo();

  // Redo a document change that was undone.  Requires 'canRedo()'.
  void redo();

  // Between begin/end, all document modifications will be grouped
  // together into a single undo action.
  void beginUndoGroup()                { m_doc->beginUndoGroup(); }
  void endUndoGroup()                  { m_doc->endUndoGroup(); }

  // True if we currently have an undo group open.
  bool inUndoGroup() const             { return m_doc->inUndoGroup(); }

  // True if the document has changed since it was last saved.  This
  // is related to undo/redo because if you save, make a change, then
  // undo, it is the undo/redo mechanism that understands the file now
  // has no unsaved changes.
  bool unsavedChanges() const          { return m_doc->unsavedChanges(); }

  // -------------------------- observers -----------------------------
  void addObserver(TextDocumentObserver *observer)
    { m_doc->addObserver(observer); }
  void removeObserver(TextDocumentObserver *observer)
    { m_doc->removeObserver(observer); }
  bool hasObserver(TextDocumentObserver const *observer) const
    { return m_doc->hasObserver(observer); }

  // ------------------------- diagnostics ----------------------------
  // Print some debugging information to stdout for use in testing.
  void printHistory() const            { m_doc->printHistory(); }
  void printHistoryStats() const       { m_doc->printHistoryStats(); }

  // Print the document contents and things like cursor and mark.
  void debugPrint() const;

  // ---------------------- iterator ----------------------------
public:      // types
  // Iterate over the bytes in a line, with layout information.
  //
  // That is, each time `advByte()` is called, we step forward one byte
  // in the model, but might step forward more than one layout column.
  // (Currently, the only reason to do that is for tab characters.)
  //
  // TODO UTF-8: Allow iteration over code points.
  class LineIterator {
    NO_OBJECT_COPIES(LineIterator);

  private:     // instance data
    // Document editor we are iterating over.
    TextDocumentEditor const &m_tde;

    // Underlying iterator.
    TextDocument::LineIterator m_iter;

    // Column number where the current byte's glyph starts.  If
    // '!has()', this is one more than the column number where the
    // previous glyph (the last one in the line) ends.
    int m_column;

  public:      // funcs
    // Same interface as TextDocument::LineIterator.
    LineIterator(TextDocumentEditor const &tde, int line);
    ~LineIterator()                    {}
    bool has() const                   { return m_iter.has(); }
    int byteOffset() const             { return m_iter.byteOffset(); }
    int byteAt() const                 { return m_iter.byteAt(); }
    void advByte();

    // See 'm_column'.
    int columnOffset() const           { return m_column; }
  };
};


// Save/restore cursor, mark, and scroll position across an operation.
//
// The data members are public as a convenience in case the client
// wants to refer to the old values.
class CursorRestorer {
public:      // data
  // Editor we will restore.
  RCSerf<TextDocumentEditor> tde;

  // Values to restore.
  TextLCoord const cursor;
  bool const markActive;
  TextLCoord const mark;
  TextLCoord const firstVisible;

  // We don't save 'lastVisible' since we assume that the visible
  // region size will not be affected by the operations this class
  // is wrapped around.

public:      // funcs
  CursorRestorer(TextDocumentEditor &tde);
  ~CursorRestorer() NOEXCEPT;
};


// Class to begin/end an undo group.
class UndoHistoryGrouper {
private:     // data
  RCSerf<TextDocumentEditor> m_editor;

public:      // funcs
  UndoHistoryGrouper(TextDocumentEditor &e);
  ~UndoHistoryGrouper() NOEXCEPT;
};


// The purpose of this class is to be inherited by TextDocumentAndEditor
// (TDAE).  It allows TDAE to "inherit" TextDocument without exposing
// its members, which would cause ambiguities with those of TDE.  The
// reason TDAE needs to inherit TD instead of just having one as a
// member is to ensure that TD is constructed before TDE, which is
// necessary since TDE::m_doc is an RCSerf<TD>.
class WrappedTextDocument {
public:
  TextDocument m_innerDoc;
};


// Combination of editor and document to edit.  This is mainly useful
// in test programs.
//
// Incidentally, this is the first time I have used private inheritance
// without overriding any virtual methods.  This is an interesting
// little pattern.
class TextDocumentAndEditor : private WrappedTextDocument,
                              public TextDocumentEditor {
public:
  TextDocumentAndEditor()
    : WrappedTextDocument(),
      TextDocumentEditor(&m_innerDoc)
  {}

  TextDocument &writableDoc() { return m_innerDoc; }
};


#endif // TD_EDITOR_H
