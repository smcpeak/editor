// td-core.h
// Representation of a text document for use in a text editor.

#ifndef TD_CORE_H
#define TD_CORE_H

#include "td-core-fwd.h"               // fwds for this module

// editor
#include "gap.h"                       // GapArray
#include "td-fwd.h"                    // TextDocument
#include "td-line.h"                   // TextDocumentLine
#include "textmcoord.h"                // TextMCoord

// smbase
#include "smbase/array.h"              // ArrayStack
#include "smbase/rcserflist.h"         // RCSerfList
#include "smbase/refct-serf.h"         // SerfRefCount
#include "smbase/sm-noexcept.h"        // NOEXCEPT
#include "smbase/sobjlist.h"           // SObjList
#include "smbase/str.h"                // string

// libc++
#include <cstdint>                     // std::uint64_t
#include <vector>                      // std::vector


// A text document is a non-empty sequence of lines.
//
// To convert them to an on-disk text file, a single newline character
// is inserted *between* every pair of lines.  Consequently, the
// document consisting of one empty line corresponds to an on-disk file
// with 0 bytes.
//
// A line is a possibly empty sequence of Latin-1 code points, each
// in [0,255].  TODO UTF-8: Change to Unicode code points.
//
// All uses of char* in this interface use Latin-1 encoding.  If 'char'
// is signed, the values [-128,-1] are interpreted as naming the code
// points in [128,255] instead.  TODO: Change to UTF-8 encoding.
//
// All line lengths in this class are counts of *bytes*.
//
// This class is the "core" of a text document because it does not have
// any facilities for undo and redo.  Those are added by TextDocument
// (declared in td.h).
class TextDocumentCore : public SerfRefCount {
public:      // types
  // Type use to record document version numbers.
  typedef std::uint64_t VersionNumber;

private:     // instance data
  // This array is the spine of the document.  Every element is either
  // empty, meaning a blank line, or is a '\n'-terminated sequence of
  // bytes that represent the line's contents.
  //
  // TODO: I should not store the newline.  That is inconsistent with
  // the "blank line" case.
  GapArray<TextDocumentLine> m_lines;

  // the most-recently edited line number, or -1 to mean that
  // no line's contents are stored
  int m_recent;

  // Length of the longest line this file has ever had, in bytes.  This
  // is my poor-man's substitute for a proper interval map, etc., to be
  // able to answer the 'maxLineLength()' query.
  int m_longestLengthSoFar;

  // if recent != -1, then this holds the contents of that line,
  // and lines[recent] is NULL
  GapArray<char> m_recentLine;

  // Version number for the contents.  This starts at 1 and increases by
  // one each time the logical contents, i.e., the sequence of lines, is
  // modified.  An assertion failure exception is thrown if this would
  // overflow on an increment.
  VersionNumber m_versionNumber;

  // invariants:
  //   - recent >= -1
  //   - if recent>=0, lines[recent] == NULL
  //   - if recent<0, recentLine.length() == 0
  //   - every lines[n] is NULL or valid and not empty
  //   - longestLengthSoFar >= 0

  // List of observers.  This is mutable because the highlighter wants
  // to declare, through its signature, that it does not modify the
  // document on which it operates, yet it needs to make itself an
  // observer of that document.
  mutable RCSerfList<TextDocumentObserver> m_observers;

  // Number of outstanding iterators.  This is used to check that we do
  // not do any document modification with an outstanding iterator.
  mutable int m_iteratorCount;

private:     // funcs
  // strlen, but NULL yields 0 and '\n' is terminator, in bytes.
  static int bufStrlen(char const *p);

  // bounds check line
  void bc(int line) const { xassert(0 <= line && line < numLines()); }

  // Counds check a coordinate.
  void bctc(TextMCoord tc) const { xassert(validCoord(tc)); }

  // copy the given line into 'recentLine', with given hints as to
  // where the gap should go and how big it should be
  // postcondition: recent==tc.line
  void attachRecent(TextMCoord tc, int insLength);

