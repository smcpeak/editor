// text-document.h
// Text document with undo/redo history attached to it.

#ifndef TEXT_DOCUMENT_H
#define TEXT_DOCUMENT_H

#include "history.h"      // history representation
#include "objstack.h"     // ObjStack
#include "array.h"        // ArrayStack


// Represent a file being edited:
//
//   * File contents.
//   * Cursor location.
//   * Undo history of changes to them.
//
// TextDocument provides the core functionality for manipulating
// these items.  Buffer is built on top of it and provides a
// variety of useful composed manipulations.
//
// There is no reason to create a TextDocument alone; the division
// is just to ensure a separation between core and extended
// functionality in the implementation of Buffer.
class TextDocument {
private:      // data
  // Current buffer contents and cursor location.
  CursorBuffer buf;

  // modification history
  HE_group history;

  // where are we in that history?  usually,
  // historyIndex==history.seqLength(), meaning we're at the end of the
  // recorded history; undo/redo modifies 'historyIndex' and 'buf' but
  // not 'history'
  int historyIndex;

  // what index in 'history' corresponds to the file's on-disk
  // contents?  the client of this interface has to inform me when
  // the file gets saved, but I'll track when the changes get away
  // from that point; 'savedHistoryIndex' tracks 'historyIndex' when
  // the contents are in correspondence and we're moving across
  // nondestructive actions
  //
  // invariant: -1 <= savedHistoryIndex <= seq.Length()
  int savedHistoryIndex;

  // stack of open history groups, which will soon be collapsed
  // and added to their parent group, or 'history' for the last
  // (outermost) group; typically this stack is empty, or has
  // just one element between beginGroup() and endGroup(), but
  // I allow for the generality of a stack anyway
  ObjStack<HE_group> groupStack;

private:     // funcs
  void appendElement(HistoryElt *e);

public:      // funcs
  TextDocument();      // empty buffer, empty history, cursor at 0,0
  ~TextDocument();

  // ---- queries ----
  // read-only access to the underlying representation
  CursorBuffer const &core() const        { return buf; }

  // TextDocumentCore's queries, directly exposed.
  int numLines() const                    { return buf.numLines(); }
  int lineLength(int line) const          { return buf.lineLength(line); }
  bool validCoord(TextCoord tc) const     { return buf.validCoord(tc); }
  void getLine(int line, int col, char *dest, int destLen) const
    { return buf.getLine(TextCoord(line, col), dest, destLen); }
  int maxLineLength() const               { return buf.maxLineLength(); }

  // cursor
  int line() const                        { return buf.line; }
  int col() const                         { return buf.col; }
  TextCoord cursor() const                { return buf.cursor(); }
  bool cursorAtEnd() const                { return buf.cursorAtEnd(); }

  // current contents differ from those on disk?
  bool unsavedChanges() const;


  // ---- global changes ----
  // clear history, leaving only the current buffer contents
  void clearHistory();

  // clear buffer contents *and* history
  void clearContentsAndHistory();

  // Replace current contents with a new file, and reset cursor
  // to 0,0; clears the history.
  //
  // Like ::readFile, if the file cannot be opened, then this throws
  // XOpen and does not modify anything.  But a later read error leaves
  // this object in an incomplete state.
  void readFile(char const *fname);

  // Remember the current historyIndex as one where the file's contents
  // agree with those on the disk.
  void noUnsavedChanges() { savedHistoryIndex = historyIndex; }


  // ---- manipulate and append to history ----

  // Insert 'text' at 'tc'.  'text' may contain newline characters.
  // 'tc' must be valid for the document.
  void insertAt(TextCoord tc, char const *text, int textLen);

  // Delete 'count' characters at (to the right of) 'tc'.  This
  // may span lines.  Each end-of-line counts as one character.
  // 'tc' must be valid for the document.
  void deleteAt(TextCoord tc, int count);


  // ---- LEGACY manipulation interface ----

  // TODO: These three methods should be moved into a new class,
  // TextDocumentEditor.

  // cursor motion; line/col are relative if their respective 'rel'
  // flag is true
  void moveCursor(bool relLine, int line, bool relCol, int col);

  // insertion at cursor; 'left' or 'right' refers to where the cursor
  // ends up after the insertion; cursor must be in defined area
  void insertLR(bool left, char const *text, int textLen);

  // deletion at cursor; 'left' or 'right' refers to which side of
  // the cursor has the text to be deleted
  void deleteLR(bool left, int count);


  // facility for grouping actions with HE_group
  void beginGroup();
  void endGroup();

  // true if we have an open group; note that undo/redo is not allowed
  // in that case, even though canUndo/Redo() may return true
  bool inGroup() const        { return groupStack.isNotEmpty(); }


  // ---- undo/redo ----
  bool canUndo() const        { return historyIndex > 0; }
  bool canRedo() const        { return historyIndex < history.seqLength(); }

  void undo();
  void redo();


  // print the history in a textual format, with the current history
  // index marked (or no mark if history index is at the end)
  void printHistory(stringBuilder &sb) const;
  void printHistory(/*to stdout*/) const;

  // get statistics about history memory usage
  void historyStats(HistoryStats &stats) const
    { history.stats(stats); }
  void printHistoryStats() const;
};


class HBGrouper {
  TextDocument &buf;

public:
  HBGrouper(TextDocument &b) : buf(b)
    { buf.beginGroup(); }
  ~HBGrouper()
    { buf.endGroup(); }
};




#if 0      // high-level stuff to put in Buffer
  // relative cursor movement
  void moveDown(int dy);
  void moveRight(int dx);

  // absolute cursor movement
  void gotoLine(int newLine);
  void gotoCol(int newCol);
  void gotoLC(int newLine, int newCol);

  // insertion at cursor; 'left' or 'right' refers to where the
  // cursor ends up after the insertion; if the cursor is beyond
  // the end of the line or file, newlines/spaces are inserted to
  // fill to the cursor first
  void insertLeft(char const *text, int length);
  void insertRight(char const *text, int length);

  // deletion at cursor; 'left' or 'right' refers to which side of
  // the cursor has the text to be deleted; space is filled before
  // the deletion, if necessary
  void deleteLeft(int count);
  void deleteRight(int count);
#endif // 0


  // HERE:
  // problem: need to introduce notion of additional cursors, to
  // support having multiple views into the same buffer, it seems..


#endif // TEXT_DOCUMENT_H