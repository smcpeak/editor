// history.h
// Represent the undo/redo history of a text document.

#ifndef HISTORY_H
#define HISTORY_H

#include "array.h"                     // ObjArrayStack
#include "exc.h"                       // xBase
#include "str.h"                       // stringBuilder
#include "text-document-core.h"        // TextDocumentCore

#include <stdint.h>                    // uintptr_t


// fwd in this file
class HistoryStats;


// buffer + cursor, the object that the history manipulates
//
// TODO: This class should be eliminated and the cursor storage moved
// to a new class, TextDocumentEditor.
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
    { return this->cursor() == endCoord(*this); }
};


// XHistory is thrown when a history event finds it cannot be applied
// because the CursorBuffer state isn't consistent with the
// information stored in the event.
//
// NOTE:  All of the code paths that use XHistory are, as yet,
// untested.  To test them I need to implement a parser for the
// history concrete syntax, and make some histories that are
// inconsistent with some buffer contents.
class XHistory : public xBase {
public:
  XHistory(char const *msg) : xBase(stringc << "XHistory: " << msg) {}
  XHistory(XHistory const &obj) : xBase(obj) {}
};


// Interface that elements of the history sequence implement: an
// invertible transformation on a TextDocument.
class HistoryElt {
public:
  // Clients may deallocate HistoryElt objects.
  virtual ~HistoryElt();

  // Type interrogation.
  enum Tag { HE_TEXT, HE_GROUP, NUM_HE_TYPES };
  virtual Tag getTag() const=0;

  // Apply this operator, possibly in reverse; or, throw XHistory
  // if the event does not match the current state of the buffer,
  // but in this case the buffer must not be modified.
  //
  // Return the coordinate of the left edge of the modified text.
  virtual TextCoord apply(TextDocumentCore &doc, bool reverse) const=0;

  // render this command as a text line, indented by 'indent' spaces
  virtual void print(stringBuilder &sb, int indent) const=0;

  // account for this history record
  virtual void stats(HistoryStats &stats) const=0;
};


// Text insertion/deletion.
class HE_text : public HistoryElt {
public:      // data
  // Where in the document to make the modification.  If this is
  // a deletion, this is the left edge of the span.
  TextCoord tc;

  // If true, this is an insertion; otherwise a deletion.
  bool insertion;

  // Text to insert or delete; may contain embedded NULs.
  char *text;          // (owner) NULL iff textLen==0
  int textLen;

private:     // funcs
  static void insert(TextDocumentCore &buf, TextCoord tc,
                     char const *text, int textLen, bool reverse);

public:      // funcs
  // This makes a copy of 'text'.
  HE_text(TextCoord tc, bool insertion, char const *text, int textLen);
  ~HE_text();

  // HistoryElt funcs
  virtual Tag getTag() const override { return HE_TEXT; }
  virtual TextCoord apply(TextDocumentCore &buf, bool reverse) const override;
  virtual void print(stringBuilder &sb, int indent) const override;
  virtual void stats(HistoryStats &stats) const override;

  // 'apply', but static
  static TextCoord static_apply(
    TextDocumentCore &buf, TextCoord tc, bool insertion,
    char const *text, int textLen, bool reverse);

  // compute correct 'text' and 'textLen' for forward application of a
  // deletion of 'count' characters; entire span of deleted text must
  // be in defined area
  void computeText(TextDocumentCore const &buf, int count);
};


// Group of history elements to be treated as a unit for
// purposes of interactive undo/redo.
class HE_group : public HistoryElt {
private:     // data
  // Sequence of actions in this group.
  ObjArrayStack<HistoryElt> seq;

private:     // funcs
  // apply elements in reverse order if 'reverse' is true
  TextCoord applySeqElt(TextDocumentCore &buf, int start, int end, int offset,
                        bool reverseIndex, bool reverseOperation) const;

public:      // funcs
  HE_group();          // initially, seq is empty
  ~HE_group();         // deallocates objects in 'seq'

  // sequence manipulation; I do not expose 'seq' itself, because
  // I want to be able to do various forms of compression while
  // the sequence is being built; when a HistoryElt* is passed in,
  // the HE_group takes ownership of it (and may in fact delete it,
  // in favor of a different representation)

  // Number of elements in this group.
  int seqLength() const         { return seq.length(); }

  // Add 'e' to the end of this group, taking ownership of it.
  void append(HistoryElt *e);

  // Pull the last element out of the sequence, returning an owner
  // pointer.  The sequence must be non-empty.
  HistoryElt *popLastElement();

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
  TextCoord applySeq(TextDocumentCore &buf, int start, int end, bool reverse) const;

  // apply a single element of the sequence, possibly in reverse
  TextCoord applyOne(TextDocumentCore &buf, int index, bool reverse) const;

  // print, and mark the nth element of the history in the left
  // margin; if 'n' is outside the range of valid indices, no mark is
  // printed
  void printWithMark(stringBuilder &sb, int indent, int n) const;


  // HistoryElt funcs
  virtual Tag getTag() const override { return HE_GROUP; }
  virtual TextCoord apply(TextDocumentCore &buf, bool reverse) const override;
  virtual void print(stringBuilder &sb, int indent) const override;
  virtual void stats(HistoryStats &stats) const override;
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