  // copy contents of 'recentLine', if any, back into lines[];
  // postcondition: recent==-1
  void detachRecent();

  // update 'longestLineSoFar', given the existence of a line
  // that is 'len' long
  void seenLineLength(int len);

  // Notify all observers of a total change to the document.
  void notifyTotalChange();

  // Increment `m_versionNumber`.  Also check that there are no
  // outstanding iterators.
  void bumpVersionNumber();

public:    // funcs
  TextDocumentCore();        // one empty line
  ~TextDocumentCore();

  // Check internal invariants, throwing assertion if broken.
  void selfCheck() const;

  // ---------------------- document shape ------------------------
  // # of lines stored; always at least 1
  int numLines() const { return m_lines.length(); }

  // True if the given line is empty.
  bool isEmptyLine(int line) const;

  // Length of a given line, not including the '\n', in bytes.
  int lineLengthBytes(int line) const;

  // True if 'tc' has a line in [0,numLines()-1] and a byteIndex in
  // [0,lineLengthBytes(line)] that is not in the middle of a multibyte
  // code unit sequence.  That means, for example, one can insert a
  // code point and it will be adjacent to an existing code point.
  //
  // It *is* valid for a coordinate to refer to a composing code point.
  // As far as TextDocumentCore is concerned, character composition is
  // beyond its scope of concern.  (I treat it as a layout issue.)
  bool validCoord(TextMCoord tc) const;

  // True if both endpoints are valid and start >= end.
  bool validRange(TextMCoordRange const &range) const;

  // Return the first valid coordinate.
  TextMCoord beginCoord() const { return TextMCoord(); }

  // Return the last valid coordinate.
  TextMCoord endCoord() const;

  // Coordinates for begin and end of a line, which must be valid.
  TextMCoord lineBeginCoord(int line) const;
  TextMCoord lineEndCoord(int line) const;

  // Maximum length of a line.  TODO: Implement this properly (right
  // now it just uses the length of the longest line ever seen, even
  // if that line is subsequently deleted).
  int maxLineLengthBytes() const { return m_longestLengthSoFar; }

  // Number of lines in the file as a user would typically view it: if
  // the file ends in a newline, then return the number of newlines.
  // Otherwise return newlines+1, the same as numLines().
  int numLinesExceptFinalEmpty() const;

  // Walk the given coordinate forwards (right, then down, when
  // distance>0) or backwards (left, then up, when distance<0) by the
  // given distance in *bytes* through the valid coordinates of the
  // file.  It must initially be a valid coordinate, but if by walking
  // we reach an invalid coordinate, then the function simply returns
  // false (otherwise true).
  bool walkCoordBytes(TextMCoord &tc, int distance) const;

  // Compute the number of bytes in a range.
  int countBytesInRange(TextMCoordRange const &range) const;

  // If `tc` is not valid, adjust it to the nearest coordinate that is.
  // Specifically, if the line is too large, set `tc` to `endCoord()`,
  // and if it is negative, then set `tc` to `beginCoord()`.  If the
  // line is ok but the byte index is out of bounds, similarly confine
  // it to its line.  If the byte index points into the middle of a
  // multibyte character, move it to the start of that character.
  // Return true iff a change was made.
  bool adjustMCoord(TextMCoord /*INOUT*/ &tc) const;

  // Adjust both endpoints of range, then if end < start, set the end to
  // the start.  Return true iff a change was made.
  bool adjustMCoordRange(TextMCoordRange /*INOUT*/ &range) const;

  // --------------------- line contents ------------------------
  // Get part of a line's contents, starting at 'tc' and getting
  // 'numBytes'.  All bytes must be in the line now.  The retrieved
  // text never includes the '\n' character, nor a terminating NUL.
  //
  // The retrieved bytes are appended to 'dest'.
  void getPartialLine(TextMCoord tc,
    ArrayStack<char> /*INOUT*/ &dest, int numBytes) const;

