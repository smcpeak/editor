// td-diagnostics-test.cc
// Tests for `td-diagnostics` module.

#include "smbase/gdvalue-optional-fwd.h"         // gdv::GDValue(std::optional)

#include "unit-tests.h"                          // decl for my entry point

#include "td-diagnostics.h"                      // module under test

#include "named-td.h"                            // NamedTextDocument

#include "smbase/gdv-ordered-map.h"              // gdv::GDVOrderedMap (for TEST_CASE_EXPRS)
#include "smbase/gdvalue-optional.h"             // gdv::GDValue(std::optional)
#include "smbase/gdvalue.h"                      // gdv::toGDValue
#include "smbase/sm-macros.h"                    // OPEN_ANONYMOUS_NAMESPACE
#include "smbase/sm-test.h"                      // EXPECT_EQ[_GDVSER], TEST_CASE_EXPRS

using namespace gdv;


OPEN_ANONYMOUS_NAMESPACE


// If `s` has a value, construct `DEST(*s)` and wrap that in an
// `optional`.  Otherwise return `nullopt`.
//
// TODO: Move this into `smbase`.
template <typename DEST, typename SRC>
std::optional<DEST> makeOptFromOpt(std::optional<SRC> const &s)
{
  if (s.has_value()) {
    return std::optional<DEST>(*s);
  }
  else {
    return std::nullopt;
  }
}


void testOneContainsByteIndex(
  std::optional<int> startByteIndex,
  std::optional<int> endByteIndex,
  int testByteIndex,
  bool expect)
{
  EXN_CONTEXT_EXPR(toGDValue(startByteIndex));
  EXN_CONTEXT_EXPR(toGDValue(endByteIndex));
  EXN_CONTEXT_EXPR(testByteIndex);

  TDD_Diagnostic dummy("");
  TextDocumentDiagnostics::LineEntry entry(
    makeOptFromOpt<ByteIndex>(startByteIndex),
    makeOptFromOpt<ByteIndex>(endByteIndex),
    &dummy);

  EXPECT_EQ(entry.containsByteIndex(ByteIndex(testByteIndex)), expect);
}


void test_TDD_LineEntry_containsByteIndex()
{
  testOneContainsByteIndex({}, {}, 0, true);
  testOneContainsByteIndex({}, {}, 1, true);
  testOneContainsByteIndex({}, {}, 999, true);

  testOneContainsByteIndex(5, {}, 1, false);
  testOneContainsByteIndex(5, {}, 4, false);
  testOneContainsByteIndex(5, {}, 5, true);
  testOneContainsByteIndex(5, {}, 6, true);

  testOneContainsByteIndex({}, 10, 5, true);
  testOneContainsByteIndex({}, 10, 9, true);
  testOneContainsByteIndex({}, 10, 10, false);
  testOneContainsByteIndex({}, 10, 11, false);

  testOneContainsByteIndex(5, 10, 4, false);
  testOneContainsByteIndex(5, 10, 5, true);
  testOneContainsByteIndex(5, 10, 9, true);
  testOneContainsByteIndex(5, 10, 10, false);

  // Special case for collapsed ranges.
  testOneContainsByteIndex(7, 7, 6, false);
  testOneContainsByteIndex(7, 7, 7, true);
  testOneContainsByteIndex(7, 7, 8, false);
}


void testOneGetDiagnosticsAt(
  TextDocumentDiagnostics const &tdd,
  int line,
  int byteIndex,
  char const *expect)
{
  EXN_CONTEXT_EXPR(line);
  EXN_CONTEXT_EXPR(byteIndex);

  RCSerf<TDD_Diagnostic const> actual =
    tdd.getDiagnosticAt(TextMCoord(LineIndex(line), ByteIndex(byteIndex)));

  EXPECT_EQ(actual==nullptr, expect==nullptr);

  if (expect) {
    EXPECT_EQ(actual->m_message, expect);
  }
}


void testOneAdjacentDiagnostic(
  TextDocumentDiagnostics const &tdd,
  bool next,
  int startLine,
  int startByteIndex,
  int expectLine,
  int expectByteIndex)
{
  TEST_CASE_EXPRS("testOneAdjacentDiagnostic",
    next, startLine, startByteIndex);

  std::optional<TextMCoord> actual = tdd.getAdjacentDiagnosticLocation(
    next, TextMCoord(LineIndex(startLine), ByteIndex(startByteIndex)));

  if (expectLine == -1) {
    xassert(!actual.has_value());
  }
  else {
    xassert(actual.has_value());

    EXPECT_EQ(actual->m_line.get(), expectLine);
    EXPECT_EQ(actual->m_byteIndex, expectByteIndex);
  }
}


