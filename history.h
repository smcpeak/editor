// history.h
// undo/redo history of a buffer

#ifndef HISTORY_H
#define HISTORY_H

#include "exc.h"                       // xBase
#include "ogap.h"                      // OGapArray
#include "str.h"                       // stringBuilder
#include "text-document-core.h"        // TextDocumentCore

#include <stdint.h>                    // uintptr_t


// fwd in this file
class HistoryStats;


// buffer + cursor, the object that the history manipulates
class CursorBuffer : public TextDocumentCore {
public:
  // cursor location, initially 0,0
  int line, col;

  // as a possible point of future design variance, I might add more
  // than one cursor to this class at some point..

public:
  CursorBuffer();

  // TODO: Replace line/col with TextCoord.  This is a stopgap.
  TextCoord cursor() const
    { return TextCoord(this->line, this->col); }
  void setCursor(TextCoord tc)
    { this->line = tc.line; this->col = tc.column; }

  bool validCursor() const
    { return this->validCoord(this->cursor()); }

  bool cursorAtEnd() const
    { return this->locationAtEnd(this->cursor()); }
};


// XHistory is thrown when a history event finds it cannot be applied
// because the CursorBuffer state isn't consistent with the
// information stored in the event.
//
// NOTE:  All of the code paths that use XHistory are, as yet,
// untested.  To test them I need to implement a parser for the
// history concrete syntax, and make some histories that are
// inconsistent some buffer contents.
class XHistory : public xBase {
public:
  XHistory(char const *msg) : xBase(stringc << "XHistory: " << msg) {}
  XHistory(XHistory const &obj) : xBase(obj) {}
};


// interface that elements of the history sequence implement: an
// invertible transformation on a CursorBuffer
class HistoryElt {
public:
  // interface clients can deallocate
  virtual ~HistoryElt();

  // type interrogation
  enum Tag { HE_CURSOR, HE_TEXT, HE_GROUP, NUM_HE_TYPES };
  virtual Tag getTag() const=0;

  // apply this operator, possibly in reverse; or, throw XHistory
  // if the event does not match the current state of the buffer,
  // but in this case the buffer must not be modified; return true
  // if the action actually modified the buffer contents (as opposed
  // to just the cursor)
  virtual bool apply(CursorBuffer &buf, bool reverse)=0;

  // render this command as a text line, indented by 'indent' spaces
  virtual void print(stringBuilder &sb, int indent) const=0;

  // account for this history record
  virtual void stats(HistoryStats &stats) const=0;
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
  static void print1dim(stringBuilder &sb, int orig, int val);

public:      // funcs
  HE_cursor(int ol, int l, int oc, int c)
    : origLine(ol), line(l), origCol(oc), col(c) {}

  // HistoryElt funcs
  virtual Tag getTag() const { return HE_CURSOR; }
  virtual bool apply(CursorBuffer &buf, bool reverse);
  virtual void print(stringBuilder &sb, int indent) const;
  virtual void stats(HistoryStats &stats) const;

  // 'apply' as a static member, to allow HE_group to represent
  // HE_cursors more efficiently but still use the same implementation
  static bool static_apply(CursorBuffer &buf, int origLine, int line,
                           int origCol, int col, bool reverse);
};


// text insertion/deletion, when cursor is in defined text area
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

  // text to insert or delete; may contain embedded NULs
  char *text;          // (owner) NULL iff textLen==0
  int textLen;

private:     // funcs
  static void insert(CursorBuffer &buf, char const *text,
                     int textLen, bool left, bool reverse);

public:      // funcs
  HE_text(bool insertion, bool left,
          char const *text, int textLen);
  ~HE_text();

  // HistoryElt funcs
  virtual Tag getTag() const { return HE_TEXT; }
  virtual bool apply(CursorBuffer &buf, bool reverse);
  virtual void print(stringBuilder &sb, int indent) const;
  virtual void stats(HistoryStats &stats) const;

  // 'apply', but static
  static bool static_apply(
    CursorBuffer &buf, bool insertion, bool left,
    char const *text, int textLen, bool reverse);

  // compute correct 'text' and 'textLen' for forward application of a
  // deletion of 'count' characters; entire span of deleted text must
  // be in defined area
  void computeText(CursorBuffer const &buf, int count);
};


// group of history elements to be treated as a unit for
// purposes of interactive undo/redo
class HE_group : public HistoryElt {
private:     // data
  // single entry in the sequence; see HE_group::encode() in
  // history.cc for encoding
  typedef uintptr_t HistoryEltCode;

  // sequence represented as a growable array for efficient
  // storage but also insertion
  GapArray<HistoryEltCode> seq;

private:     // funcs
  // apply elements in reverse order if 'reverse' is true
  bool applySeqElt(CursorBuffer &buf, int start, int end, int offset,
                   bool reverseIndex, bool reverseOperation);

  // remove the first element in the sequence (there must be at
  // least one), and return an owner pointer to it
  HistoryEltCode stealFirstEltCode();

  // encode a pointer
  HistoryEltCode encode(HistoryElt * /*owner*/ e);

  // decode a pointer; returns an owner if 'allocated' is true
  HistoryElt *decode(HistoryEltCode code, bool &allocated) const;

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

  // squeeze any space that is currently being reserved for future
  // growth; this is called when the app is fairly certain that
  // this sequence will not grow any more
  void squeezeReserved();

  // remove all elements with index 'newLength' or greater; it must
  // be that 0 <= newLength <= seqLength()
  void truncate(int newLength);

  // clear the history
  void clear() { truncate(0); }

  // apply some contiguous sequence of the element transformations;
  // the transformations applied are those with index such that
  // start <= index < end; they are applied start to end-1, unless
  // 'reverse' is true, in which case they are applied end-1 to start,
  // each transformation itself applied with 'reverse' as passed in
  bool applySeq(CursorBuffer &buf, int start, int end, bool reverse);

  // apply a single element of the sequence, possibly in reverse
  bool applyOne(CursorBuffer &buf, int index, bool reverse);

  // print, and mark the nth element of the history in the left
  // margin; if 'n' is outside the range of valid indices, no mark is
  // printed
  void printWithMark(stringBuilder &sb, int indent, int n) const;


  // HistoryElt funcs
  virtual Tag getTag() const { return HE_GROUP; }
  virtual bool apply(CursorBuffer &buf, bool reverse);
  virtual void print(stringBuilder &sb, int indent) const;
  virtual void stats(HistoryStats &stats) const;
};


// memory allocation and other resource statistics about a
// history sequence
class HistoryStats {
public:
  // # of leaf (non-group) records
  int records;

  // # of grouping constructs
  int groups;

  // memory used by the objects in the sequence
  int memUsage;

  // # of malloc (or 'new') objects allocated; used to estimate
  // heap data structure overhead
  int mallocObjects;

  // space reserved for future expansion by some data structure,
  // for example the space in the gap for HE_group
  int reservedSpace;

public:
  HistoryStats();

  // estimate total space usage from current totals
  int totalUsage() const;

  // print all info
  void printInfo() const;
};


#endif // HISTORY_H