  // Retrieve text that may span line boundaries.  Line boundaries are
  // represented in the returned string as newlines.  The span begins at
  // 'tc' (which must be valid) and proceeds for 'numBytes' bytes, but
  // if that goes beyond the end then this simply returns false without
  // changing 'dest' (otherwise true).  If it returns true then exactly
  // 'textLenBytes' bytes have been appended to 'dest'.
  bool getTextSpanningLines(TextMCoord tc,
    ArrayStack<char> /*INOUT*/ &dest, int numBytes) const;

  // Get using a range.
  void getTextForRange(TextMCoordRange const &range,
    ArrayStack<char> /*INOUT*/ &dest) const;

  // Get a complete line of text, not including the newline.  'line'
  // must be within range.  Result is appended to 'dest'.
  void getWholeLine(int line, ArrayStack<char> /*INOUT*/ &dest) const;

  // Same, but returning the line as a string.
  string getWholeLineString(int line) const;

  // Return the number of consecutive spaces and tabs at the start of
  // the given line, as a byte count.
  int countLeadingSpacesTabs(int line) const;

  // Return the number of consecutive spaces and tabs at the end of
  // the given line, as a byte count.
  int countTrailingSpacesTabs(int line) const;

  // ----------------- manipulation interface -------------------
  // This interface is deliberately very simple to *implement*: you are
  // either inserting or removing *blank* lines, or are editing the
  // contents of a *single* line.  TextDocumentEditor, built on top of
  // this one, provides a more convenient interface for clients.

  // insert a new blank line, where the new line will be line 'line';
  // 'line' must be in [0,numLines()]
  void insertLine(int line);

  // delete a blank line; the line must *already* be blank!  also,
  // you can't delete the last line
  void deleteLine(int line);

  // Insert text into a given line, starting at the given coord, which
  // must be valid.  The inserted text must *not* contain the '\n'
  // character.
  void insertText(TextMCoord tc, char const *text, int lengthBytes);
  void insertString(TextMCoord tc, string const &str)
    { insertText(tc, str.c_str(), str.length()); }

  // Delete 'length' bytes at and the right of 'tc'.
  void deleteTextBytes(TextMCoord tc, int lengthBytes);

  // ---------------------- whole file -------------------------
  // clear buffer contents, returning to just one empty line
  void clear();

  // Return the entire contents of the file as a byte sequence.
  std::vector<unsigned char> getWholeFile() const;

  // Replace the file contents with those from 'bytes'.
  //
  // Note: Currently, the file parser assumes that LF is the sole line
  // terminator.  Any CR characters in the file become part of the
  // in-memory line contents, and will then be written out as such as
  // well, like any other character.  This is not ideal of course.
  void replaceWholeFile(std::vector<unsigned char> const &bytes);

  // Get the current version number.
  //
  // When a change happens, the version number is incremented *before*
  // observers are notified, so they see the new number.
  VersionNumber getVersionNumber() const { return m_versionNumber; }

  // ---------------------- observers ---------------------------
  // Add an observer.  It must not already be there.  This is 'const'
  // for the same reason 'observers' is mutable.
  void addObserver(TextDocumentObserver *observer) const;

  // Remove an observer.  It must be present now.
  void removeObserver(TextDocumentObserver *observer) const;

  // Return true if 'observer' is among the current observers.
  bool hasObserver(TextDocumentObserver const *observer) const;

  // Send the 'observeMetadataChange' message to all observers.
  void notifyMetadataChange() const;

  // ---------------------- debugging ---------------------------
  // print internal rep
  void dumpRepresentation() const;

  // how much memory am I using?
  void printMemStats() const;

  // ---------------------- iterator ----------------------------
public:      // types
  // Iterate over the bytes in a line.
  //
  // TODO UTF-8: Allow iteration over code points.
  class LineIterator {
    NO_OBJECT_COPIES(LineIterator);

  private:     // instance data
    // Document whose line we are iterating over.  Never NULL.
    RCSerf<TextDocumentCore const> m_tdc;

    // If true, we are iterating over the "recent" line.
    bool m_isRecentLine;

