// textmcoord-map-test.cc
// Tests for `textmcoord-map` module.

#include "smbase/gdvalue-set-fwd.h"    // gdv::toGDValue(std::set)

#include "unit-tests.h"                // decl for my entry point

#include "textmcoord-map.h"            // module under test

#include "line-count.h"                // PositiveLineCount
#include "td-core.h"                   // TextDocumentCore

#include "smbase/chained-cond.h"       // smbase::cc::{z_le_le_le, le_lt, z_le_lt}
#include "smbase/gdvalue-parser.h"     // gdv::GDValueParser
#include "smbase/gdvalue-optional.h"   // gdv::toGDValue(std::optional)
#include "smbase/gdvalue-set.h"        // gdv::toGDValue(std::set)
#include "smbase/gdvalue.h"            // gdv::toGDValue
#include "smbase/set-util.h"           // smbase::setInsert
#include "smbase/sm-macros.h"          // OPEN_ANONYMOUS_NAMESPACE, NO_OBJECT_COPIES, IMEMBFP
#include "smbase/sm-random.h"          // smbase::{sm_random, RandomChoice}
#include "smbase/sm-test.h"            // ARGS_MAIN, EXPECT_EQ[_GDV], envRandomizedTestIters, [TIMED_]TEST_FUNC, DIAG[2]
#include "smbase/string-util.h"        // join, suffixAll, stringToVectorOfUChar

#include <algorithm>                   // std::max
#include <cstdlib>                     // std::rand
#include <iostream>                    // std::cout
#include <set>                         // std::set


using namespace gdv;
using namespace smbase;


// This class tests some things that require accessing private members
// of `TextMCoordMap`.
class TextMCoordMap_LineData_Tester {
public:      // types
  typedef TextMCoordMap::SingleLineSpan SingleLineSpan;
  typedef TextMCoordMap::Boundary Boundary;
  typedef TextMCoordMap::LineData LineData;

public:      // methods
  static void testCopyCompare()
  {
    LineData d;
    setInsert(d.m_singleLineSpans, SingleLineSpan(ByteIndex(1), ByteIndex(2), 3));
    setInsert(d.m_startsHere, Boundary(ByteIndex(4), 5));
    setInsert(d.m_continuesHere, 6);
    setInsert(d.m_endsHere, Boundary(ByteIndex(7), 8));

    {
      LineData d2(d);
      xassert(d == d2);
      xassert(!( d != d2 ));
      EXPECT_EQ(toGDValue(d2), toGDValue(d));

      // Check that comparison detects changes in each field.
      d2.m_singleLineSpans.clear();
      xassert(d != d2);
      xassert(!( d == d2 ));
    }

    {
      LineData d2(d);
      xassert(d == d2);
      d2.m_startsHere.clear();
      xassert(d != d2);
    }

    {
      LineData d2(d);
      xassert(d == d2);
      d2.m_continuesHere.clear();
      xassert(d != d2);
    }

    {
      LineData d2(d);
      xassert(d == d2);
      d2.m_endsHere.clear();
      xassert(d != d2);
    }
  }
};


OPEN_ANONYMOUS_NAMESPACE


TextMCoord tmc(int l, int b)
{
  return TextMCoord(LineIndex(l), ByteIndex(b));
}


// Adjust `mc` by `amt`, capping at relevant boundaries.  This is just
// meant to compute a nearby location if possible for probing purposes.
TextMCoord incMC(TextMCoord mc, int amt)
{
  if (amt < 0) {
    if (mc.m_byteIndex < -amt) {
      if (mc.m_line == 0) {
        // At start, return zero.
        return TextMCoord();
      }
      else {
        // Go to previous line.
        return TextMCoord(mc.m_line.pred(), mc.m_byteIndex);
      }
    }
  }

  return TextMCoord(mc.m_line, ByteIndex(mc.m_byteIndex.get() + amt));
}


TextMCoordRange tmcr(int sl, int sb, int el, int eb)
{
  return TextMCoordRange(
    TextMCoord(LineIndex(sl), ByteIndex(sb)),
    TextMCoord(LineIndex(el), ByteIndex(eb)));
}


void test_DocEntry_contains_orAtCollapsed()
{
  {
    TextMCoordMap::DocEntry de(tmcr(1, 2, 3, 4), 5);
    EXPECT_FALSE(de.contains_orAtCollapsed(tmc(1, 1)));
    EXPECT_TRUE(de.contains_orAtCollapsed(tmc(1, 2)));
  }

  {
    TextMCoordMap::DocEntry de(tmcr(1, 2, 1, 2), 5);
    EXPECT_FALSE(de.contains_orAtCollapsed(tmc(1, 1)));
    EXPECT_TRUE(de.contains_orAtCollapsed(tmc(1, 2)));
    EXPECT_FALSE(de.contains_orAtCollapsed(tmc(1, 3)));
  }
}


void test_lineData()
{
  TextMCoordMap_LineData_Tester tester;
  tester.testCopyCompare();
}


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

  // Number of lines if known.  Must be positive if set.
  std::optional<PositiveLineCount> m_numLines;

