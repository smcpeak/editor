// td-core.h
// Representation of a text document for use in a text editor.

#ifndef TD_CORE_H
#define TD_CORE_H

#include "td-core-fwd.h"               // fwds for this module

// editor
#include "byte-count.h"                // ByteCount
#include "byte-gap-array.h"            // ByteGapArray
#include "byte-index.h"                // ByteIndex
#include "line-count.h"                // LineCount
#include "line-gap-array.h"            // LineGapArray
#include "line-index.h"                // LineIndex
#include "positive-line-count.h"       // PositiveLineCount
#include "td-fwd.h"                    // TextDocument [n]
#include "td-line.h"                   // TextDocumentLine
#include "td-version-number.h"         // TD_VersionNumber
#include "textmcoord.h"                // TextMCoord

// smbase
#include "smbase/array.h"              // ArrayStack
#include "smbase/gdvalue-fwd.h"        // gdv::GDValue [n]
#include "smbase/rcserflist.h"         // RCSerfList
#include "smbase/refct-serf.h"         // SerfRefCount
#include "smbase/sm-noexcept.h"        // NOEXCEPT
#include "smbase/sobjlist.h"           // SObjList
#include "smbase/std-string-fwd.h"     // std::string [n]
#include "smbase/std-vector-fwd.h"     // std::vector [n]

// libc++
#include <optional>                    // std::optional


// A text document is a non-empty sequence of lines.
//
// To convert them to an on-disk text file, a single newline character
// is inserted *between* every pair of lines.  Consequently, the
// document consisting of one empty line corresponds to an on-disk file
// with 0 bytes.
//
// A line is a possibly empty sequence of Latin-1 code points, each
// in [0,255], except that newline (0x0A) is never present.
//
// The carriage return character (0x0D) is not special in any way to
// this class.  Lines that (on disk) end in CRLF are represented (in
// memory) as an array that ends with CR.
//
// TODO UTF-8: Change to Unicode code points.
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
//
class TextDocumentCore : public SerfRefCount {
private:     // instance data
  // This array is the spine of the document.  Every element is either
  // empty, meaning a blank line, or is a non-empty sequence of bytes
  // that represent the line's contents.
  LineGapArray<TextDocumentLine> m_lines;

  // The most-recently edited line number, or `nullopt` to mean that no
  // line's contents are stored.
  std::optional<LineIndex> m_recentIndex;

  // Length of the longest line this file has ever had, in bytes, not
  // counting any newline separator.  This is my poor-man's substitute
  // for a proper interval map, etc., to be able to answer the
  // 'maxLineLength()' query.
  ByteCount m_longestLengthSoFar;

  // If `m_recentIndex.has_value()`, then this holds the contents of
  // that line, and `m_lines[*m_recentIndex]` is empty.  Otherwise, this
  // is empty.
  ByteGapArray<char> m_recentLine;

  // Version number for the contents.  This starts at 1 and increases by
  // one each time the logical contents, i.e., the sequence of lines, is
  // modified.  An assertion failure exception is thrown if this would
  // overflow on an increment.
  TD_VersionNumber m_versionNumber;

  // invariants:
  //   - recent >= -1
  //   - if recent>=0, lines[recent] == NULL
  //   - if recent<0, recentLine.length() == 0
  //   - every lines[n] is NULL or valid and not empty
  //   - longestLengthSoFar >= 0

  // List of observers.  This is mutable because the highlighter wants
  // to declare, through its signature, that it does not modify the
  // document on which it operates, yet it needs to make itself an
  // observer of that document.  More generally, the set of observers is
  // considered outside the "data model" of this class, being more akin
  // to a form of control flow than primary data with meaning to the
  // user.
  mutable RCSerfList<TextDocumentObserver> m_observers;

  // Number of outstanding iterators.  This is used to check that we do
  // not do any document modification with an outstanding iterator.
  mutable int m_iteratorCount;

private:     // funcs
  // The 'M' reflects the fact that this directly accesses `m_lines`,
  // bypassing consideration of `m_recentLine`.
  TextDocumentLine const &getMLine(LineIndex lineIndex) const
    { return m_lines.get(lineIndex); }

  // Directly write an `m_lines` entry.
  void setMLine(LineIndex lineIndex, TextDocumentLine const &lineContents)
    { m_lines.set(lineIndex, lineContents); }

