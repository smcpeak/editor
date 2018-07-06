// td-editor.h
// TextDocumentEditor class.

#ifndef TD_EDITOR_H
#define TD_EDITOR_H

#include "td.h"                        // TextDocument

#include "datetime.h"                  // DateTimeProvider


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
class TextDocumentEditor {
  // Copying would not necessarily be a problem, but for the moment I
  // want to ensure I do not do it accidentally.
  NO_OBJECT_COPIES(TextDocumentEditor);

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
  TextDocument *m_doc;

  // Cursor, the point around which editing revolves.  In the UI
  // design of this class, the cursor can go anywhere--it is not
  // constrained to the current document bounds.  (But the coordinates
  // must be non-negative.)
  TextCoord m_cursor;

  // Mark, the "other" point.  When it is active, we have a
  // "selection", a region of text on which we can operate.  Like the
  // cursor, it can be anything non-negative.
  bool m_markActive;
  TextCoord m_mark;

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
  // There is an unresolved issue about how best to deal with non-
  // fixed-width elements like Tab and Unicode composing sequences.
  //
  // Invariants:
  //   * m_firstVisible.line <= m_lastVisible.line
  //   * m_firstVisible.column <= m_lastVisible.column
  TextCoord m_firstVisible;
  TextCoord m_lastVisible;

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

  // -------------------- query file dimensions --------------------
  // Number of lines in the document.  Always positive.
  int numLines() const                 { return m_doc->numLines(); }

  // Length of a given line, not including newline.  'line' must be
  // in [0,numLines()-1].
  int lineLength(int line) const       { return m_doc->lineLength(line); }

  // Line length, or 0 if it's beyond the end of the file.  'line' must
  // be non-negative.
  int lineLengthLoose(int line) const;

  // Length of line containing cursor.
  // Returns 0 when cursor is beyond EOF.
  int cursorLineLength() const         { return lineLengthLoose(cursor().line); }

  // Length of the longest line.  Note: This may overestimate.
  int maxLineLength() const            { return m_doc->maxLineLength(); }

  // First position in the file.
  TextCoord beginCoord() const         { return TextCoord(0,0); }

  // Position right after last character in file.
  TextCoord endCoord() const           { return m_doc->endCoord(); }

  // Position at end of specified line, which may be beyond EOF.
  TextCoord lineEndCoord(int line) const;

  bool validCoord(TextCoord tc) const  { return m_doc->validCoord(tc); }

  // Clamp the coordinate to the valid region of the document.
  void truncateCoord(TextCoord &tc) const;

  // Walk the given coordinate forwards (right, then down, when
  // distance>0) or backwards (left, then up, when distance<0) through
  // the valid coordinates of the file.  It must initially be in the
  // valid area, but if by walking we get out of bounds, then the
  // function simply returns false (otherwise true).
  bool walkCoord(TextCoord &cursor, int distance) const;

  // Given two locations that are within the valid area, and with
  // tc1 <= tc2, compute the # of chars between them, counting line
  // boundaries as one char.
  int computeSpanLength(TextCoord tc1, TextCoord tc2) const;

  // ---------------------------- cursor ---------------------------
  // Current cursor position.  Always non-negative, but may be beyond
  // the end of its line or the entire file.
  TextCoord cursor() const             { return m_cursor; }

  // Return true if the cursor is within the current text, i.e.,
  // not beyond the end of a line or beyond the end of the file.
  //
  // The term "valid" is bit too strong here, since the design of
  // this interface is to tolerate the cursor being anywhere in
  // the file.  But a few functions still demand this for
  // historical reasons.  TODO: Fix those.
  bool validCursor() const;

  // True if the cursor is at the very last valid position.
  bool cursorAtEnd() const;

  // Set the cursor position.  Asserts is non-negative.
  //
  // This does *not* automatically scroll the visible region.  But
  // a client can call 'scrollToCursor()' afterward.
  void setCursor(TextCoord newCursor);

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

  // Walk the cursor forward or backward by 'distance'.  It must be
  // possible to do so and remain in the valid area.
  void walkCursor(int distance);

  // ------------------------- mark ------------------------------
  // Current mark location.  The mark is the counterpart to the cursor
  // for defining a selection region.  This asserts that the mark is
  // active.
  TextCoord mark() const;

  // True if the mark is "active", meaning the UI shows a visible
  // selection region.
  bool markActive() const              { return m_markActive; }

  // Set the mark and make it active.
  void setMark(TextCoord m);

