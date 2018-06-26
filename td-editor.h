// td-editor.h
// TextDocumentEditor class.

#ifndef TD_EDITOR_H
#define TD_EDITOR_H

#include "td.h"                        // TextDocument


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

  // TODO: I think I should remove these.  It is confusing to have
  // clients sometimes directly accessing the underlying documents.
  TextDocument *doc() const { return m_doc; }
  TextDocumentCore const &core() const { return doc()->getCore(); }

  // -------------------- query file dimensions --------------------
  // Number of lines in the document.  Always positive.
  int numLines() const { return doc()->numLines(); }

  // Length of a given line, not including newline.  'line' must be
  // in [0,numLines()-1].
  int lineLength(int line) const { return doc()->lineLength(line); }

  // Line length, or 0 if it's beyond the end of the file.  'line' must
  // be non-negative.
  int lineLengthLoose(int line) const;

  // Position right after last character in file.
  TextCoord endCoord() const { return doc()->endCoord(); }

  // ---------------------------- cursor ---------------------------
  // Current cursor position.  Always non-negative, but may be beyond
  // the end of its line or the entire file.
  TextCoord cursor() const { return m_cursor; }

  // Legacy interface.  TODO: Remove.
  int line() const { return cursor().line; }
  int col() const { return cursor().column; }

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
  void setCursor(TextCoord newCursor);

  // Cursor motion; line/col are relative if their respective 'rel'
  // flag is true.
  void moveCursor(bool relLine, int line, bool relCol, int col);

  // ------------------------- mark ------------------------------
  // Current mark location.  The mark is the counterpart to the cursor
  // for defining a selection region.  This asserts that the mark is
  // active.
  TextCoord mark() const;

  // True if the mark is "active", meaning the UI shows a visible
  // selection region.
  bool markActive() const { return m_markActive; }

  // Set the mark and make it active.
  void setMark(TextCoord m);

  // Make the mark inactive.
  void clearMark() { m_mark = TextCoord(); m_markActive = false; }

  // ---------------- visible region and scrolling -----------------
  // Upper-left grid cell that is visible.
  TextCoord firstVisible() const { return m_firstVisible; }

  // Lower-right grid cell fully visible (not partial).
  TextCoord lastVisible() const { return m_lastVisible; }

  int visLines() const
    { return m_lastVisible.line - m_firstVisible.line + 1; }
  int visColumns() const
    { return m_lastVisible.column - m_firstVisible.column + 1; }

  // Scroll so 'fv' is the first visible coordinate, retaining the
  // visible region size.
  void setFirstVisible(TextCoord fv);

  // Adjust the visible region size, preserving 'firstVisible'.  This
  // will silently ensure both sizes are positive.
  void setVisibleSize(int lines, int columns);

  // ---------------- general text queries -------------------
  // Get part of a single line's contents, starting at 'line/col' and
  // getting 'destLen' chars.  All the chars must be in the line now.
  // The retrieved text never includes the '\n' character.
  void getLine(TextCoord tc, char *dest, int destLen) const
    { return doc()->getLine(tc, dest, destLen); }

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

  // get a complete line
  string getWholeLine(int line) const;

  // get the word following the given coordinate, including any non-word
  // characters that precede that word; stop at end of line
  string getWordAfter(TextCoord tc) const;

  // ---------------- whitespace text queries -------------------
  // on a particular line, get # of whitespace chars before first
  // non-ws char, or -1 if there are no non-ws chars
  int getIndentation(int line) const;

  // starting at 'line', go up until we find a line that is not
  // entirely blank (whitespace), and return the # of whitespace
  // chars to the left of the first non-whitespace char
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

  // ---------------------- cursor movement --------------------
  // move by relative line/col
  void moveRelCursor(int deltaLine, int deltaCol);

  // move to absolute line/col
  void moveAbsCursor(int newLine, int newCol);

  // use a relative movement to go to a specific line/col; this is
  // used for restoring the cursor position after some sequence
  // of edits
  void moveRelCursorTo(int newLine, int newCol);

  // relative to same line, absolute to given column
  void moveAbsColumn(int newCol);

  // line++, col=0
  void moveToNextLineStart();

  // line--, col=length(line-1)
  void moveToPrevLineEnd();

  // Move cursor position one character forwards or backwards, wrapping
  // to the next/prev line at line edges.  Stops at the start of the
  // file, but will advance indefinitely at the end.
  void advanceWithWrap(bool backwards);

  // --------------------- modify selection ----------------------
  // Select the entire line containing the cursor.
  void selectCursorLine();

  // ------------------- general text insertion ------------------
  // Insert at cursor.  'left' or 'right' refers to where the cursor
  // ends up after the insertion.  Requires validCursor().
  void insertLR(bool left, char const *text, int textLen);

  // Insert NUL-terminated text that might have newline characters at
  // the cursor.  The cursor is left at the end of the inserted text.
  // Requires validCursor().
  void insertText(char const *text);

  // ------------------- general text deletion ------------------
  // Delete at cursor.  'left' or 'right' refers to which side of
  // the cursor has the text to be deleted.  This can delete newline
  // characters.  Requires validCursor().
  void deleteLR(bool left, int count);

  void deleteText(int len) { deleteLR(false /*left*/, len); }

  void deleteChar() { deleteText(1); }

  // Delete the characters between line1/col1 and line/col2.  Both
  // are truncated to ensure validity.  line1/col1 must be before
  // or equal to line2/col2.  Final cursor is left at line1/col1.
  void deleteTextRange(int line1, int col1, int line2, int col2);

  // ---------------------- adding whitespace ----------------------
  // Add whitespace to buffer as necessary to ensure 'validCursor()'.
  void fillToCursor();

  void insertSpace() { insertText(" "); }
  void insertSpaces(int howMany);

  // split 'line' into two, putting everything after cursor column
  // into the next line; if 'col' is beyond the end of the line,
  // spaces are *not* appended to 'line' before inserting a blank line
  // after it; the function returns with line incremented by 1 and
  // col==0
  void insertNewline();

  // indent (or un-indent, if ind<0) the line range
  // [start,start+lines-1] by some # of spaces; if unindenting, but
  // there are not enough spaces, then the line is unindented as much
  // as possible w/o removing non-ws chars; the cursor is left in its
  // original position at the end
  void indentLines(int start, int lines, int ind);

  // -------------------- undo/redo -----------------------
  // True if we can go back another step in the undo history.
  bool canUndo() const                 { return doc()->canUndo(); }

  // True if we can go forward, redoing undone changes.
  bool canRedo() const                 { return doc()->canRedo(); }

  // Undo a document changes.  Required 'canUndo()'.
  void undo();

  // Redo a document change that was undone.  Required 'canRedo()'.
  void redo();

  // Between begin/end, all document modifications will be grouped
  // together into a single undo action.
  void beginUndoGroup()                { doc()->beginUndoGroup(); }
  void endUndoGroup()                  { doc()->endUndoGroup(); }

  // True if we currently have an undo group open.
  bool inUndoGroup() const             { return doc()->inUndoGroup(); }

  // True if the document has changed since it was last saved.  This
  // is related to undo/redo because if you save, make a change, then
  // undo, it is the undo/redo mechanism that understands the file now
  // has no unsaved changes.
  bool unsavedChanges() const { return doc()->unsavedChanges(); }
};


ENUM_BITWISE_OPS(TextDocumentEditor::FindStringFlags,
                 TextDocumentEditor::FS_ALL)


// save/restore cursor across an operation; uses a relative
// cursor movement to restore at the end, so the presumption
// is that only relative movements have appeared in between
class CursorRestorer {
  TextDocumentEditor &doc;
  TextCoord orig;

public:
  CursorRestorer(TextDocumentEditor &d);
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
