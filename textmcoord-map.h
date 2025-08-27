// textmcoord-map.h
// `TextMCoordMap` class.

// See license.txt for copyright and terms of use.

#ifndef EDITOR_TEXTMCOORD_MAP_H
#define EDITOR_TEXTMCOORD_MAP_H

#include "textmcoord-map-fwd.h"        // fwds for this module

#include "line-count.h"                // LineCount, PositiveLineCount
#include "line-gap-array.h"            // LineGapArray
#include "line-index.h"                // LineIndex
#include "td-core-fwd.h"               // TextDocumentCore
#include "textmcoord.h"                // TextMCoord

#include "smbase/compare-util-iface.h" // DEFINE_FRIEND_RELATIONAL_OPERATORS
#include "smbase/gdvalue-fwd.h"        // gdv::GDValue
#include "smbase/gdvalue-parser-fwd.h" // gdv::GDValueParser
#include "smbase/sm-macros.h"          // NO_OBJECT_COPIES

#include <optional>                    // std::optional
#include <set>                         // std::set


/*
  This class implements an associative map from a range of some text
  document to some value, where the range endpoints are adjusted in
  response to edits performed on the document.

  As a simple example, we might start by creating associated spans that
  look like:

    0  text text
    1  text [span1] text
    2  text text
    3  text [span2
    4        span2
    5        span2] more text
    6  text text

  The first column is the 0-based line index.  There are two entries
  here, one for `span1` and one for `span2`.

  After inserting a line at index 3, the map+document looks like:

    0  text text
    1  text [span1] text
    2  text text
    3  inserted text
    4  text [span2
    5        span2
    6        span2] more text
    7  text text

  Then, after deleting line 5, we have:

    0  text text
    1  text [span1] text
    2  text text
    3  inserted text
    4  text [span2
    5        span2] more text
    6  text text

  An so on.  Inserting text moves all later endpoints right and down.
  Deleting text moves later endpoints left and up, and endpoints within
  the deleted region move to its start.  Entries are never removed,
  although deletions can cause their endpoints to coincide.

  The design is intended to perform well when most spans only intersect
  a single line, multi-line spans are fairly short, and the total number
  of spans is reasonably small.  In particular, the primary intended use
  of spans is to record the text described by compiler error messages.

  The map can operate in two "modes", one where it merely holds the
  diagnostic data without being able to update it, and the other where
  it can perform updates.  The latter mode requires that the map know
  the number of lines in the document being tracked; see
  `canTrackUpdates()`.

  The class `TextDocumentDiagnostics` (`td-diagnostics.h`) is built on
  top of this one and is what is intended to be directly used by an
  editor application.  This class merely provides the algorithmic core.
*/
class TextMCoordMap {
  // Allow test code to access private members.
  friend class TextMCoordMap_LineData_Tester;

  // Not impossible, just not implemented, and automatic would be wrong.
  void operator=(TextMCoordMap const &obj) = delete;

public:      // types
  // In this class, values are simply integers.  Clients are expected to
  // have some sort of auxiliary array of more meaningful values to
  // associate with ranges, and use indices into that array as the
  // `Value`s here.
  typedef int Value;

  // "Document entry", an element stored in the map.
  //
  // The "document" part of the name indicates the scope of its
  // boundaries is the entire document, as opposed to `LineEntry`, which
  // has boundaries that only apply to one line.
  //
  // This class is used as part of the interface to the map, but the
  // data is not stored internally this way for efficiency reasons.
  class DocEntry {
  public:      // data
    // Range of text associated with the value.  This range is
    // normalized in the sense that its start is less than or equal to
    // its end.
    TextMCoordRange m_range;

    // The associated value.
    Value m_value;

  public:      // methods
    DocEntry(TextMCoordRange range, Value value);

    void selfCheck() const;

    operator gdv::GDValue() const;

    DECLARE_COMPARETO_AND_DEFINE_RELATIONALS(DocEntry)
  };

  // Data returned by `getEntriesForLine`, describing the entries that
  // intersect that line.
  class LineEntry final {
  public:      // data
    // If set, the index where the range starts on this line.  If not
    // set, the range begins on a previous line.
    std::optional<int> m_startByteIndex;