  // Make the mark inactive.
  void clearMark() { m_mark = TextCoord(); m_markActive = false; }

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

  // Store into 'selLow' the lower of 'cursor' and 'mark', and into
  // 'selHigh' the higher.  If the mark is not active, set both to
  // 'cursor'.
  void getSelectRegion(TextCoord &selLow, TextCoord &selHigh) const;

  // Get selected text, or "" if nothing selected.
  string getSelectedText() const;

  // Swap the cursor and mark.  Does nothing if the mark is inactive.
  void swapCursorAndMark();

  // If the mark is active and greater than the cursor, swap them
  // so the mark is the lesser of the two.
  void normalizeCursorGTEMark();

  // ---------------- visible region and scrolling -----------------
  // Upper-left grid cell that is visible.
  TextCoord firstVisible() const       { return m_firstVisible; }

  // Lower-right grid cell fully visible (not partial).
  TextCoord lastVisible() const        { return m_lastVisible; }

  int visLines() const
    { return m_lastVisible.line - m_firstVisible.line + 1; }
  int visColumns() const
    { return m_lastVisible.column - m_firstVisible.column + 1; }

  // Scroll so 'fv' is the first visible coordinate, retaining the
  // visible region size.
  void setFirstVisible(TextCoord fv);

  // Scroll in one dimension.
  void setFirstVisibleLine(int L)
    { this->setFirstVisible(TextCoord(L, m_firstVisible.column)); }
  void setFirstVisibleCol(int C)
    { this->setFirstVisible(TextCoord(m_firstVisible.line, C)); }

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

  // Adjust the visible region size, preserving 'firstVisible'.  This
  // will silently ensure both sizes are positive.
  void setVisibleSize(int lines, int columns);

  // Scroll the view the minimum amount so that the cursor is visible
  // and at least 'edgeGap' lines/columns from the edge (except that
  // it can always get to the left and top edges of the document).
  //
  // If 'edgeGap' is -1, then if the cursor isn't already visible,
  // center the visible region on the cursor.
  void scrollToCursor(int edgeGap=0);

  // Scroll vertically so the cursor line is in the center of the
  // visible region if possible (it might not be possible due to the
  // cursor being too near the top of the file).  Also scroll so the
  // visible region is as far to the left as possible while keeping
  // the cursor in view.
  void centerVisibleOnCursorLine();

  // ---------------- general text queries -------------------
  // Get part of a single line's contents, starting at 'line/col' and
  // getting 'destLen' chars.  All the chars must be in the line now.
  // The retrieved text never includes the '\n' character.
  void getLine(TextCoord tc, char *dest, int destLen) const
    { return m_doc->getLine(tc, dest, destLen); }

  // get a range of text from a line, but if the position is outside
  // the defined range, pretend the line exists (if necessary) and
  // that there are space characters up to 'col+destLen' (if necessary)
  void getLineLoose(TextCoord tc, char *dest, int destLen) const;

  // retrieve the text between two positions, as in a text editor
  // where the positions are the selection endpoints and the user
  // wants a string to put in the clipboard; it must be the case
  // that tc1 <= tc2; characters outside defined area
  // are taken to be whitespace
  string getTextRange(TextCoord tc1, TextCoord tc2) const;

  // Get a complete line.  Returns "" when beyond EOF.  'line' must
  // be non-negative.
  string getWholeLine(int line) const;

  // get the word following the given coordinate, including any non-word
  // characters that precede that word; stop at end of line
  string getWordAfter(TextCoord tc) const;

  // ---------------- whitespace text queries -------------------
  // For the given line, count the number of whitespace characters
  // before either a non-ws character or EOL.  Beyond EOF, return 0.
  int countLeadingSpaceChars(int line) const;

  // On a particular line, get # of whitespace chars before first
  // non-ws char, or -1 if there are no non-ws chars.  Lines beyond
  // EOF return -1 (as if they are entirely whitespace).
  int getIndentation(int line) const;

  // Starting at 'line', including that line, search up until we find
  // a line that is not entirely blank (whitespace), and return the
  // number of whitespace chars to the left of the first non-whitespace
  // char.  Lines beyond EOF are treated as entirely whitespace.  If we
  // hit BOF, return 0.
  int getAboveIndentation(int line) const;

  // ---------------------- text search ----------------------
  // flags for findString()
  enum FindStringFlags {
    FS_NONE                = 0x00, // nothing special

