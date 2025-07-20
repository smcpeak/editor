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


// Dump the internals as an intended GDVN string for comparison with
// expected values below.
std::string internals(TextMCoordMap const &m)
{
  GDValueWriteOptions opts;

  // This level of indentation meshes properly with the code context
  // where the expected output appears.
  opts.m_indentLevel = 2;

  return m.dumpInternals().asIndentedString(opts);
}


// Get the entries for `line`, using 99 as "end of line".
std::string lineEntries(TextMCoordMap const &m, int line)
{
  return toGDValue(m.getEntriesForLine(line, 99)).asString();
}


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


// This test follows the example in the comments above the declaration
// of the `TextMCoordMap` class in the header file.
void test_commentsExample()
{
  EXN_CONTEXT("commentsExample");

  DIAG("Start with empty map.");
  TextMCoordMap m;
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

  // Perform all operations in parallel on the reference map.
  ReferenceMap r;
  checkSame(r, m);

  DIAG("Insert value 1 at 1:5 to 1:12.");
  m.insert({{{1,5}, {1,12}}, 1});
  r.insert({{{1,5}, {1,12}}, 1});
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
  checkSame(r, m);

  DIAG("Insert value 2 at 3:5 to 5:12.");
  m.insert({{{3,5}, {5,12}}, 2});
  r.insert({{{3,5}, {5,12}}, 2});
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
  checkSame(r, m);

  DIAG("Insert line at 3.");
  m.insertLines(3, 1);
  r.insertLines(3, 1);
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
  checkSame(r, m);

  DIAG("Delete line 5.");
  m.deleteLines(5, 1);
  r.deleteLines(5, 1);
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
  checkSame(r, m);
}


void entry(int argc, char **argv)
{
  test_commentsExample();
}


CLOSE_ANONYMOUS_NAMESPACE


ARGS_TEST_MAIN


// EOF
