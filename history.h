// history.h
// undo/redo history of a buffer

#ifndef HISTORY_H
#define HISTORY_H

#include "buffercore.h"      // BufferCore
#include "exc.h"             // xBase


// buffer + cursor, the object that the history manipulates
class CursorBuffer : public BufferCore {
public:
  // cursor location, intially 0,0
  int line, col;

  // as a possible point of future design variance, I might add more
  // than one cursor to this class at some point..
  
public:
  CursorBuffer();
  
  bool cursorInDefined() const
    { return locationInDefined(line, col); }
};

  
// thrown when a history event finds it cannot be applied because
// the CursorBuffer state isn't consistent with the information
// stored in the event
class XHistory : public xBase {
public:
  XHistory(char const *msg) : xBase(msg) {}
  XHistory(XHistory const &obj) : xBase(obj) {}
};


// interface that elements of the history sequence implement: an
// invertible transformation on a CursorBuffer
class HistoryElt {
public:
  // interface clients can deallocate
  virtual ~HistoryElt();

  // apply this operator, possibly in reverse; or, throw XHistory
  // if the event does not match the current state of the buffer,
  // but in this case the buffer must not be modified
  virtual void apply(CursorBuffer &buf, bool reverse)=0;
};


// cursor motion
class HE_cursor : public HistoryElt {
public:      // data
  // if origLine==-1, then 'line' is a relative line; otherwise
  // this operator moves the cursor from 'origLine' to 'line' as
  // absolute line numbers
  int origLine, line;

  // similar encoding for column movement
  int origCol, col;
        
private:     // funcs
  static void move1dim(int &value, int orig, int update, bool reverse);

public:      // funcs
  HE_cursor(int ol, int l, int oc, int c)
    : origLine(ol), line(l), origCol(oc), col(c) {}

  // HistoryElt funcs
  virtual void apply(CursorBuffer &buf, bool reverse);

  // 'apply' as a static member, to allow HE_group to represent
  // HE_cursors more efficiently but still use the same implementation
  static void static_apply(CursorBuffer &buf, int origLine, int line,
                           int origCol, int col, bool reverse);
};


// text insertion/deletion
class HE_text : public HistoryElt {
public:      // data
  // if true, this is an insertion; otherwise a deletion
  bool insertion;

  // if true:
  //   insertion: leave cursor to left of inserted text
  //   deletion: delete text to the left of the cursor
  // if false:
  //   insertion: leave cursor to right of inserted text
  //   deletion: delete text to the right of the cursor
  bool left;

  // number of lines or spaces of fill to add at the end of
  // the line and/or buffer to make the cursor be within
  // the defined text area
  int fill;

  // text to insert or delete; may contain embedded NULs
  char *text;          // (owner)
  int textLen;

private:     // funcs
  static void fill(CursorBuffer &buf, int fill, bool reverse);
  static void fillRight(CursorBuffer &buf, int fill);
  static bool onlySpaces(CursorBuffer &buf, int amt);
  static void insert(CursorBuffer &buf, char const *text,
                     int textLen, bool left, bool reverse)
  static void computeBothFill(CursorBuffer &buf, int &rowfill, int &colfill);

public:      // funcs
  HE_text(bool insertion, bool left, int fill,
          char const *text, int textLen);
  ~HE_text();

  // HistoryElt funcs
  virtual void apply(CursorBuffer &buf, bool reverse);

  // 'apply', but static
  static void static_apply(
    CursorBuffer &buf, bool insertion, bool left, int fill,
    char const *text, int textLen, bool reverse);

  // compute the correct 'fill' for forward application
  void computeFill(CursorBuffer &buf);

  // compute correct 'text' and 'textLen' for forward application of a
  // deletion of 'count' characters; entire span of deleted text must
  // be in defined area
  void computeText(CursorBuffer &buf, int count);
};


// group of history elements to be treated as a unit for
// purposes of interactive undo/redo
class HE_group : public HistoryElt {
private:     // data
  // sequence represented as a growable array for efficient
  // storage but also insertion; the objects are owned by
  // this object
  OGapArray<HistoryElt> seq;

private:     // funcs
  // yield elements in reverse order if 'reverse' is true
  HistoryElt *elt(int start, int end, bool reverse, int offset);

public:      // funcs
  HE_group();          // initially, seq is empty
  ~HE_group();         // deallocates objects in 'seq'

  // sequence manipulation; I do not expose 'seq' itself, because
  // I want to be able to do various forms of compression while
  // the sequence is being built; when a HistoryElt* is passed in,
  // the HE_group takes ownership of it (and may in fact delete it,
  // in favor of a different representation)

  int seqLength() const         { return seq.length(); }

  void append(HistoryElt *e);

  // remove all elements with index 'newLength' or greater; it must
  // be that 0 <= newLength <= seqLength()
  void truncate(int newLength);

  // apply some contiguous sequence of the element transformations;
  // the transformations applied are those with index such that
  // start <= index < end; they are applied start to end-1, unless
  // 'reverse' is true, in which case they are applied end-1 to start,
  // each transformation itself applied with 'reverse' as passed in
  void applySeq(CursorBuffer &buf, int start, int end, bool reverse);

  // apply a single element of the sequence, possibly in reverse
  void applyOne(CursorBuffer &buf, int index, bool reverse);


  // HistoryElt funcs
  virtual void apply(CursorBuffer &buf, bool reverse);
};


#endif // HISTORY_H