public:
  ~ReferenceMap() = default;

  ReferenceMap(std::optional<PositiveLineCount> numLines)
    : m_entries(),
      IMEMBFP(numLines)
  {
    selfCheck();
  }

  void selfCheck()
  {
    if (m_numLines.has_value()) {
      xassert(*m_numLines > 0);
    }
  }

  void insertEntry(DocEntry entry)
  {
    m_entries.insert(entry);
  }

  void clearEverything(std::optional<PositiveLineCount> numLines)
  {
    m_entries.clear();
    m_numLines = numLines;
  }


  static TextMCoord adjustMC_forDocument(
    TextMCoord mc, TextDocumentCore const &doc)
  {
    doc.adjustMCoord(mc);
    return mc;
  }

  void adjustForDocument(TextDocumentCore const &doc)
  {
    std::set<DocEntry> newEntries;

    for (DocEntry const &e : m_entries) {
      newEntries.insert(DocEntry(
        TextMCoordRange(
          adjustMC_forDocument(e.m_range.m_start, doc),
          adjustMC_forDocument(e.m_range.m_end, doc)
        ),
        e.m_value));
    }

    m_entries.swap(newEntries);

    m_numLines = doc.numLines();
  }


  static TextMCoord adjustMC_insertLines(TextMCoord mc, LineIndex line, LineCount count)
  {
    // Push at or later lines down by `count`.
    if (mc.m_line >= line) {
      mc.m_line += count;
    }
    return mc;
  }

  // The logic for inserting bytes exactly parallels the logic for
  // inserting lines.
  static TextMCoord adjustMC_insertLineBytes(
    TextMCoord mc, TextMCoord tc, ByteCount lengthBytes)
  {
    if (mc.m_line == tc.m_line) {
      // Push at or later bytes right by `lengthBytes`.
      if (mc.m_byteIndex >= tc.m_byteIndex) {
        mc.m_byteIndex += lengthBytes;
      }
    }

    return mc;
  }

  TextMCoord adjustMC_deleteLines(TextMCoord mc, LineIndex line, LineCount count) const
  {
    if (cc::le_lt(line, mc.m_line, line+count)) {
      // The endpoint is in the deleted region, so its byte index gets
      // zeroed.
      mc.m_byteIndex.set(0);
    }

    // Pull later lines up, but not above `line`.
    if (mc.m_line > line) {
      mc.m_line.clampIncrease(-count, line);
    }

    // Except, the line must be less than `*m_numLines`.
    xassert(mc.m_line <= *m_numLines);
    if (mc.m_line.get() == *m_numLines) {
      mc.m_line = LineIndex(m_numLines->pred());
    }

    return mc;
  }

  static TextMCoord adjustMC_deleteLineBytes(
    TextMCoord mc, TextMCoord tc, ByteCount lengthBytes)
  {
    if (mc.m_line == tc.m_line) {
      // Pull later bytes left, but not in front of `tc.m_byteIndex`.
      if (mc.m_byteIndex > tc.m_byteIndex) {
        mc.m_byteIndex.clampDecrease(lengthBytes, tc.m_byteIndex);
      }
    }

    return mc;
  }

  void insertLines(LineIndex line, LineCount count)
  {
    xassert(m_numLines.has_value());

    // Any number of lines can be appended, so we merely require that
    // `line` be at most `*m_numLines`.
    xassert(line <= *m_numLines);
    xassert(count >= 0);

    *m_numLines += count;

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

  // If the end is before the start, return a range where the end is at
  // the start.
  //
  // This is needed because `deleteLines` can move an endpoint on a
  // deleted last line to the start of the preceding line, but if the
  // start is on that line, then we want to match it.
  TextMCoordRange fixEnd(TextMCoordRange range)
  {
    if (range.m_end < range.m_start) {
      range.m_end = range.m_start;
    }
    return range;
  }

  void deleteLines(LineIndex line, LineCount count)
  {
    xassert(m_numLines.has_value());

    // All the lines being deleted must currently exist.
    xassert(cc::le_le(line, line+count, LineIndex(*m_numLines)));

    // You can't delete all of the lines.
    xassert(count < *m_numLines);

    *m_numLines -= count;

    std::set<DocEntry> newEntries;

    for (DocEntry const &e : m_entries) {
      newEntries.insert(DocEntry(
        fixEnd(TextMCoordRange(
          adjustMC_deleteLines(e.m_range.m_start, line, count),
          adjustMC_deleteLines(e.m_range.m_end, line, count)
        )),
        e.m_value));
    }

    m_entries.swap(newEntries);
  }

  void insertLineBytes(TextMCoord tc, ByteCount lengthBytes)
  {
    xassert(m_numLines.has_value());
    xassert(tc.m_line < *m_numLines);

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

  void deleteLineBytes(TextMCoord tc, ByteCount lengthBytes)
  {
    xassert(m_numLines.has_value());
    xassert(tc.m_line < *m_numLines);

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

  int maxEntryLine() const
  {
    int ret = -1;

    for (DocEntry const &e : m_entries) {
      ret = std::max(ret, e.m_range.m_end.m_line.get());
    }

    return ret;
  }

  LineCount numLinesWithData() const
  {
    return LineCount(maxEntryLine() + 1);
  }

  std::optional<PositiveLineCount> getNumLinesOpt() const
  {
    return m_numLines;
  }

  PositiveLineCount getNumLines() const
  {
    xassert(m_numLines.has_value());
    return *m_numLines;
  }

  std::set<LineEntry> getLineEntries(LineIndex line) const
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

  std::set<DocEntry> getEntriesContaining_orAtCollapsed(
    TextMCoord tc) const
  {
    std::set<DocEntry> ret;

    for (DocEntry const &e : m_entries) {
      if (e.contains_orAtCollapsed(tc)) {
        ret.insert(e);
      }
    }

    return ret;
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
    // This isn't actually a `TextMCoordMap`, but it will be compared
    // with one, expecting equality.
    GDValue m(GDVK_TAGGED_ORDERED_MAP, "TextMCoordMap"_sym);
    GDV_WRITE_MEMBER_SYM(m_numLines);
    m.mapSetValueAtSym("entries", toGDValue(getAllEntries()));
    return m;
  }
};


// Check that `m`, regarded as the "actual" value, and `r`, regarded as
// the "expected" value, agree in all respects.
void checkSame(TextMCoordMap const &m, ReferenceMap const &r)
{
  EXN_CONTEXT("checkSame");

  EXPECT_EQ(m.empty(), r.empty());
  EXPECT_EQ(m.numEntries(), r.numEntries());
  EXPECT_EQ(m.maxEntryLine(), r.maxEntryLine());
  EXPECT_EQ_GDV(m.getNumLinesOpt(), r.getNumLinesOpt());

  // Compare as GDValue first so we get a printout on mismatch.
  EXPECT_EQ(toGDValue(m), toGDValue(r));

  // But then also compare without the conversion.  I don't know why
  // this would ever fail if the above succeeded, but it won't hurt.
  std::set<TextMCoordMap::DocEntry> actualEntries = m.getAllEntries();
  xassert(actualEntries == r.getAllEntries());

  // Probe the perimeter locations of every entry to test the behavior
  // of `getEntriesContaining_orAtCollapsed`.  That function should
  // behave the same for *all* coordinates, but probing them all would
  // be fairly expensive, and the boundaries are probably sufficient.
  for (auto const &entry : actualEntries) {
    EXN_CONTEXT(toGDValue(entry));

    auto probe = [&m, &r](TextMCoord tc) {
      EXN_CONTEXT(tc);

      EXPECT_EQ_GDVSER(
        m.getEntriesContaining_orAtCollapsed(tc),
        r.getEntriesContaining_orAtCollapsed(tc));
    };

    probe(incMC(entry.m_range.m_start, -1));
    probe(incMC(entry.m_range.m_start,  0));
    probe(incMC(entry.m_range.m_start, +1));

    probe(incMC(entry.m_range.m_end, -1));
    probe(incMC(entry.m_range.m_end,  0));
    probe(incMC(entry.m_range.m_end, +1));

    if (entry.m_range.m_start < entry.m_range.m_end) {
      // Probe the second line of a multiline range.
      probe(TextMCoord(entry.m_range.m_start.m_line.succ(),
                       entry.m_range.m_start.m_byteIndex));
    }
  }

  for (LineIndex i(0); i < r.numLinesWithData(); ++i) {
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

  MapPair(std::optional<PositiveLineCount> numLines)
    : m_sut(numLines),
      m_ref(numLines)
  {
    selfCheck();
  }

  void selfCheck()
  {
    m_sut.selfCheck();
    m_ref.selfCheck();
    checkSame(m_sut, m_ref);
  }

  // Mutations: Perform on each in parallel.

  void insertEntry(DocEntry entry)
  {
    // Print these operations as C++ code that I can copy into my
    // tests to recreate a scenario that was generated randomly.
    DIAG2("m.insert(" << toCode(entry) << ");");

    m_sut.insertEntry(entry);
    m_ref.insertEntry(entry);
  }

  void clearEverything(std::optional<PositiveLineCount> numLines)
  {
    DIAG2("m.clear();");
    m_sut.clearEverything(numLines);
    m_ref.clearEverything(numLines);
  }

  void adjustForDocument(TextDocumentCore const &doc)
  {
    DIAG2("m.adjustForDocument(doc);");
    m_sut.adjustForDocument(doc);
    m_ref.adjustForDocument(doc);
  }

  void insertLines(LineIndex line, LineCount count)
  {
    DIAG2("m.insertLines(" << line << ", " << count << ");");
    m_sut.insertLines(line, count);
    m_ref.insertLines(line, count);
  }

  void deleteLines(LineIndex line, LineCount count)
  {
    DIAG2("m.deleteLines(" << line << ", " << count << ");");
    m_sut.deleteLines(line, count);
    m_ref.deleteLines(line, count);
  }

  void insertLineBytes(TextMCoord tc, ByteCount lengthBytes)
  {
    DIAG2("m.insertLineBytes(" << toCode(tc) << ", " << lengthBytes << ");");
    m_sut.insertLineBytes(tc, lengthBytes);
    m_ref.insertLineBytes(tc, lengthBytes);
  }

  void deleteLineBytes(TextMCoord tc, ByteCount lengthBytes)
  {
    DIAG2("m.deleteLineBytes(" << toCode(tc) << ", " << lengthBytes << ");");
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

  int maxEntryLine() const
  {
    return m_sut.maxEntryLine();
  }

  LineCount numLinesWithData() const
  {
    return m_sut.numLinesWithData();
  }

  PositiveLineCount getNumLines() const
  {
    return m_sut.getNumLines();
  }

  std::set<LineEntry> getLineEntries(LineIndex line) const
  {
    return m_sut.getLineEntries(line);
  }

  std::set<DocEntry> getAllEntries() const
  {
    return m_sut.getAllEntries();
  }

  std::set<DocEntry> getEntriesContaining_orAtCollapsed(
    TextMCoord tc) const
  {
    return m_sut.getEntriesContaining_orAtCollapsed(tc);
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
  if (line < 0) {
    // Disable tests that relied on negative line indices.
    return "{}";
  }

  return toGDValue(m.getLineEntries(LineIndex(line))).asString();
}


// Get all line entries, each terminated by a newline.
std::string allLineEntries(MapPair const &m)
{
  std::vector<std::string> results;

  for (int i=0; i <= m.maxEntryLine(); ++i) {
    results.push_back(lineEntriesString(m, i));
  }

  return join(suffixAll(results, "\n"), "");
}


// Check that every `LineEntry` in `m` can de/serialize to itself.
void checkLineEntriesRoundtrip(MapPair const &m)
{
  typedef MapPair::LineEntry LineEntry;

  for (LineIndex i(0); i < m.numLinesWithData(); ++i) {
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
  TEST_FUNC();

  DIAG("Start with empty map.");
  MapPair m(PositiveLineCount(7) /*numLines*/);
  m.selfCheck();
  checkLineEntriesRoundtrip(m);
  EXPECT_EQ(m.empty(), true);
  EXPECT_EQ(m.numEntries(), 0);
  EXPECT_EQ(m.maxEntryLine(), -1);
  EXPECT_EQ(stringb(toGDValue(m)),
    "TextMCoordMap[numLines:7 entries:{}]");
  EXPECT_EQ(toGDValue(m.getMappedValues()).asString(),
    "{}");
  EXPECT_EQ(internals(m),
    "TextMCoordMapInternals[numLines:7 values:{} lineData:{length:0}]");

  // Asking about out-of-range lines is allowed, and yields an empty set.
  EXPECT_EQ(lineEntriesString(m, -1), "{}");
  EXPECT_EQ(lineEntriesString(m, 0), "{}");

  DIAG("Insert value 1 at 1:5 to 1:12.");
  m.insertEntry({tmcr(1,5, 1,12), 1});
  m.selfCheck();
  checkLineEntriesRoundtrip(m);
  EXPECT_EQ(m.empty(), false);
  EXPECT_EQ(m.numEntries(), 1);
  EXPECT_EQ(m.maxEntryLine(), 1);
  EXPECT_EQ(stringb(toGDValue(m)),
    "TextMCoordMap["
      "numLines:7 "
      "entries:{DocEntry[range:MCR(MC(1 5) MC(1 12)) value:1]}"
    "]");
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
      numLines: 7
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
  m.insertEntry({tmcr(3,5, 5,12), 2});
  m.selfCheck();
  checkLineEntriesRoundtrip(m);
  EXPECT_EQ(m.empty(), false);
  EXPECT_EQ(m.numEntries(), 2);
  EXPECT_EQ(m.maxEntryLine(), 5);
  EXPECT_EQ(stringb(toGDValue(m)),
    "TextMCoordMap["
      "numLines:7 "
      "entries:{"
        "DocEntry[range:MCR(MC(1 5) MC(1 12)) value:1] "
        "DocEntry[range:MCR(MC(3 5) MC(5 12)) value:2]"
      "}"
    "]");
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
      numLines: 7
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
  m.insertLines(LineIndex(3), LineCount(1));
  m.selfCheck();
  checkLineEntriesRoundtrip(m);
  EXPECT_EQ(m.empty(), false);
  EXPECT_EQ(m.numEntries(), 2);
  EXPECT_EQ(m.maxEntryLine(), 6);
  EXPECT_EQ(stringb(toGDValue(m)),
    "TextMCoordMap["
      "numLines:8 "
      "entries:{"
        "DocEntry[range:MCR(MC(1 5) MC(1 12)) value:1] "
        "DocEntry[range:MCR(MC(4 5) MC(6 12)) value:2]"
      "}"
    "]");
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
      numLines: 8
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
  m.deleteLines(LineIndex(5), LineCount(1));
  m.selfCheck();
  checkLineEntriesRoundtrip(m);
  EXPECT_EQ(m.empty(), false);
  EXPECT_EQ(m.numEntries(), 2);
  EXPECT_EQ(m.maxEntryLine(), 5);
  EXPECT_EQ(stringb(toGDValue(m)),
    "TextMCoordMap["
      "numLines:7 "
      "entries:{"
        "DocEntry[range:MCR(MC(1 5) MC(1 12)) value:1] "
        "DocEntry[range:MCR(MC(4 5) MC(5 12)) value:2]"
      "}"
    "]");
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
      numLines: 7
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


// Get the set of entries as a GDVN string.
std::string allEntriesString(MapPair const &m)
{
  return toGDValue(m.getAllEntries()).asIndentedString();
}


// Insertions within a single line.
void test_lineInsertions()
{
  TEST_FUNC();

  MapPair m(PositiveLineCount(1) /*numLines*/);
  m.selfCheck();

  DIAG("Make a span.");
  m.insertEntry({tmcr(0,5, 0,10), 1});
  m.selfCheck();

  EXPECT_EQ(allEntriesString(m),
    "{DocEntry[range:MCR(MC(0 5) MC(0 10)) value:1]}");
  // Initial state:
  //             1         2         3
  //   0123456789012345678901234567890
  //        [    )
  //          ^
  //         ins

  DIAG("Insert within.");
  m.insertLineBytes(tmc(0,7), ByteCount(1));
  m.selfCheck();

  EXPECT_EQ(allEntriesString(m),
    "{DocEntry[range:MCR(MC(0 5) MC(0 11)) value:1]}");
  //             1         2         3
  //   0123456789012345678901234567890
  //        [     )
  //        ^
  //       ins

  DIAG("Insert just inside left edge.");
  m.insertLineBytes(tmc(0,5), ByteCount(1));
  m.selfCheck();

  EXPECT_EQ(allEntriesString(m),
    "{DocEntry[range:MCR(MC(0 6) MC(0 12)) value:1]}");
  //             1         2         3
  //   0123456789012345678901234567890
  //         [     )
  //        ^
  //       ins

  DIAG("Insert just outside left edge.");
  m.insertLineBytes(tmc(0,5), ByteCount(1));
  m.selfCheck();

  EXPECT_EQ(allEntriesString(m),
    "{DocEntry[range:MCR(MC(0 7) MC(0 13)) value:1]}");
  //             1         2         3
  //   0123456789012345678901234567890
  //          [     )
  //               ^
  //              ins

  DIAG("Insert just inside right edge.");
  m.insertLineBytes(tmc(0,12), ByteCount(1));
  m.selfCheck();

  EXPECT_EQ(allEntriesString(m),
    "{DocEntry[range:MCR(MC(0 7) MC(0 14)) value:1]}");
  //             1         2         3
  //   0123456789012345678901234567890
  //          [      )
  //                 ^
  //                ins

  DIAG("Insert just outside right edge.");
  m.insertLineBytes(tmc(0,14), ByteCount(1));
  m.selfCheck();

  // It is questionable behavior to expand the range here, but that is
  // what my implementation does currently.

  EXPECT_EQ(allEntriesString(m),
    "{DocEntry[range:MCR(MC(0 7) MC(0 15)) value:1]}");
  //             1         2         3
  //   0123456789012345678901234567890
  //          [       )
}


// Insertions affecting a multi-line span.
void test_multilineInsertions()
{
  TEST_FUNC();

  MapPair m(PositiveLineCount(2) /*numLines*/);
  m.selfCheck();

  DIAG("Make a span.");
  m.insertEntry({tmcr(0,5, 1,10), 1});
  m.selfCheck();

  EXPECT_EQ(allEntriesString(m),
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
  m.insertLineBytes(tmc(0,7), ByteCount(1));
  m.selfCheck();

  EXPECT_EQ(allEntriesString(m),
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
  m.insertLineBytes(tmc(1,7), ByteCount(1));
  m.selfCheck();

  EXPECT_EQ(allEntriesString(m),
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
  m.insertLineBytes(tmc(0,5), ByteCount(1));
  m.selfCheck();

  EXPECT_EQ(allEntriesString(m),
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
  m.insertLineBytes(tmc(0,5), ByteCount(1));
  m.selfCheck();

  EXPECT_EQ(allEntriesString(m),
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
  m.insertLineBytes(tmc(1,10), ByteCount(1));
  m.selfCheck();

  EXPECT_EQ(allEntriesString(m),
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
  m.insertLineBytes(tmc(1,12), ByteCount(1));
  m.selfCheck();

  // It is questionable behavior to expand the range here, but that is
  // what my implementation does currently.

  EXPECT_EQ(allEntriesString(m),
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
  TEST_FUNC();

  MapPair m(PositiveLineCount(1) /*numLines*/);
  m.selfCheck();

  DIAG("Make a span.");
  m.insertEntry({tmcr(0,10, 0,20), 1});
  m.selfCheck();

  EXPECT_EQ(allEntriesString(m),
    "{DocEntry[range:MCR(MC(0 10) MC(0 20)) value:1]}");
  // Initial state:
  //             1         2         3
  //   0123456789012345678901234567890
  //             [         )
  //                 ^
  //                del

  DIAG("Delete one byte in the middle.");
  m.deleteLineBytes(tmc(0,14), ByteCount(1));
  m.selfCheck();

  EXPECT_EQ(allEntriesString(m),
    "{DocEntry[range:MCR(MC(0 10) MC(0 19)) value:1]}");
  //             1         2         3
  //   0123456789012345678901234567890
  //             [        )
  //             ^
  //            del

  DIAG("Delete one byte just inside the left edge.");
  m.deleteLineBytes(tmc(0,10), ByteCount(1));
  m.selfCheck();

  EXPECT_EQ(allEntriesString(m),
    "{DocEntry[range:MCR(MC(0 10) MC(0 18)) value:1]}");
  //             1         2         3
  //   0123456789012345678901234567890
  //             [       )
  //            ^
  //           del

  DIAG("Delete one byte just outside the left edge.");
  m.deleteLineBytes(tmc(0,9), ByteCount(1));
  m.selfCheck();

  EXPECT_EQ(allEntriesString(m),
    "{DocEntry[range:MCR(MC(0 9) MC(0 17)) value:1]}");
  //             1         2         3
  //   0123456789012345678901234567890
  //            [       )
  //                   ^
  //                  del

  DIAG("Delete one byte just inside the right edge.");
  m.deleteLineBytes(tmc(0,16), ByteCount(1));
  m.selfCheck();

  EXPECT_EQ(allEntriesString(m),
    "{DocEntry[range:MCR(MC(0 9) MC(0 16)) value:1]}");
  //             1         2         3
  //   0123456789012345678901234567890
  //            [      )
  //                   ^
  //                  del

  DIAG("Delete one byte just outside the right edge (no effect).");
  m.deleteLineBytes(tmc(0,16), ByteCount(1));
  m.selfCheck();

  EXPECT_EQ(allEntriesString(m),
    "{DocEntry[range:MCR(MC(0 9) MC(0 16)) value:1]}");
  //             1         2         3
  //   0123456789012345678901234567890
  //            [      )
  //           ^^
  //          del

  DIAG("Delete two bytes straddling the left edge.");
  m.deleteLineBytes(tmc(0,8), ByteCount(2));
  m.selfCheck();

  EXPECT_EQ(allEntriesString(m),
    "{DocEntry[range:MCR(MC(0 8) MC(0 14)) value:1]}");
  //             1         2         3
  //   0123456789012345678901234567890
  //           [     )
  //                ^^
  //               del

  DIAG("Delete two bytes straddling the right edge.");
  m.deleteLineBytes(tmc(0,13), ByteCount(2));
  m.selfCheck();

  EXPECT_EQ(allEntriesString(m),
    "{DocEntry[range:MCR(MC(0 8) MC(0 13)) value:1]}");
  //             1         2         3
  //   0123456789012345678901234567890
  //           [    )
  //           ^^^^^
  //            del

  DIAG("Delete the exact range.");
  m.deleteLineBytes(tmc(0,8), ByteCount(5));
  m.selfCheck();

  EXPECT_EQ(allEntriesString(m),
    "{DocEntry[range:MCR(MC(0 8) MC(0 8)) value:1]}");
  //             1         2         3
  //   0123456789012345678901234567890
  //           H
  //          ^^
  //          del

  DIAG("Delete two bytes straddling the empty range.");
  m.deleteLineBytes(tmc(0,7), ByteCount(2));
  m.selfCheck();

  EXPECT_EQ(allEntriesString(m),
    "{DocEntry[range:MCR(MC(0 7) MC(0 7)) value:1]}");
  //             1         2         3
  //   0123456789012345678901234567890
  //          H
  //   ^^^^^^^
  //     del

  DIAG("Delete all preceding bytes.");
  m.deleteLineBytes(tmc(0,0), ByteCount(7));
  m.selfCheck();

  EXPECT_EQ(allEntriesString(m),
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
  MapPair m(PositiveLineCount(7) /*numLines*/);

  m.insertEntry({tmcr(2,24, 4,1), 3});
  m.selfCheck();
  EXPECT_EQ(allEntriesString(m),
    "{DocEntry[range:MCR(MC(2 24) MC(4 1)) value:3]}");

  //             1         2         3
  //   0123456789012345678901234567890
  // 0
  // 1
  // 2                         [
  // 3
  // 4  )         <-- del two lines starting here

  m.deleteLines(LineIndex(4), LineCount(2));
  m.selfCheck();
  EXPECT_EQ(allEntriesString(m),
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
  TEST_FUNC();

  MapPair m(PositiveLineCount(4) /*numLines*/);

  m.insertEntry({tmcr(0,21, 3,0), 3});
  m.selfCheck();
  EXPECT_EQ(allEntriesString(m),
    "{DocEntry[range:MCR(MC(0 21) MC(3 0)) value:3]}");
  //             1         2         3
  //   0123456789012345678901234567890
  // 0                      [
  // 1
  // 2
  // 3 )

  m.deleteLines(LineIndex(0), LineCount(2));
  m.selfCheck();
  EXPECT_EQ(allEntriesString(m),
    "{DocEntry[range:MCR(MC(0 0) MC(1 0)) value:3]}");
  //             1         2         3
  //   0123456789012345678901234567890
  // 0 [
  // 1 )
}


LineIndex randomLine()
{
  return LineIndex(sm_random(20));
}

ByteIndex randomByteIndex()
{
  return ByteIndex(sm_random(40));
}

ByteCount randomByteCount()
{
  return ByteCount(sm_random(40));
}


// Append lines to ensure `line` is a valid index.
void ensureValidLineIndex(MapPair &m, LineIndex line)
{
  LineIndex maxLine = LineIndex(m.getNumLines().pred());
  if (line > maxLine) {
    m.insertLines(maxLine.succ(), PositiveLineCount(line - maxLine));
  }
  xassert(line < m.getNumLines());
}


// Insert a random coordinate-map entry.
void randomInsertEntry(MapPair &m)
{
  int spanID = m.numEntries() + 1;

  LineIndex startLine = randomLine();
  ByteIndex startByteIndex = randomByteIndex();

  LineIndex endLine;
  ByteIndex endByteIndex;

  if (sm_random(7) == 0) {
    // Multi-line (rare).
    endLine = startLine + LineDifference(1 + sm_random(2));
    endByteIndex = randomByteIndex();
  }
  else {
    // Single-line (common).
    endLine = startLine;
    endByteIndex = randomByteIndex() + startByteIndex;
  }

  // Append lines if needed to allow the entry to be added.
  ensureValidLineIndex(m, endLine);

  m.insertEntry(MapPair::DocEntry(
    TextMCoordRange(
      TextMCoord(startLine, startByteIndex),
      TextMCoord(endLine, endByteIndex)
    ),
    spanID));

  m.selfCheck();
}


void randomInsertMultipleEntries(MapPair &m, int n)
{
  while (n--) {
    randomInsertEntry(m);
  }
}


void randomEdit(MapPair &m)
{
  RandomChoice c(803);

  if (c.check(2)) {
    // Insert a new span after (most likely) having done some edits.
    randomInsertEntry(m);
  }

  else if (c.check(1)) {
    m.clearEverything(PositiveLineCount(1));
    m.selfCheck();
    randomInsertMultipleEntries(m, 10);
  }

  else if (c.check(200)) {
    LineIndex line = randomLine();
    LineCount count(sm_random(3));
    ensureValidLineIndex(m, line.predClamped());
    m.insertLines(line, count);
  }

  else if (c.check(200)) {
    LineIndex line = randomLine();
    LineCount count(sm_random(3));
    ensureValidLineIndex(m, (line+count).predClamped());
    m.deleteLines(line, count);
  }

  else if (c.check(200)) {
    LineIndex line = randomLine();
    ensureValidLineIndex(m, line);
    m.insertLineBytes(TextMCoord(line, randomByteIndex()), randomByteCount());
  }

  else if (c.check(200)) {
    LineIndex line = randomLine();
    ensureValidLineIndex(m, line);
    m.deleteLineBytes(TextMCoord(line, randomByteIndex()), randomByteCount());
  }

  else {
    xfailure("impossible");
  }
}


void test_randomOps()
{
  TIMED_TEST_FUNC();

  // On my computer, with the defaults and no optimization, this test
  // takes ~0.4s.
  int const outerLimit =
    envRandomizedTestIters(3, "TMT_OUTER_LIMIT", 2);
  int const innerLimit =
    envRandomizedTestIters(50, "TMT_INNER_LIMIT", 2);

  for (int outer=0; outer < outerLimit; ++outer) {
    EXN_CONTEXT_EXPR(outer);

    MapPair m(PositiveLineCount(1) /*numLines*/);
    m.selfCheck();

    randomInsertMultipleEntries(m, 10);

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
  TEST_FUNC();

  MapPair m(PositiveLineCount(20) /*numLines*/);
  m.selfCheck();

  m.insertLineBytes(tmc(13,13), ByteCount(2));
  m.deleteLineBytes(tmc(13,31), ByteCount(21));
  m.insertLines(LineIndex(18), LineCount(1));
  m.deleteLines(LineIndex(10), LineCount(2));
}


// Issue with inserting right after the last range.
void test_insertAfterLast()
{
  TEST_FUNC();

  MapPair m(PositiveLineCount(2) /*numLines*/);

  m.insertEntry({tmcr(1,4, 1,42), 2});
  m.selfCheck();
  EXPECT_EQ(m.maxEntryLine(), 1);

  m.insertLines(LineIndex(2), LineCount(1));
  m.selfCheck();
  EXPECT_EQ(m.maxEntryLine(), 1);
}


// Clearing should ensure `maxEntryLine()==-1`.
void test_clear()
{
  TEST_FUNC();

  MapPair m(PositiveLineCount(2) /*numLines*/);
  m.insertEntry({tmcr(1,4, 1,42), 2});
  m.selfCheck();
  EXPECT_EQ(m.maxEntryLine(), 1);

  m.clearEverything(PositiveLineCount(2) /*numLines*/);
  m.selfCheck();
  EXPECT_EQ(m.maxEntryLine(), -1);
}


// Another one found by random testing.
void test_multilineDeletion3()
{
  TEST_FUNC();

  MapPair m(PositiveLineCount(5) /*numLines*/);

  m.insertEntry({tmcr(3,0, 4,8), 2});

  VPVAL(toGDValue(m));

  // The issue here is we have a multiline deletion that ends just
  // before the line containing the endpoint.  Consequently, what was
  // a multiline range has to be converted to a single-line range.
  m.deleteLines(LineIndex(3), LineCount(1));

  m.selfCheck();
}


// Found by random testing.
void test_multilineDeletion4()
{
  TEST_FUNC();

  MapPair m(PositiveLineCount(30) /*numLines*/);

  m.insertEntry({tmcr(19,11, 20,27), 3});

  VPVAL(toGDValue(m));

  // Multiline deletion that covers the entire span.
  m.deleteLines(LineIndex(19), LineCount(2));

  VPVAL(toGDValue(m));
  m.selfCheck();
}


// The issue here is that an insertion creates a line with a span start
// greater than 99, which for a while was my crude sentinel in the test
// code.
void test_insertMakesLongLine()
{
  TEST_FUNC();

  MapPair m(PositiveLineCount(2) /*numLines*/);

  m.insertEntry({tmcr(0,94, 1,0), 3});
  m.selfCheck();

  m.insertLineBytes(tmc(0,11), ByteCount(33));
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
  TEST_FUNC();

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


void test_adjustForDocument()
{
  TEST_FUNC();

  MapPair m(PositiveLineCount(10) /*numLines*/);

  // Simple case of reducing the end coordinate within a line.
  m.insertEntry({tmcr(1,1, 1,42), 1});

  // Reduce both coordinates
  m.insertEntry({tmcr(2,20, 2,42), 2});

  // Span where the first coordinate gets reduced.
  m.insertEntry({tmcr(1,40, 2,2), 3});

  // Single-line span near EOF that is not affected.
  m.insertEntry({tmcr(4,1, 4,2), 4});

  // Single-line span beyond EOF.
  m.insertEntry({tmcr(5,1, 5,42), 5});

  // Multiline span where only the end gets moved.
  m.insertEntry({tmcr(4,1, 6,42), 6});


  EXPECT_EQ(allEntriesString(m), "{\n"
  "  DocEntry[range:MCR(MC(1 1) MC(1 42)) value:1]\n"
  "  DocEntry[range:MCR(MC(1 40) MC(2 2)) value:3]\n"
  "  DocEntry[range:MCR(MC(2 20) MC(2 42)) value:2]\n"
  "  DocEntry[range:MCR(MC(4 1) MC(4 2)) value:4]\n"
  "  DocEntry[range:MCR(MC(4 1) MC(6 42)) value:6]\n"
  "  DocEntry[range:MCR(MC(5 1) MC(5 42)) value:5]\n"
  "}");
  m.selfCheck();

  TextDocumentCore doc;
  doc.replaceWholeFile(stringToVectorOfUChar(
    "zero\n"
    "one\n"
    "two\n"
    "three\n"
    "four\n"));

  m.adjustForDocument(doc);
  EXPECT_EQ(allEntriesString(m), "{\n"
  "  DocEntry[range:MCR(MC(1 1) MC(1 3)) value:1]\n"
  "  DocEntry[range:MCR(MC(1 3) MC(2 2)) value:3]\n"
  "  DocEntry[range:MCR(MC(2 3) MC(2 3)) value:2]\n"
  "  DocEntry[range:MCR(MC(4 1) MC(4 2)) value:4]\n"
  "  DocEntry[range:MCR(MC(4 1) MC(5 0)) value:6]\n"
  "  DocEntry[range:MCR(MC(5 0) MC(5 0)) value:5]\n"
  "}");
  m.selfCheck();
}


void randomDocInsertions(TextDocumentCore &doc, int n)
{
  for (LineIndex i(0); i < LineCount(n); ++i) {
    doc.insertLine(i);
    doc.insertText(TextMCoord(i,ByteIndex(0)),
      "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx", randomByteCount());
  }
}


void test_adjustForDocumentRandomized()
{
  TIMED_TEST_FUNC();

  // With defaults, takes ~0.4s.
  int const iters = envRandomizedTestIters(30, "AFDR_ITERS");

  for (int i=0; i < iters; ++i) {
    // Random diagnostics.
    MapPair m(PositiveLineCount(1) /*numLines*/);
    randomInsertMultipleEntries(m, 10);

    // Random document.
    TextDocumentCore doc;
    randomDocInsertions(doc, 10);

    // Confine diagnostics to document.
    m.adjustForDocument(doc);
    m.selfCheck();
  }
}


// This reproduces a problem found during randomized testing of
// `td-obs-recorder`.
//
// This test case is what motivated me to add `m_numLines` to
// `TextMCoordMap` and `TextDocumentDiagnostics`, since I didn't see
// another way to make this work the way I want.
void test_deleteNearEnd()
{
  TEST_FUNC();

  MapPair m(PositiveLineCount(3) /*numLines*/);

  // This is supposed to represent a diagnostic that goes right to the
  // end of a file that has 3 lines total.
  m.insertEntry({tmcr(1,0, 2,19), 8740});        // Start at BOL.
  m.insertEntry({tmcr(1,5, 2,19), 8745});        // Start a little after.
  m.selfCheck();

  // We then delete a span that has the effect of removing one line, so
  // the diagnostic should be adjusted to end on line 1, not 2.
  //doc.deleteTextRange(TextMCoordRange(tmc(1,8), tmc(2,6)));
  m.deleteLineBytes(tmc(1,8), ByteCount(4));  // End of first line.
  m.deleteLineBytes(tmc(2,0), ByteCount(6));  // Start of next line.
  m.deleteLineBytes(tmc(2,0), ByteCount(13)); // Remainder of next line (for splice).
  m.deleteLines(LineIndex(2), LineCount(1));  // Remove the next line.
  m.insertLineBytes(tmc(1,8), ByteCount(13)); // Put the spliced part back.

  // Now, the endpoint of the adjusted diagnostic should be on line 1.
  m.selfCheck();
  EXPECT_EQ(m.getNumLines(), 2);

  EXPECT_EQ_GDV(m.getAllEntries(), fromGDVN(R"(
    {DocEntry[range:MCR(MC(1 0) MC(1 0)) value:8740]
     DocEntry[range:MCR(MC(1 5) MC(1 5)) value:8745]}
  )"));
}


// Ad-hoc reproduction of problematic sequences.
void test_repro()
{
  TEST_FUNC();

  MapPair m(PositiveLineCount(2) /*numLines*/);

  return;
}


CLOSE_ANONYMOUS_NAMESPACE


// Called from unit-tests.cc.
void test_textmcoord_map(CmdlineArgsSpan args)
{
  TimedTestCase timer("entire test_textmcoord_map");

  #define RUN_TEST(funcname) { \
    EXN_CONTEXT(#funcname);    \
    funcname();                \
  }

  RUN_TEST(test_repro);
  RUN_TEST(test_DocEntry_contains_orAtCollapsed);
  RUN_TEST(test_lineData);
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
  RUN_TEST(test_parseLineEntry);
  RUN_TEST(test_adjustForDocument);
  RUN_TEST(test_deleteNearEnd);

  // Randomized tests at the end.
  RUN_TEST(test_randomOps);
  RUN_TEST(test_adjustForDocumentRandomized);

  #undef RUN_TEST
}


// EOF
