// historybuf.h
// buffer with undo/redo history attached to it

#ifndef HISTORYBUF_H
#define HISTORYBUF_H

#include "history.h"      // history representation
#include "objstack.h"     // ObjStack


// buffer that records all of the manipulations in its undo
// history; on top of this spartan interface I'll build the
// Buffer class itself, with many convenience functions that
// all map onto calls into this class
class HistoryBuffer {
private:      // data
  // current buffer contents and cursor location
  CursorBuffer buf;

  // modification history
  HE_group history;

  // where are we in that history?  usually,
  // time==history.seqLength(), meaning we're at the end of the
  // recorded history; undo/redo modifies 'time' and 'buf' but not
  // 'history'
  int time;

  // stack of open history groups, which will soon be collapsed
  // and added to their parent group, or 'history' for the last
  // (outermost) group; typically this stack is empty, or has
  // just one element between beginGroup() and endGroup(), but
  // I allow for the generality of a stack anyway
  ObjStack<HE_group> groupStack;

private:     // funcs
  void appendElement(HistoryElt *e);

public:      // funcs
  HistoryBuffer();      // empty buffer, empty history, cursor at 0,0
  ~HistoryBuffer();

  // ---- queries ----
  // read-only access to the underlying representation
  CursorBuffer const &core() const        { return buf; }

  // BufferCore's queries, directly exposed
  int numLines() const                    { return buf.numLines(); }
  int lineLength(int line) const          { return buf.lineLength(line); }
  void getLine(int line, int col, char *dest, int destLen) const
    { return buf.getLine(line, col, dest, destLen); }
  int maxLineLength() const               { return buf.maxLineLength(); }

  // cursor
  int line() const { return buf.line; }
  int col() const { return buf.col; }


  // ---- global changes ----
  // clear history, leaving only the current buffer contents
  void clearHistory();

  // clear buffer contents *and* history
  void clearContentsAndHistory();

  // replace current contents with a new file, and reset cursor
  // to 0,0; clears the history
  void readFile(char const *fname);


  // ---- manipulate and append to history ----
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
  bool canUndo() const        { return time > 0; }
  bool canRedo() const        { return time < history.seqLength()-1; }

  void undo();
  void redo();
           

  // print the history in a textual format, with the current time
  // marked (or no mark if time is at the end)
  void printHistory(stringBuilder &sb) const;
  void printHistory(/*to stdout*/) const;

  // get statistics about history memory usage
  void historyStats(HistoryStats &stats) const 
    { history.stats(stats); }
  void printHistoryStats() const;
};


class HBGrouper {
  HistoryBuffer &buf;

public:
  HBGrouper(HistoryBuffer &b) : buf(b)
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


#endif // HISTORYBUF_H
