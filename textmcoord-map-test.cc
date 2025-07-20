// textmcoord-map-test.cc
// Tests for `textmcoord-map` module.

#include "smbase/gdvalue-set-fwd.h"    // gdv::toGDValue(std::set)

#include "textmcoord-map.h"            // module under test

#include "smbase/gdvalue-set.h"        // gdv::toGDValue(std::set)
#include "smbase/gdvalue.h"            // gdv::toGDValue
#include "smbase/sm-macros.h"          // OPEN_ANONYMOUS_NAMESPACE, NO_OBJECT_COPIES
#include "smbase/sm-test.h"            // ARGS_MAIN

#include <algorithm>                   // std::max
#include <iostream>                    // std::cout
#include <set>                         // std::set


using namespace gdv;


OPEN_ANONYMOUS_NAMESPACE


// Simple but inefficient implementation of `TextMCoordMap`.
//
// This works by rewriting the entire set on every edit, moving each
// endpoint separately.
//
class ReferenceMap {
  // For uniformity with the real thing.
  NO_OBJECT_COPIES(ReferenceMap);

public:      // types
  typedef TextMCoordMap::Entry Entry;

public:      // data
  // All entries.
  std::set<Entry> m_entries;

public:
  ~ReferenceMap() = default;
  ReferenceMap() = default;

  void insert(Entry entry)
  {
    m_entries.insert(entry);
  }

  void clear()
  {
    m_entries.clear();
  }

  static TextMCoord adjustMC_insertLines(TextMCoord mc, int line, int count)
  {
    // Push at or later lines down by `count`.
    if (mc.m_line >= line) {
      mc.m_line += count;
    }
    return mc;
  }

  // The logic for inserting bytes exactly parallels the logic for
  // inserting lines.
  static TextMCoord adjustMC_insertLineBytes(TextMCoord mc, TextMCoord tc, int lengthBytes)
  {
    if (mc.m_line == tc.m_line) {
      // Push at or later bytes right by `lengthBytes`.
      if (mc.m_byteIndex >= tc.m_byteIndex) {
        mc.m_byteIndex += lengthBytes;
      }
    }

    return mc;
  }

  static TextMCoord adjustMC_deleteLines(TextMCoord mc, int line, int count)
  {
    // Pull later lines up, but not above `line`.
    if (mc.m_line > line) {
      mc.m_line = std::max(line, mc.m_line - count);
    }
    return mc;
  }

  static TextMCoord adjustMC_deleteLineBytes(TextMCoord mc, TextMCoord tc, int lengthBytes)
  {
    if (mc.m_line == tc.m_line) {
      // Pull later bytes left, but not in front of `tc.m_byteIndex`.
      if (mc.m_byteIndex > tc.m_byteIndex) {
        mc.m_byteIndex = std::max(tc.m_byteIndex, mc.m_byteIndex - lengthBytes);
      }
    }

    return mc;
  }

  void insertLines(int line, int count)
  {
    std::set<Entry> newEntries;

    for (Entry const &e : m_entries) {
      newEntries.insert(Entry(
        TextMCoordRange(
          adjustMC_insertLines(e.m_range.m_start, line, count),
          adjustMC_insertLines(e.m_range.m_end, line, count)
        ),
        e.m_value));
    }

    m_entries.swap(newEntries);
  }

  void deleteLines(int line, int count)
  {
    std::set<Entry> newEntries;

    for (Entry const &e : m_entries) {
      newEntries.insert(Entry(
        TextMCoordRange(
          adjustMC_deleteLines(e.m_range.m_start, line, count),
          adjustMC_deleteLines(e.m_range.m_end, line, count)
        ),
        e.m_value));
    }

    m_entries.swap(newEntries);
  }

  void insertLineBytes(TextMCoord tc, int lengthBytes)
  {
    std::set<Entry> newEntries;

    for (Entry const &e : m_entries) {
      newEntries.insert(Entry(
        TextMCoordRange(
          adjustMC_insertLineBytes(e.m_range.m_start, tc, lengthBytes),
          adjustMC_insertLineBytes(e.m_range.m_end, tc, lengthBytes)
        ),
        e.m_value));
    }

    m_entries.swap(newEntries);
  }

  void deleteLineBytes(TextMCoord tc, int lengthBytes)
  {
    std::set<Entry> newEntries;

    for (Entry const &e : m_entries) {
      newEntries.insert(Entry(
        TextMCoordRange(
          adjustMC_deleteLineBytes(e.m_range.m_start, tc, lengthBytes),
          adjustMC_deleteLineBytes(e.m_range.m_end, tc, lengthBytes)
        ),
        e.m_value));
    }

    m_entries.swap(newEntries);
  }