    // If set, the index where the range ends on this line.  If not set,
    // the range ends on a subsequent line.
    //
    // Invariant: If both indices are set, then start <= end.
    std::optional<int> m_endByteIndex;

    // The associated value.
    Value m_value;

  public:
    LineEntry(
      std::optional<int> startByteIndex,
      std::optional<int> endByteIndex,
      Value value);

    void selfCheck();

    operator gdv::GDValue() const;

    // There's no particular need to be able to parse GDV->LineEntry,
    // but I've wanted it for other nearby types, so this is a
    // preliminary experiment.
    explicit LineEntry(gdv::GDValueParser const &p);

    DECLARE_COMPARETO_AND_DEFINE_RELATIONALS(LineEntry)
  };

private:     // types
  // Record of a range that is entirely contained on one line.
  class SingleLineSpan {
  public:      // data
    // 0-based byte index of the start.
    int m_startByteIndex;

    // Byte index of the end.  The spanned region is [start,end), i.e.,
    // it does not include the byte with at end index.
    //
    // Invariant: 0 <= m_startByteIndex <= m_endByteIndex.
    int m_endByteIndex;

    // The value associated with this span.
    Value m_value;

  public:      // methods
    SingleLineSpan(int startByteIndex, int endByteIndex, Value value);

    void selfCheck() const;

    operator gdv::GDValue() const;

    DECLARE_COMPARETO_AND_DEFINE_RELATIONALS(SingleLineSpan)
  };

  // Record of the start or end of a range associated with a value.
  class Boundary {
  public:      // data
    // 0-based byte index of the boundary within its line.  If this is
    // a start point, the named byte is included in the range.  If this
    // is an end point, the named byte is *not* included.
    int m_byteIndex;

    // The associated value.
    Value m_value;

  public:      // methods
    Boundary(int byteIndex, Value value);

    void selfCheck() const;

    operator gdv::GDValue() const;

    DECLARE_COMPARETO_AND_DEFINE_RELATIONALS(Boundary)
  };

  // Data about values that are associated with ranges that start, end,
  // or span a particular line.  An object of this type is only created
  // for a line that has at least one intersecting span.
  class LineData {
  public:      // data
    // Values associated with spans entirely on one line.
    std::set<SingleLineSpan> m_singleLineSpans;

    // Values whose ranges start on this line and continue past it.
    std::set<Boundary> m_startsHere;

    // Values whose ranges span the line (start above, end below).
    std::set<Value> m_continuesHere;

    // Values whose ranges end on this line, having begun above it.
    std::set<Boundary> m_endsHere;

  private:     // methods
    // Insertion as it affects `m_singleLineSpans`.
    void insertBytes_spans(int insStart, int lengthBytes);

    // Insertion as it affects one of the boundary sets.
    static void insertBytes_boundaries(
      std::set<Boundary> &boundaries,
      int insStart,
      int lengthBytes);

    // Deletion as it affects `m_singleLineSpans`.
    void deleteBytes_spans(int delStart, int lengthBytes);

    // Deletion as it affects one of the boundary sets.
    static void deleteBytes_boundaries(
      std::set<Boundary> &boundaries,
      int delStart,
      int lengthBytes);

  public:      // methods
    ~LineData();
    LineData(LineData const &obj);
    LineData();

    bool operator==(LineData const &obj) const;
    bool operator!=(LineData const &obj) const
      { return !operator==(obj); }

    void selfCheck() const;

    operator gdv::GDValue() const;

    // Modify the associated spans to reflect inserting `lengthBytes`
    // bytes starting at `insStart`.
    void insertBytes(int insStart, int lengthBytes);

    // Modify the associated spans to reflect deleting `lengthBytes`
    // bytes starting at `delStart`.
    void deleteBytes(int delStart, int lengthBytes);

    // Remove from `m_endsHere` the boundary that applies to `v`, which
    // must exist.  Return the byte index it carried.
    int removeEnd_getByteIndex(Value v);

    // Return the `LineEntry`s for this line.
    std::set<LineEntry> getLineEntries() const;

