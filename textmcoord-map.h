// textmcoord-map.h
// `TextMCoordMap` class.

// See license.txt for copyright and terms of use.

#ifndef EDITOR_TEXTMCOORD_MAP_H
#define EDITOR_TEXTMCOORD_MAP_H

#include "gap.h"                       // GapArray
#include "textmcoord.h"                // TextMCoord

#include "smbase/compare-util-iface.h" // DEFINE_FRIEND_RELATIONAL_OPERATORS
#include "smbase/gdvalue-fwd.h"        // gdv::GDValue
#include "smbase/sm-macros.h"          // NO_OBJECT_COPIES
#include "smbase/std-vector-fwd.h"     // stdfwd::vector

#include <set>                         // std::set


// Candidate to move someplace more general.
#define DECLARE_COMPARETO_AND_DEFINE_RELATIONALS(Class) \
  int compareTo(Class const &b) const;                  \
  friend int compare(Class const &a, Class const &b)    \
    { return a.compareTo(b); }                          \
  DEFINE_FRIEND_RELATIONAL_OPERATORS(Class)


// Associative map from a range of some text document to some value.
class TextMCoordMap {
  // Not impossible, just not implemented, and automatic would be wrong.
  NO_OBJECT_COPIES(TextMCoordMap);

public:      // types
  // For the moment, just say values are integers.
  typedef int Value;

  // Element stored in the map.
  //
  // This class is used as part of the interface to the map, but the
  // data is not stored internally this way for efficiency reasons.
  class Entry {
  public:      // data
    // Range of text associated with the value.  This range is
    // normalized in the sense that its start is less than or equal to
    // its end.
    TextMCoordRange m_range;

    // The associated value.
    Value m_value;

  public:      // methods
    Entry(TextMCoordRange range, Value value);

    void selfCheck() const;

    operator gdv::GDValue() const;
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
  // or span a particular line.
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
    // Deletion as it affects `m_singleLineSpans`.
    void deleteBytes_spans(int delStart, int lengthBytes);

    // Deletion as it affects one of the boundary sets.
    static void deleteBytes_boundaries(
      std::set<Boundary> &boundaries,
      int delStart,
      int lengthBytes);

  public:      // methods
    ~LineData();
    LineData();

    void selfCheck() const;

    operator gdv::GDValue() const;

    // Modify the associated spans to reflect inserting `lengthBytes`
    // bytes starting at `insStart`.
    void insertBytes(int insStart, int lengthBytes);

    // Modify the associated spans to reflect deleting `lengthBytes`
    // bytes starting at `delStart`.
    void deleteBytes(int delStart, int lengthBytes);
  };

private:     // data
  // Set of values that are part of some range.
  //
  // Invariant: This set is the same as that mentioned across all of
  // `m_lineData`.
  std::set<Value> m_values;

  // Map from 0-based line number to associated data.
  GapArray<LineData * NULLABLE /*owner*/> m_lineData;

private:     // methods
  // Get the data for `line`, creating it if necessary.
  LineData *getOrCreateLineData(int line);

  // Get the data, or nullptr if there is no data for that line.  This
  // allows `line` to be out of range, including negative.
  LineData const * NULLABLE getLineDataC(int line) const;

  // Same, but non-const.
  LineData * NULLABLE getLineData(int line);

public:      // methods
  ~TextMCoordMap();

  // Make an empty map.
  TextMCoordMap();

  // Assert all invariants.
  void selfCheck() const;


  // ---- Manipulate the mapping directly ----
  // Add an entry.  Requires that its value not already be in the map.
  void insert(Entry entry);

  // There is not currently a way to remove individual entries because I
  // don't anticipate needing to do so.

  // Remove all entries.
  void clear();


  // ---- Manipulate the mapping indirectly via text insert/delete ----
  // Insert `count` lines starting at `line`, shifting all range
  // boundaries that come after the line down.
  void insertLines(int line, int count);

  // Delete `count` lines, starting at `line` and going down, shifting
  // all later boundaries up.  A boundary that is on the line will be
  // shifted to the start of the line.  Thus, any range that started and
  // ended on the line will, afterward, have an associated zero-length
  // range at the start of the line; but it will still be in the map.
  void deleteLines(int line, int count);

  // Insert characters on a single line, at `tc`, shifting later
  // boundaries to the right.
  void insertLineBytes(TextMCoord tc, int lengthBytes);

  // Remove characters from a line, shifting later boundaries to the
  // left, and boundaries within the deleted region to `tc`.
  void deleteLineBytes(TextMCoord tc, int lengthBytes);


  // ---- Query the mapping ----
  // Get all the entries that intersect `line`.
  stdfwd::vector<Entry> getEntriesForLine(int line) const;

  // Get all current entries.
  stdfwd::vector<Entry> getAllEntries() const;

  // Dump the data.
  operator gdv::GDValue() const;
};


#endif // EDITOR_TEXTMCOORD_MAP_H