  bool empty() const
  {
    return m_entries.empty();
  }

  int numEntries() const
  {
    return m_entries.size();
  }

  int numLines() const
  {
    int ret = 0;

    for (Entry const &e : m_entries) {
      ret = std::max(ret, e.m_range.m_end.m_line+1);
    }

    return ret;
  }

  std::set<Entry> getEntriesForLine(int line, int lineEndByteIndex) const
  {
    std::set<Entry> ret;

    for (Entry const &e : m_entries) {
      if (e.m_range.m_start.m_line == e.m_range.m_end.m_line) {
        // Entirely on one line.
        if (e.m_range.m_start.m_line == line) {
          // Entirely on *this* line.
          ret.insert(e);
        }
      }

      else if (e.m_range.m_start.m_line == line) {
        // Begins here, goes to EOL.
        ret.insert(Entry(
          TextMCoordRange(
            e.m_range.m_start,
            TextMCoord(line, lineEndByteIndex)
          ),
          e.m_value));
      }

      else if (e.m_range.m_end.m_line == line) {
        // Ends here, starts at line start.
        ret.insert(Entry(
          TextMCoordRange(
            TextMCoord(line, 0),
            e.m_range.m_end
          ),
          e.m_value));
      }

      else if (e.m_range.m_start.m_line < line &&
                                          line < e.m_range.m_end.m_line) {
        // Continues here.
        ret.insert(Entry(
          TextMCoordRange(
            TextMCoord(line, 0),
            TextMCoord(line, lineEndByteIndex)
          ),
          e.m_value));
      }
    }

    return ret;
  }

  std::set<Entry> getAllEntries() const
  {
    return m_entries;
  }

  operator gdv::GDValue() const
  {
    return toGDValue(m_entries);
  }
};


// Check that `r` and `m` agree in all respects.
void checkSame(ReferenceMap const &r, TextMCoordMap const &m)
{
  EXPECT_EQ(r.empty(), m.empty());
  EXPECT_EQ(r.numEntries(), m.numEntries());
  EXPECT_EQ(r.numLines(), m.numLines());

  // Compare as GDValue first so we get a printout on mismatch.
  EXPECT_EQ(toGDValue(r), toGDValue(m));

  // But then also compare without the conversion.  I don't know why
  // this would ever fail if the above succeeded, but it won't hurt.
  xassert(r.getAllEntries() == m.getAllEntries());

  for (int i=0; i < r.numLines(); ++i) {
    EXN_CONTEXT_EXPR(i);

    EXPECT_EQ(toGDValue(r.getEntriesForLine(i, 99)),
              toGDValue(m.getEntriesForLine(i, 99)));
    xassert(r.getEntriesForLine(i, 99) ==
            m.getEntriesForLine(i, 99));
  }
}


// Combination of `TextMCoordMap` and `ReferenceMap`.
class MapPair {
public:      // types
  typedef TextMCoordMap::Entry Entry;

public:      // data
  // System under test.
  TextMCoordMap m_sut;

  // Reference implementation.
  ReferenceMap m_ref;

public:      // methods
  ~MapPair() = default;
  MapPair() = default;

  void selfCheck()
  {
    m_sut.selfCheck();
    checkSame(m_ref, m_sut);
  }

  // Mutations: Perform on each in parallel.

  void insert(Entry entry)
  {
    m_sut.insert(entry);
    m_ref.insert(entry);
  }

  void clear()
  {
    m_sut.clear();
    m_ref.clear();
  }

  void insertLines(int line, int count)
  {
    m_sut.insertLines(line, count);
    m_ref.insertLines(line, count);
  }

  void deleteLines(int line, int count)
  {
    m_sut.deleteLines(line, count);
    m_ref.deleteLines(line, count);
  }

  void insertLineBytes(TextMCoord tc, int lengthBytes)
  {
    m_sut.insertLineBytes(tc, lengthBytes);
    m_ref.insertLineBytes(tc, lengthBytes);
  }

  void deleteLineBytes(TextMCoord tc, int lengthBytes)
  {
    m_sut.deleteLineBytes(tc, lengthBytes);
    m_ref.deleteLineBytes(tc, lengthBytes);
  }

  // Queries: Pass through to system under test.

  bool empty() const
  {
    return m_sut.empty();
  }

  int numEntries() const
  {
    return m_sut.numEntries();
  }

  int numLines() const
  {
    return m_sut.numLines();
  }

  std::set<Entry> getEntriesForLine(int line, int lineEndByteIndex) const
  {
    return m_sut.getEntriesForLine(line, lineEndByteIndex);
  }