  // True if line `i` of `*this` and `obj` is equal.
  //
  // Requires: `i` is within bounds for both.
  bool equalLineAt(LineIndex i, TextDocumentCore const &obj) const;

  // strlen, but NULL yields 0 and '\n' is terminator, in bytes.
  //
  // TODO: Remove this.
  static int bufStrlen(char const *p);

  // bounds check line
  void bc(LineIndex line) const { xassert(validLine(line)); }

  // Counds check a coordinate.
  void bctc(TextMCoord tc) const { xassert(validCoord(tc)); }

  // copy the given line into 'recentLine', with given hints as to
  // where the gap should go and how big it should be
  // postcondition: recent==tc.line
  void attachRecent(TextMCoord tc, ByteCount insLength);

  // copy contents of 'recentLine', if any, back into lines[];
  // postcondition: recent==-1
  void detachRecent();

  // update 'longestLineSoFar', given the existence of a line
  // that is 'len' long
  void seenLineLength(ByteCount len);

  // Notify all observers of a total change to the document.
  void notifyTotalChange();

public:    // funcs
  TextDocumentCore();        // one empty line
  ~TextDocumentCore();

  // Check internal invariants, throwing assertion if broken.
  void selfCheck() const;

  // True if both documents represent the same sequence of lines, i.e.,
  // that their `getWholeFileString()` returns would be the same.
  bool operator==(TextDocumentCore const &obj) const;

  bool operator!=(TextDocumentCore const &obj) const
    { return !operator==(obj); }

  // ------------------------- GDValue export --------------------------
  // Logically, this object represents a versioned sequence of strings,
  // so this operator returns an ordered map with `version` and `lines`
  // attributes.
  operator gdv::GDValue() const;

  // Just get the sequence of strings for the lines.
  gdv::GDValue getAllLines() const;

  // Dump internals for test/debug.
  gdv::GDValue dumpInternals() const;

  // ---------------------- document shape ------------------------
  // # of lines stored; always at least 1
  PositiveLineCount numLines() const
    { return PositiveLineCount(m_lines.length()); }

  // True if `line` is valid, i.e., within range for this document.
  bool validLine(LineIndex line) const
    { return line.get() < numLines(); }

  // Index of the last valid line.  There is always at least one.
  LineIndex lastLineIndex() const;

  // True if the given line is empty.
  bool isEmptyLine(LineIndex line) const;

  // Length of a given line, not including any newline, in bytes.
  ByteCount lineLengthBytes(LineIndex line) const;

  // Same, but as a byte index.
  ByteIndex lineLengthByteIndex(LineIndex line) const;

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
  TextMCoord lineBeginCoord(LineIndex line) const;
  TextMCoord lineEndCoord(LineIndex line) const;

  // Maximum length of a line, not including any newline separator.
  //
  // TODO: Implement this properly (right now it just uses the length of
  // the longest line ever seen, even if that line is subsequently
  // deleted).
  ByteCount maxLineLengthBytes() const { return m_longestLengthSoFar; }

  // Number of lines in the file as a user would typically view it: if
  // the last line is empty, meaning the on-disk file ends in a newline,
  // then return the number of newline separators.  Otherwise return
  // newlines+1, the same as numLines().
  LineCount numLinesExcludingFinalEmpty() const;

  // Walk the given coordinate forwards (right, then down, when
  // distance>0) or backwards (left, then up, when distance<0) by the
  // given distance in *bytes* through the valid coordinates of the
  // file.  It must initially be a valid coordinate, but if by walking
  // we reach an invalid coordinate, then the function returns false,
  // leaving `tc` at the invalid coordinate if we walked past the end,
  // or the zero coordinate if we walked past the start.
  //
  // When walking between lines, it takes one byte to advance past the
  // implicit newline separating them.
  //
  // Otherwise it returns true and `tc` is valid afterward.
  bool walkCoordBytes(
    TextMCoord &tc /*INOUT*/, ByteDifference distance) const;

  // Same, but asserting we did not walk off either end.
  void walkCoordBytesValid(
    TextMCoord &tc /*INOUT*/, ByteDifference distance) const;

  // Compute the number of bytes in a range, *including* any implicit
  // newline separators the range spans.
  ByteCount countBytesInRange(TextMCoordRange const &range) const;