    // The largest byte index mentioned by any end point.
    std::optional<int> largestByteIndex() const;

    // Ensure the coordinates are valid for `line` in `doc`.
    void adjustForDocument(TextDocumentCore const &doc, LineIndex line);

    // In the somewhat rare case that the last line is deleted but it
    // had an entry, add an end for the value to this line, the one that
    // was just before the last line.  This line must either have a
    // start or a continuation of `v`.
    void addEndBoundaryToLastLine(Value v);
  };

  // Array of owner pointers to `LineData`.
  typedef LineGapArray<LineData * NULLABLE /*owner*/> LineDataGapArray;

private:     // data
  // Set of values that are part of some range.
  //
  // Invariant: This is the set of values mentioned across all of
  // `m_lineData`, which is also the set of values mentioned in all of
  // the `insert()` calls since the last call to `clear()`.
  std::set<Value> m_values;

  /* Map from 0-based line number to associated data.

     Invariant: For every value `v` in `m_values`, either:

     * it occurs as the value of exactly one SingleLineSpan and not in
       any Boundary or continuation, or

     * it occurs as the value of exactly one start Boundary, exactly one
       stop Boundary (which is after the start), all intervening
       continuations, and no SingleLineSpans.

     Furthermore, the last entry in this array, if it is not empty, is
     not null.  That is, it is only as long as it needs to be in order
     to hold the `LineData` closest to the end.
  */
  LineDataGapArray m_lineData;

  // If set, this is the number of lines in the file, i.e., the number
  // of newline characters in the document plus one (since newline
  // characters are treated as line *separators* in this context).
  //
  // We need this information in order to properly handle the case of
  // deleting the last line in the file, since otherwise we would not
  // know when to diagnostics on a deleted line up one line (normally
  // they keep their line number).
  //
  // However, it is optional because for part of the lifecycle of this
  // object, it is just a passive container for diagnostics, not
  // associated with any document contents.  In that mode, it cannot
  // update the diagnostics in response to document changes.
  //
  // Invariant: if has value, m_lineData.length() <= m_numLines
  // Invariant: if has value, m_numLines >= 1
  std::optional<PositiveLineCount> m_numLines;

private:     // methods
  // Get the data for `line`, creating it if necessary.
  LineData *getOrCreateLineData(LineIndex line);

  // Get the data, or nullptr if there is no data for that line.  This
  // allows `line` to be out of range, including negative.
  LineData const * NULLABLE getLineDataC(LineIndex line) const;

  // Same, but non-const.
  LineData * NULLABLE getLineData(LineIndex line);

  // Assert:
  //   0 <= line &&
  //   m_numLines.has_value() ==>
  //     line < *m_numLines
  void validateLineIndex(LineIndex line) const;

  // True if `a` and `b` are (logically) equal.
  static bool equalLineData(
    LineDataGapArray const &a,
    LineDataGapArray const &b);

  // Ensure all line indices are in [0, maxNumLines-1].
  //
  // Requires: maxNumLines > 0
  void confineLineIndices(PositiveLineCount maxNumLines);

public:      // methods
  ~TextMCoordMap();

  TextMCoordMap(TextMCoordMap const &obj);

  // Make an empty map corresponding to a document with the given
  // number of lines initially.
  //
  // Requires: if has value, numLines > 0
  //
  // TODO: I think I only ever pass `nullopt` as `numLines`.
  explicit TextMCoordMap(std::optional<PositiveLineCount> numLines);

  bool operator==(TextMCoordMap const &obj) const;
  bool operator!=(TextMCoordMap const &obj) const
    { return !operator==(obj); }

  // Assert all invariants.
  void selfCheck() const;


  // ---- Manipulate the mapping directly ----
  // Add an entry.  Requires that its value not already be in the map.
  // Also requires `validRange(entry.m_range)`.
  void insertEntry(DocEntry entry);

  // There is not currently a way to remove individual entries because I
  // don't anticipate needing to do so.

  // Remove all entries, but leave the number of lines as-is.
  void clearEntries();

  // Remove entries and set `numLines`, which must be positive if it has
  // a value.
  void clearEverything(std::optional<PositiveLineCount> numLines);