  std::set<Entry> getAllEntries() const
  {
    return m_sut.getAllEntries();
  }

  operator gdv::GDValue() const
  {
    return m_sut.operator GDValue();
  }

  gdv::GDValue dumpInternals() const
  {
    return m_sut.dumpInternals();
  }
};


// Dump the internals as an intended GDVN string for comparison with
// expected values below.
std::string internals(MapPair const &m)
{
  GDValueWriteOptions opts;

  // This level of indentation meshes properly with the code context
  // where the expected output appears.
  opts.m_indentLevel = 2;

  return m.dumpInternals().asIndentedString(opts);
}


// Get the entries for `line`, using 99 as "end of line".
std::string lineEntries(MapPair const &m, int line)
{
  return toGDValue(m.getEntriesForLine(line, 99)).asString();
}


// This test follows the example in the comments above the declaration
// of the `TextMCoordMap` class in the header file.  Note: That example
// only does edits at the line granularity.
void test_commentsExample()
{
  EXN_CONTEXT("commentsExample");

  DIAG("Start with empty map.");
  MapPair m;
  m.selfCheck();
  EXPECT_EQ(m.empty(), true);
  EXPECT_EQ(m.numEntries(), 0);
  EXPECT_EQ(m.numLines(), 0);
  EXPECT_EQ(stringb(toGDValue(m)),
    "{}");
  EXPECT_EQ(stringb(m.dumpInternals()),
    "TextMCoordMapInternals[values:{} lineData:{length:0}]");

  // Asking about out-of-range lines is allowed, and yields an empty set.
  EXPECT_EQ(lineEntries(m, -1), "{}");
  EXPECT_EQ(lineEntries(m, 0), "{}");

  DIAG("Insert value 1 at 1:5 to 1:12.");
  m.insert({{{1,5}, {1,12}}, 1});
  m.selfCheck();
  EXPECT_EQ(m.empty(), false);
  EXPECT_EQ(m.numEntries(), 1);
  EXPECT_EQ(m.numLines(), 2);
  EXPECT_EQ(stringb(toGDValue(m)),
    "{Entry[range:MCR(MC(1 5) MC(1 12)) value:1]}");
  EXPECT_EQ(lineEntries(m, -1), "{}");
  EXPECT_EQ(lineEntries(m, 0),
    "{}");
  EXPECT_EQ(lineEntries(m, 1),
    "{Entry[range:MCR(MC(1 5) MC(1 12)) value:1]}");
  EXPECT_EQ(lineEntries(m, 2), "{}");
  EXPECT_EQ(internals(m),
    R"(TextMCoordMapInternals[
      values: {1}
      lineData: {
        length: 2
        1: LineData[
          singleLineSpans:
            {SingleLineSpan[startByteIndex:5 endByteIndex:12 value:1]}
          startsHere: {}
          continuesHere: {}
          endsHere: {}
        ]
      }
    ])");

  DIAG("Insert value 2 at 3:5 to 5:12.");
  m.insert({{{3,5}, {5,12}}, 2});
  m.selfCheck();
  EXPECT_EQ(m.empty(), false);
  EXPECT_EQ(m.numEntries(), 2);
  EXPECT_EQ(m.numLines(), 6);
  EXPECT_EQ(stringb(toGDValue(m)),
    "{Entry[range:MCR(MC(1 5) MC(1 12)) value:1] "
     "Entry[range:MCR(MC(3 5) MC(5 12)) value:2]}");
  EXPECT_EQ(lineEntries(m, 0),
    "{}");
  EXPECT_EQ(lineEntries(m, 1),
    "{Entry[range:MCR(MC(1 5) MC(1 12)) value:1]}");
  EXPECT_EQ(lineEntries(m, 2),
    "{}");
  EXPECT_EQ(lineEntries(m, 3),
    "{Entry[range:MCR(MC(3 5) MC(3 99)) value:2]}");
  EXPECT_EQ(lineEntries(m, 4),
    "{Entry[range:MCR(MC(4 0) MC(4 99)) value:2]}");
  EXPECT_EQ(lineEntries(m, 5),
    "{Entry[range:MCR(MC(5 0) MC(5 12)) value:2]}");
  EXPECT_EQ(internals(m),
    R"(TextMCoordMapInternals[
      values: {1 2}
      lineData: {
        length: 6
        1: LineData[
          singleLineSpans:
            {SingleLineSpan[startByteIndex:5 endByteIndex:12 value:1]}
          startsHere: {}
          continuesHere: {}
          endsHere: {}
        ]
        3: LineData[
          singleLineSpans: {}
          startsHere: {Boundary[byteIndex:5 value:2]}
          continuesHere: {}
          endsHere: {}
        ]
        4: LineData[
          singleLineSpans: {}
          startsHere: {}
          continuesHere: {2}
          endsHere: {}
        ]
        5: LineData[
          singleLineSpans: {}
          startsHere: {}
          continuesHere: {}
          endsHere: {Boundary[byteIndex:12 value:2]}
        ]
      }
    ])");

  DIAG("Insert line at 3.");
  m.insertLines(3, 1);
  m.selfCheck();
  EXPECT_EQ(m.empty(), false);
  EXPECT_EQ(m.numEntries(), 2);
  EXPECT_EQ(m.numLines(), 7);
  EXPECT_EQ(stringb(toGDValue(m)),
    "{Entry[range:MCR(MC(1 5) MC(1 12)) value:1] "
     "Entry[range:MCR(MC(4 5) MC(6 12)) value:2]}");
  EXPECT_EQ(lineEntries(m, 0),
    "{}");
  EXPECT_EQ(lineEntries(m, 1),
    "{Entry[range:MCR(MC(1 5) MC(1 12)) value:1]}");
  EXPECT_EQ(lineEntries(m, 2),
    "{}");
  EXPECT_EQ(lineEntries(m, 3),
    "{}");
  EXPECT_EQ(lineEntries(m, 4),
    "{Entry[range:MCR(MC(4 5) MC(4 99)) value:2]}");
  EXPECT_EQ(lineEntries(m, 5),
    "{Entry[range:MCR(MC(5 0) MC(5 99)) value:2]}");
  EXPECT_EQ(lineEntries(m, 6),
    "{Entry[range:MCR(MC(6 0) MC(6 12)) value:2]}");
  EXPECT_EQ(internals(m),
    R"(TextMCoordMapInternals[
      values: {1 2}
      lineData: {
        length: 7
        1: LineData[
          singleLineSpans:
            {SingleLineSpan[startByteIndex:5 endByteIndex:12 value:1]}
          startsHere: {}
          continuesHere: {}
          endsHere: {}
        ]
        4: LineData[
          singleLineSpans: {}
          startsHere: {Boundary[byteIndex:5 value:2]}
          continuesHere: {}
          endsHere: {}
        ]
        5: LineData[
          singleLineSpans: {}
          startsHere: {}
          continuesHere: {2}
          endsHere: {}
        ]
        6: LineData[
          singleLineSpans: {}
          startsHere: {}
          continuesHere: {}
          endsHere: {Boundary[byteIndex:12 value:2]}
        ]
      }
    ])");

  DIAG("Delete line 5.");
  m.deleteLines(5, 1);
  m.selfCheck();
  EXPECT_EQ(m.empty(), false);
  EXPECT_EQ(m.numEntries(), 2);
  EXPECT_EQ(m.numLines(), 6);
  EXPECT_EQ(stringb(toGDValue(m)),
    "{Entry[range:MCR(MC(1 5) MC(1 12)) value:1] "
     "Entry[range:MCR(MC(4 5) MC(5 12)) value:2]}");
  EXPECT_EQ(lineEntries(m, 0),
    "{}");
  EXPECT_EQ(lineEntries(m, 1),
    "{Entry[range:MCR(MC(1 5) MC(1 12)) value:1]}");
  EXPECT_EQ(lineEntries(m, 2),
    "{}");
  EXPECT_EQ(lineEntries(m, 3),
    "{}");
  EXPECT_EQ(lineEntries(m, 4),
    "{Entry[range:MCR(MC(4 5) MC(4 99)) value:2]}");
  EXPECT_EQ(lineEntries(m, 5),
    "{Entry[range:MCR(MC(5 0) MC(5 12)) value:2]}");
  EXPECT_EQ(internals(m),
    R"(TextMCoordMapInternals[
      values: {1 2}
      lineData: {
        length: 6
        1: LineData[
          singleLineSpans:
            {SingleLineSpan[startByteIndex:5 endByteIndex:12 value:1]}
          startsHere: {}
          continuesHere: {}
          endsHere: {}
        ]
        4: LineData[
          singleLineSpans: {}
          startsHere: {Boundary[byteIndex:5 value:2]}
          continuesHere: {}
          endsHere: {}
        ]
        5: LineData[
          singleLineSpans: {}
          startsHere: {}
          continuesHere: {}
          endsHere: {Boundary[byteIndex:12 value:2]}
        ]
      }
    ])");
}