  // If `tc` is not valid, adjust it to the nearest coordinate that is.
  // Specifically, if the line is too large, set `tc` to `endCoord()`.
  // (Due to the `LineIndex` constraint, it cannot be negative.)  If the
  // line is ok but the byte index is out of bounds, confine it to its
  // line.  If the byte index points into the middle of a multibyte
  // character, move it to the start of that character.  (TODO: The
  // multibyte part is unimplemented.)
  //
  // Return true iff a change was made.
  bool adjustMCoord(TextMCoord /*INOUT*/ &tc) const;

  // Adjust both endpoints of range, then if end < start, set the end to
  // the start.  Return true iff a change was made.
  bool adjustMCoordRange(TextMCoordRange /*INOUT*/ &range) const;

  // -------------------------- line contents --------------------------
  // Get part of a line's contents, starting at 'tc' and getting
  // 'numBytes'.  All bytes must be in the line now.  The retrieved text
  // never includes the newline character, nor a terminating NUL.
  //
  // The retrieved bytes are appended to 'dest'.
  void getPartialLine(TextMCoord tc,
    ArrayStack<char> /*INOUT*/ &dest, ByteCount numBytes) const;

  // Retrieve text that may span line separators, which are represented
  // in the returned string as newlines.  The span begins at 'tc' (which
  // must be valid) and proceeds for 'numBytes' bytes, but if that goes
  // beyond the end then this simply returns false without changing
  // 'dest' (otherwise true).  If it returns true then exactly
  // 'textLenBytes' bytes have been appended to 'dest'.
  bool getTextSpanningLines(TextMCoord tc,
    ArrayStack<char> /*INOUT*/ &dest, ByteCount numBytes) const;

  // Get using a range.
  void getTextForRange(TextMCoordRange const &range,
    ArrayStack<char> /*INOUT*/ &dest) const;

  // Get a complete line of text, not including the newline.  'line'
  // must be within range.  Result is appended to 'dest'.
  void getWholeLine(LineIndex line, ArrayStack<char> /*INOUT*/ &dest) const;

  // Same, but returning the line as a string.
  std::string getWholeLineString(LineIndex line) const;

  // If `validLine(lineIndex)`, return `getWholeLineString(lineIndex)`.
  // Otherwise, return a string describing the out-of-range error, which
  // refers to the file as having `fname`.
  std::string getWholeLineStringOrRangeErrorMessage(
    LineIndex lineIndex,
    std::string const &fname) const;

  // Return the number of consecutive spaces and tabs at the start of
  // the given line, as a byte count.
  ByteCount countLeadingSpacesTabs(LineIndex line) const;

  // Return the number of consecutive spaces and tabs at the end of
  // the given line, as a byte count.
  ByteCount countTrailingSpacesTabs(LineIndex line) const;

  // ------------------- Core manipulation interface -------------------
  // This interface is deliberately very simple to *implement*: you are
  // either inserting or removing *blank* lines, or are editing the
  // contents of a *single* line.  TextDocumentEditor, built on top of
  // this one, provides a more convenient interface for clients.

  // insert a new blank line, where the new line will be line 'line';
  // 'line' must be in [0,numLines()]
  void insertLine(LineIndex line);

  // delete a blank line; the line must *already* be blank!  also,
  // you can't delete the last line
  void deleteLine(LineIndex line);

  // Insert text into a given line, starting at the given coord, which
  // must be valid.
  //
  // Requires: `text` must *not* contain the '\n' character.
  void insertText(TextMCoord tc, char const *text, ByteCount lengthBytes);
  void insertString(TextMCoord tc, std::string const &str);

  // Delete 'length' bytes at and the right of 'tc'.
  void deleteTextBytes(TextMCoord tc, ByteCount lengthBytes);

  // --------------------- Multi-line manipulation ---------------------
  // Although a above comment mentions `TextDocumentEditor`, an editor
  // cannot be created to operate on an arbitrary `TextDocumentCore`,
  // only a `TextDocument`.  So, here is one more convenient editing
  // function.

  // Replace the text in `range` (which must have valid coordinates with
  // start <= end) with `text`.  The range can span multiple lines, and
  // `text` can have newlines in it.
  void replaceMultilineRange(
    TextMCoordRange const &range, std::string const &text);

  // ---------------------- whole file -------------------------
  // clear buffer contents, returning to just one empty line
  void clear();