    FS_CASE_INSENSITIVE    = 0x01, // case insensitive
    FS_BACKWARDS           = 0x02, // search backwards in file
    FS_ADVANCE_ONCE        = 0x04, // advance meta-cursor once before searching
    FS_ONE_LINE            = 0x08, // only search the named line

    FS_ALL                 = 0x0F  // all flags
  };

  // search from 'tc' to find the first occurrence of
  // 'text', and update 'tc' to the beginning of the
  // match; return false for no match; 'text' will not be
  // tested for matches that span multiple lines
  bool findString(TextCoord /*INOUT*/ &tc, char const *text,
                  FindStringFlags flags = FS_NONE) const;

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
  void insertText(char const *text, int textLen);

  // Same, but using a 'string' object.
  //
  // This is not called 'insertText' because 'string' has an implicit
  // conversion from char*, which would make it easy to call the wrong
  // one if they were overloaded.
  void insertString(string text);

  // Same, but assuming NUL termination.  Potentially dangerous!
  void insertNulTermText(char const *text)
    { insertText(text, strlen(text)); }

  // ------------------- general text deletion ------------------
  // Delete at cursor.  'left' or 'right' refers to which side of
  // the cursor has the text to be deleted.  This can delete newline
  // characters.  Requires validCursor().
  void deleteLR(bool left, int count);

  void deleteText(int len)             { deleteLR(false /*left*/, len); }

  void deleteChar()                    { deleteText(1); }

  // Delete the characters between 'tc1' and 'tc2'.  Both
  // are truncated to ensure validity.  Requires 'tc1 <= tc2'.
  // Final cursor is left at 'tc1'.  Clears the mark.
  void deleteTextRange(TextCoord tc1, TextCoord tc2);

  // Delete the selected text.  Requires markActive().  Cursor is
  // left at the low end of the selection, mark is cleared.  Scrolls
  // to the cursor afterward.
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
  // Add minimum whitespace near cursor to ensure 'validCursor()'.
  void fillToCursor();

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
  // Automatic indentation means adding as many spaces as there are
  // before the first non-whitespace character in the first non-blank
  // line found by searching upward from the newly inserted line.
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
  // empty, although clients should normally avoid that.  The cursor
  // is left at the end of the inserted text.
  void clipboardPaste(char const *text, int textLen);

  // -------------------- undo/redo -----------------------
  // True if we can go back another step in the undo history.
  bool canUndo() const                 { return m_doc->canUndo(); }

  // True if we can go forward, redoing undone changes.
  bool canRedo() const                 { return m_doc->canRedo(); }

  // Undo a document changes.  Required 'canUndo()'.
  void undo();

  // Redo a document change that was undone.  Required 'canRedo()'.
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

  // ------------------------- diagnostics ----------------------------
  // Print some debugging information to stdout for use in testing.
  void printHistory() const            { m_doc->printHistory(); }
  void printHistoryStats() const       { m_doc->printHistoryStats(); }

  // Print the document contents and things like cursor and mark.
  void debugPrint() const;
};


ENUM_BITWISE_OPS(TextDocumentEditor::FindStringFlags,
                 TextDocumentEditor::FS_ALL)


// Save/restore cursor, mark, and scroll position across an operation.
//
// The data members are public as a convenience in case the client
// wants to refer to the old values.
class CursorRestorer {
public:      // data
  // Editor we will restore.
  TextDocumentEditor &tde;

  // Values to restore.
  TextCoord const cursor;
  bool const markActive;
  TextCoord const mark;
  TextCoord const firstVisible;

  // We don't save 'lastVisible' since we assume that the visible
  // region size will not be affected by the operations this class
  // is wrapped around.

public:      // funcs
  CursorRestorer(TextDocumentEditor &tde);
  ~CursorRestorer();
};


// Class to begin/end an undo group.
class UndoHistoryGrouper {
  TextDocumentEditor &editor;

public:
  UndoHistoryGrouper(TextDocumentEditor &e) : editor(e)
    { editor.beginUndoGroup(); }
  ~UndoHistoryGrouper()
    { editor.endUndoGroup(); }
};


// Combination of editor and document to edit.  This is mainly useful
// in test programs.
class TextDocumentAndEditor : public TextDocumentEditor {
private:     // data
  // Embedded document the editor is editing
  TextDocument innerDoc;

public:
  TextDocumentAndEditor() : TextDocumentEditor(&innerDoc) {}
};


#endif // TD_EDITOR_H