// Do some deletions within a single line.
void test_lineDeletions()
{
  EXN_CONTEXT("lineDeletions");

  MapPair m;
  m.selfCheck();

  DIAG("Make a span.");
  m.insert({{{0,10}, {0,20}}, 1});
  m.selfCheck();

  EXPECT_EQ(stringb(toGDValue(m)),
    "{Entry[range:MCR(MC(0 10) MC(0 20)) value:1]}");
  // Initial state:
  //             1         2         3
  //   0123456789012345678901234567890
  //             [         )
  //                 ^
  //                del

  DIAG("Delete one byte in the middle.");
  m.deleteLineBytes({0,14}, 1);
  m.selfCheck();

  EXPECT_EQ(stringb(toGDValue(m)),
    "{Entry[range:MCR(MC(0 10) MC(0 19)) value:1]}");
  //             1         2         3
  //   0123456789012345678901234567890
  //             [        )
  //             ^
  //            del

  DIAG("Delete one byte just inside the left edge.");
  m.deleteLineBytes({0,10}, 1);
  m.selfCheck();

  EXPECT_EQ(stringb(toGDValue(m)),
    "{Entry[range:MCR(MC(0 10) MC(0 18)) value:1]}");
  //             1         2         3
  //   0123456789012345678901234567890
  //             [       )
  //            ^
  //           del

  DIAG("Delete one byte just outside the left edge.");
  m.deleteLineBytes({0,9}, 1);
  m.selfCheck();

  EXPECT_EQ(stringb(toGDValue(m)),
    "{Entry[range:MCR(MC(0 9) MC(0 17)) value:1]}");
  //             1         2         3
  //   0123456789012345678901234567890
  //            [       )
  //                   ^
  //                  del

  DIAG("Delete one byte just inside the right edge.");
  m.deleteLineBytes({0,16}, 1);
  m.selfCheck();

  EXPECT_EQ(stringb(toGDValue(m)),
    "{Entry[range:MCR(MC(0 9) MC(0 16)) value:1]}");
  //             1         2         3
  //   0123456789012345678901234567890
  //            [      )
  //                   ^
  //                  del

  DIAG("Delete one byte just outside the right edge (no effect).");
  m.deleteLineBytes({0,16}, 1);
  m.selfCheck();

  EXPECT_EQ(stringb(toGDValue(m)),
    "{Entry[range:MCR(MC(0 9) MC(0 16)) value:1]}");
  //             1         2         3
  //   0123456789012345678901234567890
  //            [      )
  //           ^^
  //          del

  DIAG("Delete two bytes straddling the left edge.");
  m.deleteLineBytes({0,8}, 2);
  m.selfCheck();

  EXPECT_EQ(stringb(toGDValue(m)),
    "{Entry[range:MCR(MC(0 8) MC(0 14)) value:1]}");
  //             1         2         3
  //   0123456789012345678901234567890
  //           [     )
  //                ^^
  //               del

  DIAG("Delete two bytes straddling the right edge.");
  m.deleteLineBytes({0,13}, 2);
  m.selfCheck();

  EXPECT_EQ(stringb(toGDValue(m)),
    "{Entry[range:MCR(MC(0 8) MC(0 13)) value:1]}");
  //             1         2         3
  //   0123456789012345678901234567890
  //           [    )
  //           ^^^^^
  //            del

  DIAG("Delete the exact range.");
  m.deleteLineBytes({0,8}, 5);
  m.selfCheck();

  EXPECT_EQ(stringb(toGDValue(m)),
    "{Entry[range:MCR(MC(0 8) MC(0 8)) value:1]}");
  //             1         2         3
  //   0123456789012345678901234567890
  //           H
  //          ^^
  //          del

  DIAG("Delete two bytes straddling the empty range.");
  m.deleteLineBytes({0,7}, 2);
  m.selfCheck();

  EXPECT_EQ(stringb(toGDValue(m)),
    "{Entry[range:MCR(MC(0 7) MC(0 7)) value:1]}");
  //             1         2         3
  //   0123456789012345678901234567890
  //          H
  //   ^^^^^^^
  //     del

  DIAG("Delete all preceding bytes.");
  m.deleteLineBytes({0,0}, 7);
  m.selfCheck();

  EXPECT_EQ(stringb(toGDValue(m)),
    "{Entry[range:MCR(MC(0 0) MC(0 0)) value:1]}");
  //             1         2         3
  //   0123456789012345678901234567890
  //   H
}


void entry(int argc, char **argv)
{
  test_commentsExample();
  test_lineDeletions();
}


CLOSE_ANONYMOUS_NAMESPACE


ARGS_TEST_MAIN


// EOF