  // Return the entire contents of the file as a byte sequence.
  std::vector<unsigned char> getWholeFile() const;

  // Replace the file contents with those from 'bytes'.
  void replaceWholeFile(std::vector<unsigned char> const &bytes);

  // Same, but using `string`.
  std::string getWholeFileString() const;
  void replaceWholeFileString(std::string const &str);

  // Get the current version number.
  //
  // When a change happens, the version number is incremented *before*
  // observers are notified, so they see the new number.
  TD_VersionNumber getVersionNumber() const { return m_versionNumber; }

  // Increment `m_versionNumber`.  Also check that there are no
  // outstanding iterators.
  //
  // Normally this is done when the content changes, but it is fine to
  // bump the version even when nothing has changed.
  void bumpVersionNumber();

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
  // Iterate over the bytes in a line, not including any newline
  // separtor.
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

    // Number of bytes in the line we are iterating over.
    ByteCount m_totalBytes;

    // Byte offset of the iterator within the current line.
    ByteIndex m_byteOffset;

  public:      // funcs
    // Iterate over the given line number.  While this object exists, it
    // is not possible to modify the document.
    //
    // Unlike most TDC methods, here, 'line' can be out of bounds, with
    // the result being the same as an empty line.  This choice is made
    // since it is sometimes awkward to avoid creating an iterator.
    LineIterator(TextDocumentCore const &tdc, LineIndex line);

    ~LineIterator();

    // True if the iterator has more data, i.e., it has *not* reached
    // the end of the line.
    bool has() const;

    // Current byte offset within the iterated line.  It *is* legal to
    // call this when '!has()', in which case it returns the length of
    // the line in bytes.
    ByteIndex byteOffset() const { return m_byteOffset; }

    // Current byte value in [0,255], excluding newline (0x0A).
    //
    // Requires: has()
    int byteAt() const;

    // Advance to the next byte.
    //
    // Requires: has()
    void advByte();
  };
  friend LineIterator;
};


// Iterate over all of the line indices in `doc`.
#define FOR_EACH_LINE_INDEX_IN(indexVar, doc) \
  for (LineIndex indexVar(0); indexVar < (doc).numLines(); ++indexVar)


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
  virtual ~TextDocumentObserver();

  // These are analogues of the TextDocumentCore manipulation interface, but
  // we also pass the TextDocumentCore itself so the observer doesn't need
  // to remember which buffer it's observing.  These are called
  // *after* the TextDocumentCore updates its internal representation.  The
  // default implementations do nothing.
  virtual void observeInsertLine(TextDocumentCore const &doc, LineIndex line) NOEXCEPT;
  virtual void observeDeleteLine(TextDocumentCore const &doc, LineIndex line) NOEXCEPT;
  virtual void observeInsertText(TextDocumentCore const &doc, TextMCoord tc, char const *text, ByteCount lengthBytes) NOEXCEPT;
  virtual void observeDeleteText(TextDocumentCore const &doc, TextMCoord tc, ByteCount lengthBytes) NOEXCEPT;

  // The document has changed in some major way that does not easily
  // allow for incremental updates.  Observers must refresh completely.
  virtual void observeTotalChange(TextDocumentCore const &doc) NOEXCEPT;

  /* This notification is sent to observers if some data in one of the
     higher-level document classes changed (or might have changed), and
     that change should trigger a redraw of a widget showing this
     document.  Currently, there are three such cases:

     1. The "has unsaved changes" property changes
        (`TextDocument::unsavedChanges()`).

     2. Updated language diagnostics were received from an LSP server
        (`NamedTextDocument::m_diagnostics`) or a document version was
        sent to the server and the diagnostics are pending
        (`NamedTextDocument::m_observationRecorder`).

     3. The file contents were reloaded from disk, which means they are
        out of date w.r.t. any previously received diagnostics.

     Since the only expected reaction is a redraw, there's not a high
     penalty for firing this off when unnecessary, but it's not entirely
     negligible either (we don't want to redraw the entire UI every time
     there is incidental network activty, for example).

     Design-wise, having a notification here that pertains to data that
     `TextDocumentCore` lacks is not ideal, but the only obvious
     alternative is to have parallel observers at each level, which
     seems like overkill.
  */
  virtual void observeMetadataChange(TextDocumentCore const &doc) NOEXCEPT;
};


#endif // TD_CORE_H
