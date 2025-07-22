// textmcoord-map-test.cc
// Tests for `textmcoord-map` module.

#include "smbase/gdvalue-set-fwd.h"    // gdv::toGDValue(std::set)

#include "textmcoord-map.h"            // module under test

#include "smbase/gdvalue-parser.h"     // gdv::GDValueParser
#include "smbase/gdvalue-set.h"        // gdv::toGDValue(std::set)
#include "smbase/gdvalue.h"            // gdv::toGDValue
#include "smbase/sm-env.h"             // smbase::envAsIntOr
#include "smbase/sm-macros.h"          // OPEN_ANONYMOUS_NAMESPACE, NO_OBJECT_COPIES
#include "smbase/sm-test.h"            // ARGS_MAIN
#include "smbase/string-util.h"        // join, suffixAll

#include <algorithm>                   // std::max
#include <cstdlib>                     // std::rand
#include <iostream>                    // std::cout
#include <set>                         // std::set


using namespace gdv;
using namespace smbase;


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
  typedef TextMCoordMap::Value Value;
  typedef TextMCoordMap::DocEntry DocEntry;
  typedef TextMCoordMap::LineEntry LineEntry;

public:      // data
  // All entries.
  std::set<DocEntry> m_entries;

public:
  ~ReferenceMap() = default;
  ReferenceMap() = default;

  void insert(DocEntry entry)
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
    if (line <= mc.m_line &&
                mc.m_line < (line+count)) {
      // The endpoint is in the deleted region, so its column gets
      // zeroed.
      mc.m_byteIndex = 0;
    }

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
    std::set<DocEntry> newEntries;

    for (DocEntry const &e : m_entries) {
      newEntries.insert(DocEntry(
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
    std::set<DocEntry> newEntries;

    for (DocEntry const &e : m_entries) {
      newEntries.insert(DocEntry(
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
    std::set<DocEntry> newEntries;

    for (DocEntry const &e : m_entries) {
      newEntries.insert(DocEntry(
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
    std::set<DocEntry> newEntries;

    for (DocEntry const &e : m_entries) {
      newEntries.insert(DocEntry(
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

    for (DocEntry const &e : m_entries) {
      ret = std::max(ret, e.m_range.m_end.m_line+1);
    }

    return ret;
  }

  std::set<LineEntry> getLineEntries(int line) const
  {
    std::set<LineEntry> ret;

    for (DocEntry const &e : m_entries) {
      if (e.m_range.m_start.m_line == e.m_range.m_end.m_line) {
        // Entirely on one line.
        if (e.m_range.m_start.m_line == line) {
          // Entirely on *this* line.
          ret.insert(LineEntry(
            e.m_range.m_start.m_byteIndex,
            e.m_range.m_end.m_byteIndex,
            e.m_value
          ));
        }
      }

      else if (e.m_range.m_start.m_line == line) {
        // Begins here, goes to EOL.
        ret.insert(LineEntry(
          e.m_range.m_start.m_byteIndex,
          std::nullopt,
          e.m_value
        ));
      }

      else if (e.m_range.m_end.m_line == line) {
        // Ends here, starts at line start.
        ret.insert(LineEntry(
          std::nullopt,
          e.m_range.m_end.m_byteIndex,
          e.m_value
        ));
      }

      else if (e.m_range.m_start.m_line < line &&
                                          line < e.m_range.m_end.m_line) {
        // Continues here.
        ret.insert(LineEntry(
          std::nullopt,
          std::nullopt,
          e.m_value
        ));
      }
    }

    return ret;
  }

  std::set<DocEntry> getAllEntries() const
  {
    return m_entries;
  }

  std::set<Value> getMappedValues() const
  {
    std::set<Value> ret;

    for (DocEntry const &e : m_entries) {
      ret.insert(e.m_value);
    }

    return ret;
  }

  operator gdv::GDValue() const
  {
    return toGDValue(m_entries);
  }
};


// Check that `m`, regarded as the "actual" value, and `r`, regarded as
// the "expected" value, agree in all respects.
void checkSame(TextMCoordMap const &m, ReferenceMap const &r)
{
  EXN_CONTEXT("checkSame");

  EXPECT_EQ(m.empty(), r.empty());
  EXPECT_EQ(m.numEntries(), r.numEntries());
  EXPECT_EQ(m.numLines(), r.numLines());

  // Compare as GDValue first so we get a printout on mismatch.
  EXPECT_EQ(toGDValue(m), toGDValue(r));

  // But then also compare without the conversion.  I don't know why
  // this would ever fail if the above succeeded, but it won't hurt.
  xassert(m.getAllEntries() == r.getAllEntries());

  for (int i=0; i < r.numLines(); ++i) {
    EXN_CONTEXT_EXPR(i);

    EXPECT_EQ(toGDValue(m.getLineEntries(i)),
              toGDValue(r.getLineEntries(i)));
    xassert(m.getLineEntries(i) ==
            r.getLineEntries(i));
  }

  xassert(m.getMappedValues() == r.getMappedValues());
}


// Return a string that will evaluate to `tc` in a C++ context where a
// `TextMCoord` is expected.
std::string toCode(TextMCoord tc)
{
  return stringb("{" << tc.m_line << "," << tc.m_byteIndex << "}");
}

std::string toCode(TextMCoordRange tcr)
{
  return stringb("{" << toCode(tcr.m_start) << ", " << toCode(tcr.m_end) << "}");
}

std::string toCode(TextMCoordMap::DocEntry const &e)
{
  return stringb("{" << toCode(e.m_range) << ", " << e.m_value << "}");
}


// Combination of `TextMCoordMap` and `ReferenceMap`.
class MapPair {
public:      // types
  typedef TextMCoordMap::Value Value;
  typedef TextMCoordMap::DocEntry DocEntry;
  typedef TextMCoordMap::LineEntry LineEntry;

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
    checkSame(m_sut, m_ref);
  }

  // Mutations: Perform on each in parallel.

  void insert(DocEntry entry)
  {
    // Print these operations as C++ code that I can copy into my
    // tests to recreate a scenario that was generated randomly.
    DIAG("m.insert(" << toCode(entry) << ");");

    m_sut.insert(entry);
    m_ref.insert(entry);
  }

  void clear()
  {
    DIAG("m.clear();");
    m_sut.clear();
    m_ref.clear();
  }

  void insertLines(int line, int count)
  {
    DIAG("m.insertLines(" << line << ", " << count << ");");
    m_sut.insertLines(line, count);
    m_ref.insertLines(line, count);
  }

  void deleteLines(int line, int count)
  {
    DIAG("m.deleteLines(" << line << ", " << count << ");");
    m_sut.deleteLines(line, count);
    m_ref.deleteLines(line, count);
  }

  void insertLineBytes(TextMCoord tc, int lengthBytes)
  {
    DIAG("m.insertLineBytes(" << toCode(tc) << ", " << lengthBytes << ");");
    m_sut.insertLineBytes(tc, lengthBytes);
    m_ref.insertLineBytes(tc, lengthBytes);
  }

  void deleteLineBytes(TextMCoord tc, int lengthBytes)
  {
    DIAG("m.deleteLineBytes(" << toCode(tc) << ", " << lengthBytes << ");");
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

  std::set<LineEntry> getLineEntries(int line) const
  {
    return m_sut.getLineEntries(line);
  }

  std::set<DocEntry> getAllEntries() const
  {
    return m_sut.getAllEntries();
  }

  std::set<Value> getMappedValues() const
  {
    return m_sut.getMappedValues();
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


// Get the entries for `line` as a string.
std::string lineEntriesString(MapPair const &m, int line)
{
  return toGDValue(m.getLineEntries(line)).asString();
}


// Get all line entries, each terminated by a newline.
std::string allLineEntries(MapPair const &m)
{
  std::vector<std::string> results;

  for (int i=0; i < m.numLines(); ++i) {
    results.push_back(lineEntriesString(m, i));
  }

  return join(suffixAll(results, "\n"), "");
}


// Check that every `LineEntry` in `m` can de/serialize to itself.
void checkLineEntriesRoundtrip(MapPair const &m)
{
  typedef MapPair::LineEntry LineEntry;

  for (int i=0; i < m.numLines(); ++i) {
    std::set<LineEntry> lineEntries = m.getLineEntries(i);
    GDValue v(toGDValue(lineEntries));
    std::set<LineEntry> after =
      gdvpTo<std::set<LineEntry>>(GDValueParser(v));

    EXPECT_EQ(toGDValue(after), v);
    xassert(after == lineEntries);
  }
}


// This test follows the example in the comments above the declaration
// of the `TextMCoordMap` class in the header file.  Note: That example
// only does edits at the line granularity.
void test_commentsExample()
{
  DIAG("Start with empty map.");
  MapPair m;
  m.selfCheck();
  checkLineEntriesRoundtrip(m);
  EXPECT_EQ(m.empty(), true);
  EXPECT_EQ(m.numEntries(), 0);
  EXPECT_EQ(m.numLines(), 0);
  EXPECT_EQ(stringb(toGDValue(m)),
    "{}");
  EXPECT_EQ(toGDValue(m.getMappedValues()).asString(),
    "{}");
  EXPECT_EQ(internals(m),
    "TextMCoordMapInternals[values:{} lineData:{length:0}]");

  // Asking about out-of-range lines is allowed, and yields an empty set.
  EXPECT_EQ(lineEntriesString(m, -1), "{}");
  EXPECT_EQ(lineEntriesString(m, 0), "{}");

  DIAG("Insert value 1 at 1:5 to 1:12.");
  m.insert({{{1,5}, {1,12}}, 1});
  m.selfCheck();
  checkLineEntriesRoundtrip(m);
  EXPECT_EQ(m.empty(), false);
  EXPECT_EQ(m.numEntries(), 1);
  EXPECT_EQ(m.numLines(), 2);
  EXPECT_EQ(stringb(toGDValue(m)),
    "{DocEntry[range:MCR(MC(1 5) MC(1 12)) value:1]}");
  EXPECT_EQ(lineEntriesString(m, -1), "{}");
  EXPECT_EQ(lineEntriesString(m, 0),
    "{}");
  EXPECT_EQ(lineEntriesString(m, 1),
    "{LineEntry[startByteIndex:5 endByteIndex:12 value:1]}");
  EXPECT_EQ(lineEntriesString(m, 2), "{}");
  EXPECT_EQ(toGDValue(m.getMappedValues()).asString(),
    "{1}");
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
  checkLineEntriesRoundtrip(m);
  EXPECT_EQ(m.empty(), false);
  EXPECT_EQ(m.numEntries(), 2);
  EXPECT_EQ(m.numLines(), 6);
  EXPECT_EQ(stringb(toGDValue(m)),
    "{DocEntry[range:MCR(MC(1 5) MC(1 12)) value:1] "
     "DocEntry[range:MCR(MC(3 5) MC(5 12)) value:2]}");
  EXPECT_EQ(lineEntriesString(m, 0),
    "{}");
  EXPECT_EQ(lineEntriesString(m, 1),
    "{LineEntry[startByteIndex:5 endByteIndex:12 value:1]}");
  EXPECT_EQ(lineEntriesString(m, 2),
    "{}");
  EXPECT_EQ(lineEntriesString(m, 3),
    "{LineEntry[startByteIndex:5 endByteIndex:null value:2]}");
  EXPECT_EQ(lineEntriesString(m, 4),
    "{LineEntry[startByteIndex:null endByteIndex:null value:2]}");
  EXPECT_EQ(lineEntriesString(m, 5),
    "{LineEntry[startByteIndex:null endByteIndex:12 value:2]}");
  EXPECT_EQ(toGDValue(m.getMappedValues()).asString(),
    "{1 2}");
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
  checkLineEntriesRoundtrip(m);
  EXPECT_EQ(m.empty(), false);
  EXPECT_EQ(m.numEntries(), 2);
  EXPECT_EQ(m.numLines(), 7);
  EXPECT_EQ(stringb(toGDValue(m)),
    "{DocEntry[range:MCR(MC(1 5) MC(1 12)) value:1] "
     "DocEntry[range:MCR(MC(4 5) MC(6 12)) value:2]}");
  EXPECT_EQ(lineEntriesString(m, 0),
    "{}");
  EXPECT_EQ(lineEntriesString(m, 1),
    "{LineEntry[startByteIndex:5 endByteIndex:12 value:1]}");
  EXPECT_EQ(lineEntriesString(m, 2),
    "{}");
  EXPECT_EQ(lineEntriesString(m, 3),
    "{}");
  EXPECT_EQ(lineEntriesString(m, 4),
    "{LineEntry[startByteIndex:5 endByteIndex:null value:2]}");
  EXPECT_EQ(lineEntriesString(m, 5),
    "{LineEntry[startByteIndex:null endByteIndex:null value:2]}");
  EXPECT_EQ(lineEntriesString(m, 6),
    "{LineEntry[startByteIndex:null endByteIndex:12 value:2]}");
  EXPECT_EQ(toGDValue(m.getMappedValues()).asString(),
    "{1 2}");
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
  checkLineEntriesRoundtrip(m);
  EXPECT_EQ(m.empty(), false);
  EXPECT_EQ(m.numEntries(), 2);
  EXPECT_EQ(m.numLines(), 6);
  EXPECT_EQ(stringb(toGDValue(m)),
    "{DocEntry[range:MCR(MC(1 5) MC(1 12)) value:1] "
     "DocEntry[range:MCR(MC(4 5) MC(5 12)) value:2]}");
  EXPECT_EQ(lineEntriesString(m, 0),
    "{}");
  EXPECT_EQ(lineEntriesString(m, 1),
    "{LineEntry[startByteIndex:5 endByteIndex:12 value:1]}");
  EXPECT_EQ(lineEntriesString(m, 2),
    "{}");
  EXPECT_EQ(lineEntriesString(m, 3),
    "{}");
  EXPECT_EQ(lineEntriesString(m, 4),
    "{LineEntry[startByteIndex:5 endByteIndex:null value:2]}");
  EXPECT_EQ(lineEntriesString(m, 5),
    "{LineEntry[startByteIndex:null endByteIndex:12 value:2]}");
  EXPECT_EQ(toGDValue(m.getMappedValues()).asString(),
    "{1 2}");
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


// Insertions within a single line.
void test_lineInsertions()
{
  MapPair m;
  m.selfCheck();

  DIAG("Make a span.");
  m.insert({{{0,5}, {0,10}}, 1});
  m.selfCheck();

  EXPECT_EQ(stringb(toGDValue(m)),
    "{DocEntry[range:MCR(MC(0 5) MC(0 10)) value:1]}");
  // Initial state:
  //             1         2         3
  //   0123456789012345678901234567890
  //        [    )
  //          ^
  //         ins

  DIAG("Insert within.");
  m.insertLineBytes({0, 7}, 1);
  m.selfCheck();

  EXPECT_EQ(stringb(toGDValue(m)),
    "{DocEntry[range:MCR(MC(0 5) MC(0 11)) value:1]}");
  //             1         2         3
  //   0123456789012345678901234567890
  //        [     )
  //        ^
  //       ins

  DIAG("Insert just inside left edge.");
  m.insertLineBytes({0, 5}, 1);
  m.selfCheck();

  EXPECT_EQ(stringb(toGDValue(m)),
    "{DocEntry[range:MCR(MC(0 6) MC(0 12)) value:1]}");
  //             1         2         3
  //   0123456789012345678901234567890
  //         [     )
  //        ^
  //       ins

  DIAG("Insert just outside left edge.");
  m.insertLineBytes({0, 5}, 1);
  m.selfCheck();

  EXPECT_EQ(stringb(toGDValue(m)),
    "{DocEntry[range:MCR(MC(0 7) MC(0 13)) value:1]}");
  //             1         2         3
  //   0123456789012345678901234567890
  //          [     )
  //               ^
  //              ins

  DIAG("Insert just inside right edge.");
  m.insertLineBytes({0, 12}, 1);
  m.selfCheck();

  EXPECT_EQ(stringb(toGDValue(m)),
    "{DocEntry[range:MCR(MC(0 7) MC(0 14)) value:1]}");
  //             1         2         3
  //   0123456789012345678901234567890
  //          [      )
  //                 ^
  //                ins

  DIAG("Insert just outside right edge.");
  m.insertLineBytes({0, 14}, 1);
  m.selfCheck();

  // It is questionable behavior to expand the range here, but that is
  // what my implementation does currently.

  EXPECT_EQ(stringb(toGDValue(m)),
    "{DocEntry[range:MCR(MC(0 7) MC(0 15)) value:1]}");
  //             1         2         3
  //   0123456789012345678901234567890
  //          [       )
}


// Insertions affecting a multi-line span.
void test_multilineInsertions()
{
  MapPair m;
  m.selfCheck();

  DIAG("Make a span.");
  m.insert({{{0,5}, {1,10}}, 1});
  m.selfCheck();

  EXPECT_EQ(stringb(toGDValue(m)),
    "{DocEntry[range:MCR(MC(0 5) MC(1 10)) value:1]}");
  EXPECT_EQ(allLineEntries(m),
    "{LineEntry[startByteIndex:5 endByteIndex:null value:1]}\n"
    "{LineEntry[startByteIndex:null endByteIndex:10 value:1]}\n");
  // Initial state:
  //             1         2         3
  //   0123456789012345678901234567890
  // 0      [
  //          ^ ins
  // 1           )

  DIAG("Insert within first line (no effect).");
  m.insertLineBytes({0, 7}, 1);
  m.selfCheck();

  EXPECT_EQ(stringb(toGDValue(m)),
    "{DocEntry[range:MCR(MC(0 5) MC(1 10)) value:1]}");
  EXPECT_EQ(allLineEntries(m),
    "{LineEntry[startByteIndex:5 endByteIndex:null value:1]}\n"
    "{LineEntry[startByteIndex:null endByteIndex:10 value:1]}\n");
  //             1         2         3
  //   0123456789012345678901234567890
  // 0      [
  // 1           )
  //          ^ ins

  DIAG("Insert within second line.");
  m.insertLineBytes({1, 7}, 1);
  m.selfCheck();

  EXPECT_EQ(stringb(toGDValue(m)),
    "{DocEntry[range:MCR(MC(0 5) MC(1 11)) value:1]}");
  EXPECT_EQ(allLineEntries(m),
    "{LineEntry[startByteIndex:5 endByteIndex:null value:1]}\n"
    "{LineEntry[startByteIndex:null endByteIndex:11 value:1]}\n");
  //             1         2         3
  //   0123456789012345678901234567890
  // 0      [
  //        ^ ins
  // 1            )

  DIAG("Insert just inside left edge.");
  m.insertLineBytes({0, 5}, 1);
  m.selfCheck();

  EXPECT_EQ(stringb(toGDValue(m)),
    "{DocEntry[range:MCR(MC(0 6) MC(1 11)) value:1]}");
  EXPECT_EQ(allLineEntries(m),
    "{LineEntry[startByteIndex:6 endByteIndex:null value:1]}\n"
    "{LineEntry[startByteIndex:null endByteIndex:11 value:1]}\n");
  //             1         2         3
  //   0123456789012345678901234567890
  // 0       [
  //        ^ ins
  // 1            )

  DIAG("Insert just outside left edge.");
  m.insertLineBytes({0, 5}, 1);
  m.selfCheck();

  EXPECT_EQ(stringb(toGDValue(m)),
    "{DocEntry[range:MCR(MC(0 7) MC(1 11)) value:1]}");
  EXPECT_EQ(allLineEntries(m),
    "{LineEntry[startByteIndex:7 endByteIndex:null value:1]}\n"
    "{LineEntry[startByteIndex:null endByteIndex:11 value:1]}\n");
  //             1         2         3
  //   0123456789012345678901234567890
  // 0        [
  // 1            )
  //             ^ ins

  DIAG("Insert just inside right edge.");
  m.insertLineBytes({1, 10}, 1);
  m.selfCheck();

  EXPECT_EQ(stringb(toGDValue(m)),
    "{DocEntry[range:MCR(MC(0 7) MC(1 12)) value:1]}");
  EXPECT_EQ(allLineEntries(m),
    "{LineEntry[startByteIndex:7 endByteIndex:null value:1]}\n"
    "{LineEntry[startByteIndex:null endByteIndex:12 value:1]}\n");
  //             1         2         3
  //   0123456789012345678901234567890
  // 0        [
  // 1             )
  //               ^ ins

  DIAG("Insert just outside right edge.");
  m.insertLineBytes({1, 12}, 1);
  m.selfCheck();

  // It is questionable behavior to expand the range here, but that is
  // what my implementation does currently.

  EXPECT_EQ(stringb(toGDValue(m)),
    "{DocEntry[range:MCR(MC(0 7) MC(1 13)) value:1]}");
  EXPECT_EQ(allLineEntries(m),
    "{LineEntry[startByteIndex:7 endByteIndex:null value:1]}\n"
    "{LineEntry[startByteIndex:null endByteIndex:13 value:1]}\n");
  //             1         2         3
  //   0123456789012345678901234567890
  // 0        [
  // 1              )
}


// Do some deletions within a single line.
void test_lineDeletions()
{
  MapPair m;
  m.selfCheck();

  DIAG("Make a span.");
  m.insert({{{0,10}, {0,20}}, 1});
  m.selfCheck();

  EXPECT_EQ(stringb(toGDValue(m)),
    "{DocEntry[range:MCR(MC(0 10) MC(0 20)) value:1]}");
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
    "{DocEntry[range:MCR(MC(0 10) MC(0 19)) value:1]}");
  //             1         2         3
  //   0123456789012345678901234567890
  //             [        )
  //             ^
  //            del

  DIAG("Delete one byte just inside the left edge.");
  m.deleteLineBytes({0,10}, 1);
  m.selfCheck();

  EXPECT_EQ(stringb(toGDValue(m)),
    "{DocEntry[range:MCR(MC(0 10) MC(0 18)) value:1]}");
  //             1         2         3
  //   0123456789012345678901234567890
  //             [       )
  //            ^
  //           del

  DIAG("Delete one byte just outside the left edge.");
  m.deleteLineBytes({0,9}, 1);
  m.selfCheck();

  EXPECT_EQ(stringb(toGDValue(m)),
    "{DocEntry[range:MCR(MC(0 9) MC(0 17)) value:1]}");
  //             1         2         3
  //   0123456789012345678901234567890
  //            [       )
  //                   ^
  //                  del

  DIAG("Delete one byte just inside the right edge.");
  m.deleteLineBytes({0,16}, 1);
  m.selfCheck();

  EXPECT_EQ(stringb(toGDValue(m)),
    "{DocEntry[range:MCR(MC(0 9) MC(0 16)) value:1]}");
  //             1         2         3
  //   0123456789012345678901234567890
  //            [      )
  //                   ^
  //                  del

  DIAG("Delete one byte just outside the right edge (no effect).");
  m.deleteLineBytes({0,16}, 1);
  m.selfCheck();

  EXPECT_EQ(stringb(toGDValue(m)),
    "{DocEntry[range:MCR(MC(0 9) MC(0 16)) value:1]}");
  //             1         2         3
  //   0123456789012345678901234567890
  //            [      )
  //           ^^
  //          del

  DIAG("Delete two bytes straddling the left edge.");
  m.deleteLineBytes({0,8}, 2);
  m.selfCheck();

  EXPECT_EQ(stringb(toGDValue(m)),
    "{DocEntry[range:MCR(MC(0 8) MC(0 14)) value:1]}");
  //             1         2         3
  //   0123456789012345678901234567890
  //           [     )
  //                ^^
  //               del

  DIAG("Delete two bytes straddling the right edge.");
  m.deleteLineBytes({0,13}, 2);
  m.selfCheck();

  EXPECT_EQ(stringb(toGDValue(m)),
    "{DocEntry[range:MCR(MC(0 8) MC(0 13)) value:1]}");
  //             1         2         3
  //   0123456789012345678901234567890
  //           [    )
  //           ^^^^^
  //            del

  DIAG("Delete the exact range.");
  m.deleteLineBytes({0,8}, 5);
  m.selfCheck();

  EXPECT_EQ(stringb(toGDValue(m)),
    "{DocEntry[range:MCR(MC(0 8) MC(0 8)) value:1]}");
  //             1         2         3
  //   0123456789012345678901234567890
  //           H
  //          ^^
  //          del

  DIAG("Delete two bytes straddling the empty range.");
  m.deleteLineBytes({0,7}, 2);
  m.selfCheck();

  EXPECT_EQ(stringb(toGDValue(m)),
    "{DocEntry[range:MCR(MC(0 7) MC(0 7)) value:1]}");
  //             1         2         3
  //   0123456789012345678901234567890
  //          H
  //   ^^^^^^^
  //     del

  DIAG("Delete all preceding bytes.");
  m.deleteLineBytes({0,0}, 7);
  m.selfCheck();

  EXPECT_EQ(stringb(toGDValue(m)),
    "{DocEntry[range:MCR(MC(0 0) MC(0 0)) value:1]}");
  //             1         2         3
  //   0123456789012345678901234567890
  //   H
}


// This test is smaller than the others because I didn't really do it
// properly.  It just has a specific example I found during randomized
// testing.
void test_multilineDeletions()
{
  MapPair m;

  m.insert({{{2,24}, {4,1}}, 3});
  m.selfCheck();
  EXPECT_EQ(stringb(toGDValue(m)),
    "{DocEntry[range:MCR(MC(2 24) MC(4 1)) value:3]}");

  //             1         2         3
  //   0123456789012345678901234567890
  // 0
  // 1
  // 2                         [
  // 3
  // 4  )         <-- del two lines starting here

  m.deleteLines(4, 2);
  m.selfCheck();
  EXPECT_EQ(stringb(toGDValue(m)),
    "{DocEntry[range:MCR(MC(2 24) MC(4 0)) value:3]}");

  //             1         2         3
  //   0123456789012345678901234567890
  // 0
  // 1
  // 2                         [
  // 3
  // 4 )
}


// A specific scenario found through random testing.
void test_multilineDeletion2()
{
  MapPair m;

  m.insert({{{0,21}, {3,0}}, 3});
  m.selfCheck();
  EXPECT_EQ(stringb(toGDValue(m)),
    "{DocEntry[range:MCR(MC(0 21) MC(3 0)) value:3]}");
  //             1         2         3
  //   0123456789012345678901234567890
  // 0                      [
  // 1
  // 2
  // 3 )

  m.deleteLines(0, 2);
  m.selfCheck();
  EXPECT_EQ(stringb(toGDValue(m)),
    "{DocEntry[range:MCR(MC(0 0) MC(1 0)) value:3]}");
  //             1         2         3
  //   0123456789012345678901234567890
  // 0 [
  // 1 )
}


// Return a number in [0,n-1], approximately uniformly at random.
int random(int n)
{
  return std::rand() % n;
}


int randomLine()
{
  return random(20);
}

int randomColumn()
{
  return random(40);
}


// Facilitate making a weighted random choice.
//
// Candidate to move to someplace more general.
class RandomChoice {
public:      // data
  // Size of the uniform range.
  int m_rangeSize;

  // We've checked for all numbers below this value.
  int m_checkLimit;

  // Selected element in [0, m_rangeSize-1].
  int m_choice;

public:
  RandomChoice(int rangeSize)
    : m_rangeSize(rangeSize),
      m_checkLimit(0),
      m_choice(random(rangeSize))
  {}

  // Check whether the choice lands within the next `n` numbers.  That
  // is, the probability of `check(n)` is proportional to `n`.  The sum
  // of all `n` passed to `check` must not exceed `m_rangeSize`.
  bool check(int n)
  {
    int oldLimit = m_checkLimit;
    m_checkLimit += n;
    xassert(m_checkLimit <= m_rangeSize);

    return oldLimit <= m_choice &&
                       m_choice < m_checkLimit;
  }

  // True if the choice has not been in any checked range.
  bool remains() const
  {
    return m_choice >= m_checkLimit;
  }
};


void randomInsert(MapPair &m)
{
  int spanID = m.numEntries() + 1;

  int startLine = randomLine();
  int startCol = randomColumn();

  int endLine;
  int endCol;

  if (random(7) == 0) {
    // Multi-line (rare).
    endLine = startLine + 1 + random(2);
    endCol = randomColumn();
  }
  else {
    // Single-line (common).
    endLine = startLine;
    endCol = randomColumn() + startCol;
  }

  m.insert(MapPair::DocEntry(
    TextMCoordRange(
      TextMCoord(startLine, startCol),
      TextMCoord(endLine, endCol)
    ),
    spanID));

  m.selfCheck();
}


void randomInsertions(MapPair &m, int n)
{
  while (n--) {
    randomInsert(m);
  }
}


void randomEdit(MapPair &m)
{
  RandomChoice c(803);

  if (c.check(2)) {
    // Insert a new span after (most likely) having done some edits.
    randomInsert(m);
  }

  else if (c.check(1)) {
    m.clear();
    m.selfCheck();
    randomInsertions(m, 10);
  }

  else if (c.check(200)) {
    m.insertLines(randomLine(), random(3));
  }

  else if (c.check(200)) {
    m.deleteLines(randomLine(), random(3));
  }

  else if (c.check(200)) {
    m.insertLineBytes(TextMCoord(randomLine(), randomColumn()), randomColumn());
  }

  else if (c.check(200)) {
    m.deleteLineBytes(TextMCoord(randomLine(), randomColumn()), randomColumn());
  }

  else {
    xfailure("impossible");
  }
}


void test_randomOps()
{
  // On my machine, with the defaults, the test takes ~1s.
  int const outerLimit = envAsIntOr(10, "TMT_OUTER_LIMIT");
  int const innerLimit = envAsIntOr(100, "TMT_INNER_LIMIT");

  for (int outer=0; outer < outerLimit; ++outer) {
    EXN_CONTEXT_EXPR(outer);

    MapPair m;
    m.selfCheck();

    randomInsertions(m, 10);

    for (int inner=0; inner < innerLimit; ++inner) {
      EXN_CONTEXT_EXPR(inner);

      randomEdit(m);
      m.selfCheck();
    }

    checkLineEntriesRoundtrip(m);
  }
}


// Test issuing edit commands on top of an empty map.
void test_editEmpty()
{
  MapPair m;
  m.selfCheck();

  m.insertLineBytes({13,13}, 2);
  m.deleteLineBytes({13,31}, 21);
  m.insertLines(18, 1);
  m.deleteLines(10, 2);
}


// Issue with inserting right after the last range.
void test_insertAfterLast()
{
  MapPair m;

  m.insert({{{1,4}, {1,42}}, 2});
  m.selfCheck();
  EXPECT_EQ(m.numLines(), 2);

  m.insertLines(2, 1);
  m.selfCheck();
  EXPECT_EQ(m.numLines(), 2);
}


// Clearing should ensure `numLines()==0`.
void test_clear()
{
  MapPair m;
  m.insert({{{1,4}, {1,42}}, 2});
  m.selfCheck();
  EXPECT_EQ(m.numLines(), 2);

  m.clear();
  m.selfCheck();
  EXPECT_EQ(m.numLines(), 0);
}


// Another one found by random testing.
void test_multilineDeletion3()
{
  MapPair m;

  m.insert({{{3,0}, {4,8}}, 2});

  VPVAL(toGDValue(m));

  // The issue here is we have a multiline deletion that ends just
  // before the line containing the endpoint.  Consequently, what was
  // a multiline range has to be converted to a single-line range.
  m.deleteLines(3, 1);

  m.selfCheck();
}


// Found by random testing.
void test_multilineDeletion4()
{
  MapPair m;

  m.insert({{{19,11}, {20,27}}, 3});

  VPVAL(toGDValue(m));

  // Multiline deletion that covers the entire span.
  m.deleteLines(19, 2);

  VPVAL(toGDValue(m));
  m.selfCheck();
}


// The issue here is that an insertion creates a line with a span start
// greater than 99, which for a while was my crude sentinel in the test
// code.
void test_insertMakesLongLine()
{
  MapPair m;

  m.insert({{{0,94}, {1,0}}, 3});
  m.selfCheck();

  m.insertLineBytes({0,11}, 33);
  m.selfCheck();
}


// Turn `gdvn` into a `LineEntry` and back, checking for equality.
void checkLineEntryRoundtrip(char const *gdvn)
{
  GDValue v(GDValue::readFromStringView(gdvn));
  TextMCoordMap::LineEntry le{GDValueParser(v)};
  EXPECT_EQ(toGDValue(le).asString(), gdvn);
}


// Test parsing `LineEntry`.
void test_parseLineEntry()
{
  checkLineEntryRoundtrip(
    "LineEntry[startByteIndex:1 endByteIndex:2 value:3]");
  checkLineEntryRoundtrip(
    "LineEntry[startByteIndex:null endByteIndex:2 value:3]");
  checkLineEntryRoundtrip(
    "LineEntry[startByteIndex:1 endByteIndex:null value:3]");
  checkLineEntryRoundtrip(
    "LineEntry[startByteIndex:null endByteIndex:null value:3]");

  EXPECT_EXN_SUBSTR(
    checkLineEntryRoundtrip(
      "LineEntry[startByteIndex:2 endByteIndex:1 value:3]"),
    XGDValueError,
    "m_startByteIndex.value() <= m_endByteIndex.value()");
}


// Ad-hoc reproduction of problematic sequences.
void test_repro()
{
  MapPair m;

  return;
}


void entry(int argc, char **argv)
{
  #define RUN_TEST(funcname) { \
    EXN_CONTEXT(#funcname);    \
    funcname();                \
  }

  RUN_TEST(test_repro);
  RUN_TEST(test_editEmpty);
  RUN_TEST(test_commentsExample);
  RUN_TEST(test_lineInsertions);
  RUN_TEST(test_multilineInsertions);
  RUN_TEST(test_multilineDeletions);
  RUN_TEST(test_multilineDeletion2);
  RUN_TEST(test_multilineDeletion3);
  RUN_TEST(test_multilineDeletion4);
  RUN_TEST(test_lineDeletions);
  RUN_TEST(test_insertAfterLast);
  RUN_TEST(test_clear);
  RUN_TEST(test_insertMakesLongLine);
  RUN_TEST(test_randomOps);
  RUN_TEST(test_parseLineEntry);

  #undef RUN_TEST
}


CLOSE_ANONYMOUS_NAMESPACE


ARGS_TEST_MAIN


// EOF