void testOneNextDiagnostic(
  TextDocumentDiagnostics const &tdd,
  int startLine,
  int startByteIndex,
  int expectLine,
  int expectByteIndex)
{
  testOneAdjacentDiagnostic(tdd, true /*next*/,
    startLine, startByteIndex,
    expectLine, expectByteIndex);
}


void testOnePreviousDiagnostic(
  TextDocumentDiagnostics const &tdd,
  int startLine,
  int startByteIndex,
  int expectLine,
  int expectByteIndex)
{
  testOneAdjacentDiagnostic(tdd, false /*next*/,
    startLine, startByteIndex,
    expectLine, expectByteIndex);
}


// Combination of diagnostics and updater for convenient testing, in
// particular combining their `selfCheck`s.
//
// This could potentially be moved into the main `td-diagnostics`
// module, but for the moment I don't need to use it anywhere but here.
class TextDocumentDiagnosticsAndUpdater :
  public TextDocumentDiagnostics,
  public TextDocumentDiagnosticsUpdater {
public:      // methods
  explicit TextDocumentDiagnosticsAndUpdater(
    TD_VersionNumber originVersion,
    NamedTextDocument *document)
    : TextDocumentDiagnostics(originVersion, document->numLines()),
      TextDocumentDiagnosticsUpdater(this, document)
  {}

  void selfCheck()
  {
    TextDocumentDiagnostics::selfCheck();
    TextDocumentDiagnosticsUpdater::selfCheck();
  }
};


static TextMCoordRange tmcr(int sl, int sb, int el, int eb)
{
  return TextMCoordRange(
    TextMCoord(LineIndex(sl), ByteIndex(sb)),
    TextMCoord(LineIndex(el), ByteIndex(eb)));
}


