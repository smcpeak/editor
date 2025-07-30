// history.h
// Represent the undo/redo history of a text document.

#ifndef HISTORY_H
#define HISTORY_H

// editor
#include "td-core.h"                   // TextDocumentCore

// smbase
#include "smbase/array.h"              // ObjArrayStack
#include "smbase/exc.h"                // smbase::XMessage
#include "smbase/sm-override.h"        // OVERRIDE
#include "smbase/stringb.h"            // stringb


// fwd in this file
class HistoryStats;


// XHistory is thrown when a history event finds it cannot be applied
// because the TextDocumentCore state isn't consistent with the
// information stored in the event.
//
// NOTE:  All of the code paths that use XHistory are, as yet,
// untested.  To test them I need to implement a parser for the
// history concrete syntax, and make some histories that are
// inconsistent with some buffer contents.
class XHistory : public smbase::XMessage {
public:
  XHistory(char const *msg) : smbase::XMessage(stringb("XHistory: " << msg)) {}
  XHistory(XHistory const &obj) : smbase::XMessage(obj) {}
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
  virtual TextMCoord apply(TextDocumentCore &doc, bool reverse) const=0;

  // render this command as a text line, indented by 'indent' spaces
  virtual void print(std::ostream &sb, int indent) const=0;

  // account for this history record
  virtual void stats(HistoryStats &stats) const=0;
};


// Text insertion/deletion.
class HE_text : public HistoryElt {
public:      // data
  // Where in the document to make the modification.  If this is
  // a deletion, this is the left edge of the span.
  TextMCoord tc;

  // If true, this is an insertion; otherwise a deletion.
  bool insertion;

  // Text to insert or delete; may contain embedded NULs.
  ArrayStack<char> text;

private:     // funcs
  static void insert(TextDocumentCore &buf, TextMCoord tc,
                     ArrayStack<char> const &text, bool reverse);

public:      // funcs
  // This makes a copy of 'text'.
  HE_text(TextMCoord tc, bool insertion, char const *text, int textLen);
  ~HE_text();

  // HistoryElt funcs
  virtual Tag getTag() const OVERRIDE { return HE_TEXT; }
  virtual TextMCoord apply(TextDocumentCore &buf, bool reverse) const OVERRIDE;
  virtual void print(std::ostream &sb, int indent) const OVERRIDE;
  virtual void stats(HistoryStats &stats) const OVERRIDE;

  // 'apply', but static
  static TextMCoord static_apply(
    TextDocumentCore &buf, TextMCoord tc, bool insertion,
    ArrayStack<char> const &text, bool reverse);

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
  TextMCoord applySeqElt(TextDocumentCore &buf, int start, int end, int offset,
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
  TextMCoord applySeq(TextDocumentCore &buf, int start, int end, bool reverse) const;

  // apply a single element of the sequence, possibly in reverse
  TextMCoord applyOne(TextDocumentCore &buf, int index, bool reverse) const;

  // print, and mark the nth element of the history in the left
  // margin; if 'n' is outside the range of valid indices, no mark is
  // printed
  void printWithMark(std::ostream &sb, int indent, int n) const;


  // HistoryElt funcs
  virtual Tag getTag() const OVERRIDE { return HE_GROUP; }
  virtual TextMCoord apply(TextDocumentCore &buf, bool reverse) const OVERRIDE;
  virtual void print(std::ostream &sb, int indent) const OVERRIDE;
  virtual void stats(HistoryStats &stats) const OVERRIDE;
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