    // NULL if the line being iterated is empty.  Otherwise, if
    // '!m_isRecentLine', this is the pointer to the start of the line
    // data.  Otherwise NULL (use 'm_tdc.m_recentLine').
    char const *m_nonRecentLine;

    // Byte offset of the iterator within the current line.
    int m_byteOffset;

  public:      // funcs
    // Iterate over the given line number.  While this object exists, it
    // is not possible to modify the document.
    //
    // Unlike most TDC methods, here, 'line' can be out of bounds, with
    // the result being the same as an empty line.  This choice is made
    // since it is sometimes awkward to avoid creating an iterator.
    LineIterator(TextDocumentCore const &tdc, int line);

    ~LineIterator();

    // True if the iterator has more data, i.e., it has *not* reached
    // the end of the line.
    bool has() const;

    // Current byte offset within the iterated line.  It *is* legal to
    // call this when '!has()', in which case it returns the length of
    // the line in bytes.
    int byteOffset() const { return m_byteOffset; }

    // Current byte value in [0,255].  Requires 'has()'.
    int byteAt() const;

    // Advance to the next byte.  Requires 'has()'.
    void advByte();
  };
  friend LineIterator;
};


inline bool isSpaceOrTab(int c)
{
  return c == ' ' || c == '\t';
}


// Interface for observing changes to a TextDocumentCore.
//
// All methods have 'NOEXCEPT'.  From the perspective of the observee,
// these cannot fail, as there is nothing the observee can do about it
// nor an appropriate way to report it.  Observers are obligated to
// catch any exceptions they throw.
//
// This inherits SerfRefCount to catch bugs where an observer is
// destroyed without removing itself from the document's observer list.
// The inheritance is virtual in order to allow an object to also
// implement another observer interface that also inherits SerfRefCount.
class TextDocumentObserver : virtual public SerfRefCount {
public:      // static data
  static int s_objectCount;

public:      // funcs
  TextDocumentObserver();
  TextDocumentObserver(TextDocumentObserver const &obj);
  virtual ~TextDocumentObserver();     // virtual to silence warning

  // These are analogues of the TextDocumentCore manipulation interface, but
  // we also pass the TextDocumentCore itself so the observer doesn't need
  // to remember which buffer it's observing.  These are called
  // *after* the TextDocumentCore updates its internal representation.  The
  // default implementations do nothing.
  virtual void observeInsertLine(TextDocumentCore const &doc, int line) NOEXCEPT;
  virtual void observeDeleteLine(TextDocumentCore const &doc, int line) NOEXCEPT;
  virtual void observeInsertText(TextDocumentCore const &doc, TextMCoord tc, char const *text, int lengthBytes) NOEXCEPT;
  virtual void observeDeleteText(TextDocumentCore const &doc, TextMCoord tc, int lengthBytes) NOEXCEPT;

  // The document has changed in some major way that does not easily
  // allow for incremental updates.  Observers must refresh completely.
  virtual void observeTotalChange(TextDocumentCore const &doc) NOEXCEPT;

  // This notification is sent to observers if some data in one of the
  // higher-level document classes changed (or might have changed), and
  // that change should trigger a redraw of a widget showing this
  // document.  Currently, there are two such cases:
  //
  // 1. The "has unsaved changes" property changes
  //    (`TextDocument::unsavedChanges()`).
  //
  // 2. Updated language diagnostics were received from an LSP server
  //    (`NamedTextDocument::m_diagnostics`).
  //
  // Since the only expected reaction is a redraw, there's not a high
  // penalty for firing this off when unnecessary, but it's not entirely
  // negligible either (we don't want to redraw the entire UI every time
  // there is incidental network activty, for example).
  //
  // Design-wise, having a notification here that pertains to data that
  // `TextDocumentCore` lacks is not ideal, but the only obvious
  // alternative is to have parallel observers at each level, which
  // seems like overkill.
  //
  virtual void observeMetadataChange(TextDocumentCore const &doc) NOEXCEPT;
};


#endif // TD_CORE_H