// This also tests next/previous diagnostic navigation.
void test_TDD_getDiagnosticAt()
{
  TEST_CASE("test_TDD_getDiagnosticAt");

  NamedTextDocument doc;

  //                          1
  //                01234567890123456789
  doc.appendString("   [1]     [2 2]    \n");  // 0
  doc.appendString("                    \n");  // 1
  doc.appendString("  [3 [4 [5] 4] 3]   \n");  // 2
  doc.appendString("  [6   [7   6]  7]  \n");  // 3
  doc.appendString("                    \n");  // 4
                           // ^ 9
  doc.appendString("      [10  [11]     \n");  // 5
  doc.appendString("   [12]        10]  \n");  // 6
  doc.appendString("   [13  13]  14]    \n");  // 7
  doc.appendString("   [15  [16  16]    \n");  // 8
  EXPECT_EQ(doc.numLines(), 10);  // The final line-without-NL counts.
  doc.selfCheck();

  TextDocumentDiagnosticsAndUpdater tdd(TD_VersionNumber(1), &doc);
  tdd.selfCheck();
  EXPECT_EQ(tdd.maxDiagnosticLine(), -1);

  tdd.insertDiagnostic(tmcr(0,3, 0,6), TDD_Diagnostic("1"));
  tdd.selfCheck();
  tdd.insertDiagnostic(tmcr(0,11, 0,16), TDD_Diagnostic("2"));
  tdd.selfCheck();
  tdd.insertDiagnostic(tmcr(2,2, 2,17), TDD_Diagnostic("3"));
  tdd.selfCheck();
  tdd.insertDiagnostic(tmcr(2,5, 2,14), TDD_Diagnostic("4"));
  tdd.selfCheck();
  tdd.insertDiagnostic(tmcr(2,8, 2,11), TDD_Diagnostic("5"));
  tdd.insertDiagnostic(tmcr(3,2, 3,14), TDD_Diagnostic("6"));
  tdd.insertDiagnostic(tmcr(3,7, 3,18), TDD_Diagnostic("7"));
  EXPECT_EQ(tdd.maxDiagnosticLine(), 3);
  // I skipped "8" I guess.
  tdd.insertDiagnostic(tmcr(4,10, 4,10), TDD_Diagnostic("9"));
  tdd.insertDiagnostic(tmcr(5,6, 6,18), TDD_Diagnostic("10"));
  EXPECT_EQ(tdd.maxDiagnosticLine(), 6);
  tdd.insertDiagnostic(tmcr(5,11, 5,15), TDD_Diagnostic("11"));
  tdd.insertDiagnostic(tmcr(6,3, 6,7), TDD_Diagnostic("12"));
  tdd.insertDiagnostic(tmcr(7,3, 7,11), TDD_Diagnostic("13"));
  tdd.insertDiagnostic(tmcr(7,3, 7,16), TDD_Diagnostic("14"));
  tdd.insertDiagnostic(tmcr(8,3, 8,16), TDD_Diagnostic("15"));
  tdd.insertDiagnostic(tmcr(8,8, 8,16), TDD_Diagnostic("16"));
  EXPECT_EQ(tdd.maxDiagnosticLine(), 8);
  tdd.selfCheck();

  // Check copying and comparison.
  TextDocumentDiagnostics diagnosticsCopy(tdd);
  xassert(diagnosticsCopy == tdd);

  // This doesn't work?  GCC bug?  Clang accepts.
  //EXPECT_EQ_GDVSER(diagnosticsCopy, tdd);
  EXPECT_EQ_GDV(diagnosticsCopy, tdd);

  testOneGetDiagnosticsAt(tdd, 0,0, nullptr);
  testOneGetDiagnosticsAt(tdd, 0,2, nullptr);
  testOneGetDiagnosticsAt(tdd, 0,3, "1");
  testOneGetDiagnosticsAt(tdd, 0,4, "1");
  testOneGetDiagnosticsAt(tdd, 0,5, "1");
  testOneGetDiagnosticsAt(tdd, 0,6, nullptr);
  testOneGetDiagnosticsAt(tdd, 0,10, nullptr);
  testOneGetDiagnosticsAt(tdd, 0,11, "2");
  testOneGetDiagnosticsAt(tdd, 0,15, "2");
  testOneGetDiagnosticsAt(tdd, 0,16, nullptr);

  testOneGetDiagnosticsAt(tdd, 1,3, nullptr);

  testOneGetDiagnosticsAt(tdd, 2,1, nullptr);
  testOneGetDiagnosticsAt(tdd, 2,2, "3");
  testOneGetDiagnosticsAt(tdd, 2,4, "3");
  testOneGetDiagnosticsAt(tdd, 2,5, "4");
  testOneGetDiagnosticsAt(tdd, 2,7, "4");
  testOneGetDiagnosticsAt(tdd, 2,8, "5");
  testOneGetDiagnosticsAt(tdd, 2,10, "5");
  testOneGetDiagnosticsAt(tdd, 2,11, "4");
  testOneGetDiagnosticsAt(tdd, 2,13, "4");
  testOneGetDiagnosticsAt(tdd, 2,14, "3");
  testOneGetDiagnosticsAt(tdd, 2,16, "3");
  testOneGetDiagnosticsAt(tdd, 2,17, nullptr);

  testOneGetDiagnosticsAt(tdd, 3,1, nullptr);
  testOneGetDiagnosticsAt(tdd, 3,2, "6");
  testOneGetDiagnosticsAt(tdd, 3,6, "6");
  testOneGetDiagnosticsAt(tdd, 3,7, "7");
  testOneGetDiagnosticsAt(tdd, 3,13, "7");
  testOneGetDiagnosticsAt(tdd, 3,14, "7");
  testOneGetDiagnosticsAt(tdd, 3,17, "7");
  testOneGetDiagnosticsAt(tdd, 3,18, nullptr);

  testOneGetDiagnosticsAt(tdd, 4,9, nullptr);
  testOneGetDiagnosticsAt(tdd, 4,10, "9");
  testOneGetDiagnosticsAt(tdd, 4,11, nullptr);

  testOneGetDiagnosticsAt(tdd, 5,5, nullptr);
  testOneGetDiagnosticsAt(tdd, 5,6, "10");
  testOneGetDiagnosticsAt(tdd, 5,10, "10");
  testOneGetDiagnosticsAt(tdd, 5,11, "11");
  testOneGetDiagnosticsAt(tdd, 5,14, "11");
  testOneGetDiagnosticsAt(tdd, 5,15, "10");
  testOneGetDiagnosticsAt(tdd, 6,2, "10");
  testOneGetDiagnosticsAt(tdd, 6,3, "12");
  testOneGetDiagnosticsAt(tdd, 6,6, "12");
  testOneGetDiagnosticsAt(tdd, 6,7, "10");
  testOneGetDiagnosticsAt(tdd, 6,17, "10");
  testOneGetDiagnosticsAt(tdd, 6,18, nullptr);

  testOneGetDiagnosticsAt(tdd, 7,2, nullptr);
  testOneGetDiagnosticsAt(tdd, 7,3, "13");
  testOneGetDiagnosticsAt(tdd, 7,10, "13");
  testOneGetDiagnosticsAt(tdd, 7,11, "14");
  testOneGetDiagnosticsAt(tdd, 7,15, "14");
  testOneGetDiagnosticsAt(tdd, 7,16, nullptr);

  testOneGetDiagnosticsAt(tdd, 8,2, nullptr);
  testOneGetDiagnosticsAt(tdd, 8,3, "15");
  testOneGetDiagnosticsAt(tdd, 8,7, "15");
  testOneGetDiagnosticsAt(tdd, 8,8, "16");
  testOneGetDiagnosticsAt(tdd, 8,15, "16");
  testOneGetDiagnosticsAt(tdd, 8,16, nullptr);


  testOneNextDiagnostic(tdd, 0,0, 0,3);
  testOneNextDiagnostic(tdd, 0,1, 0,3);
  testOneNextDiagnostic(tdd, 0,2, 0,3);

  testOneNextDiagnostic(tdd, 0,3, 0,11);
  testOneNextDiagnostic(tdd, 0,4, 0,11);
  testOneNextDiagnostic(tdd, 0,10, 0,11);

  testOneNextDiagnostic(tdd, 0,11, 2,2);
  testOneNextDiagnostic(tdd, 1,0, 2,2);
  testOneNextDiagnostic(tdd, 2,0, 2,2);
  testOneNextDiagnostic(tdd, 2,1, 2,2);

  testOneNextDiagnostic(tdd, 2,2, 2,5);

  testOneNextDiagnostic(tdd, 2,5, 2,8);

  testOneNextDiagnostic(tdd, 2,8, 3,2);

  testOneNextDiagnostic(tdd, 3,2, 3,7);

  testOneNextDiagnostic(tdd, 3,7, 4,10);

  testOneNextDiagnostic(tdd, 4,10, 5,6);

  testOneNextDiagnostic(tdd, 5,6, 5,11);

  testOneNextDiagnostic(tdd, 5,11, 6,3);

  testOneNextDiagnostic(tdd, 6,3, 7,3);

  testOneNextDiagnostic(tdd, 7,3, 8,3);

  testOneNextDiagnostic(tdd, 8,3, 8,8);

  testOneNextDiagnostic(tdd, 8,8, -1,0);


  testOnePreviousDiagnostic(tdd, 0,0, -1,0);

  testOnePreviousDiagnostic(tdd, 8,20, 8,8);

  testOnePreviousDiagnostic(tdd, 5,2, 4,10);

  // This notification should clear the diagnostics.
  tdd.observeTotalChange(doc.getCore());
  EXPECT_EQ(tdd.empty(), true);
  EXPECT_EQ(tdd.size(), 0);
  EXPECT_EQ(tdd.maxDiagnosticLine(), -1);
  tdd.selfCheck();
  doc.selfCheck();

  // After the changes, the copy should no longer be equal.
  xassert(diagnosticsCopy != tdd);
}