  // Adjust all diagnostic ranges so their line indices are in [0,
  // numLines-1].  We do this after receiving diagnostics for a
  // potentially old version of a document, for which we only know the
  // line count.  This enables tracking updates.
  //
  // This is normally done before `adjustForDocument`.
  void setNumLinesAndAdjustAccordingly(PositiveLineCount numLines);

  // Adjust all diagnostic ranges to be valid for `doc`.  See the
  // comments on `TextDocumentDiagnostics::adjustForDocument` for
  // motivation, etc.
  //
  // This sets `m_numLines`, thus enabling `canTrackUpdates()`.
  void adjustForDocument(TextDocumentCore const &doc);


  // ---- Manipulate the mapping indirectly via text insert/delete ----
  // All of the methods in this section require `canTrackUpdates()`.

  // Insert `count` lines starting at `line`, shifting all range
  // boundaries that come after the line down.
  void insertLines(LineIndex line, LineCount count);

  // Delete `count` lines, starting at `line` and going down, shifting
  // all later boundaries up.  A boundary that is on the line will be
  // shifted to the start of the line.  Thus, any range that started and
  // ended on the line will, afterward, have an associated zero-length
  // range at the start of the line; but it will still be in the map.
  void deleteLines(LineIndex line, LineCount count);

  // Insert characters on a single line, at `tc`, shifting later
  // boundaries to the right.
  void insertLineBytes(TextMCoord tc, int lengthBytes);

  // Remove characters from a line, shifting later boundaries to the
  // left, and boundaries within the deleted region to `tc`.
  void deleteLineBytes(TextMCoord tc, int lengthBytes);


  // ---- Query the mapping ----
  // True if `numEntries()` is zero.
  bool empty() const;

  // Total number of entries, i.e., the number of insertions that have
  // been performed since the last `clear()`.
  int numEntries() const;

  // If there are no entries, this returns -1.  Otherwise, it is the
  // largest line number for which there is any intersecting entry.
  int maxEntryLine() const;

  // The number of lines that potentially have associated entry data.
  // Numerically, this is `maxEntryLine() + 1`.
  LineCount numLinesWithData() const;

  // Number of lines in the file, if known.
  std::optional<PositiveLineCount> getNumLinesOpt() const;

  // True if we can track document updates, which requires that we know
  // the number of lines.  Setting the number of lines is normally done
  // for the first time in `adjustForDocument` or
  // `setNumLinesAndAdjustAccordingly`.
  //
  // TODO: Is it either?  Can I make it consistently one of them?
  bool canTrackUpdates() const;

  // Return `getNumLinesOpt().value()`.
  //
  // Requires: canTrackUpdates()
  PositiveLineCount getNumLines() const;

  // True if `tc` is valid for the current number of lines.
  // Specifically, `0 <= tc.m_line` and, if `m_numLines` has a value,
  // then `tc.m_line < getNumLines()`.
  bool validCoord(TextMCoord tc) const;

  // True if both endpoints are valid and `range.isRectified()`.
  bool validRange(TextMCoordRange const &range) const;

  // Get all the entries that intersect `line`.  This will include
  // partial entries for multi-line spans, which describe only the
  // portion of the original `DocEntry` that intersects the specified
  // line.
  std::set<LineEntry> getLineEntries(LineIndex line) const;

  // Get all current entries for this document, each as a complete
  // (possibly multi-line) `DocEntry`.  This is the set of entries that
  // were originally inserted, except with the coordinates possibly
  // changed due to subsequent text modification.  (So if there have
  // been no text changes, this will return exactly the set of entries
  // that have been passed to `insert`.)
  std::set<DocEntry> getAllEntries() const;

  // Get the set of values that are mapped.
  std::set<Value> getMappedValues() const;

  // Return a tagged (`TextMCoordMap`) ordered map with `numLines` and
  // `entries`, the latter containing `toGDValue(getAllEntries())`.
  operator gdv::GDValue() const;

  // Internal data as GDValue, for debug/test purposes.
  gdv::GDValue dumpInternals() const;
};


#endif // EDITOR_TEXTMCOORD_MAP_H