// This reproduces a problem found during randomized testing of
// `td-obs-recorder`.
void test_deleteNearEnd()
{
  NamedTextDocument doc;
  doc.appendString("GGGGPPPPGgggg\n");
  doc.appendString("GGGBBBBZZZZZ\n");
  doc.appendString("zzzzZZZZZzzzzzZZZZZ");
  EXPECT_EQ(doc.numLines(), 3);
  doc.selfCheck();

  TextDocumentDiagnosticsAndUpdater tdd(doc.getVersionNumber(), &doc);
  tdd.selfCheck();

  // This diagnostic goes right to the end of the file.
  tdd.insertDiagnostic(tmcr(1,0, 2,19), TDD_Diagnostic("msg8740"));
  tdd.selfCheck();

  // We then delete a span that has the effect of removing one line, so
  // the diagnostic should be adjusted to end on line 1, not 2.
  doc.deleteTextRange(tmcr(1,8, 2,6));

  // In the buggy version, this would fail because the endpoint of the
  // adjusted diagnostic was still on line 2.
  tdd.selfCheck();

  // The actual result is we get a zero-length span at the start of the
  // second line.  This is questionable because the span originally went
  // to the end of the document, yet after deleting some text, it now
  // ends before the document end.  But, at least with the information I
  // currently track, this isn't easy to solve, and anyway should have
  // almost no practical effect.  The important thing at the moment is
  // simply that it preserves the invariant that the coordinates are
  // valid w.r.t. the document contents.
  EXPECT_EQ_GDV(tdd, fromGDVN(R"(
    {
      TDD_DocEntry[
        range: MCR(MC(1 0) MC(1 0))
        diagnostic: TDD_Diagnostic[message:"msg8740" related:[]]
      ]
    }
  )"));
}


CLOSE_ANONYMOUS_NAMESPACE


// Called from unit-tests.cc.
void test_td_diagnostics(CmdlineArgsSpan args)
{
  test_TDD_LineEntry_containsByteIndex();
  test_TDD_getDiagnosticAt();
  test_deleteNearEnd();
}


// EOF
