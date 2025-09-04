// td-core-test.cc
// Tests for 'td-core' module.

#include "unit-tests.h"                // decl for my entry point

#include "td-core.h"                   // module to test

// smbase
#include "smbase/autofile.h"           // AutoFILE
#include "smbase/exc.h"                // smbase::xmessage
#include "smbase/gdvalue.h"            // gdv::GDValue
#include "smbase/sm-file-util.h"       // SMFileUtil
#include "smbase/sm-macros.h"          // IGNORE_RESULT, OPEN_ANONYMOUS_NAMESPACE
#include "smbase/sm-test.h"            // DIAG, EXPECT_EQ[_GDV], op_eq
#include "smbase/string-util.h"        // vectorOfUCharToString

// libc++
#include <algorithm>                   // std::max

// libc
#include <assert.h>                    // assert
#include <stdlib.h>                    // system

using namespace gdv;
using namespace smbase;


OPEN_ANONYMOUS_NAMESPACE


// Self-check the 'tdc' and also verify that the iterator works for
// every line.
void fullSelfCheck(TextDocumentCore const &tdc)
{
  tdc.selfCheck();

  ArrayStack<char> text;
  FOR_EACH_LINE_INDEX_IN(i, tdc) {
    text.clear();
    tdc.getWholeLine(i, text /*INOUT*/);

    int offset = 0;
    TextDocumentCore::LineIterator iter(tdc, i);
    for (; iter.has(); iter.advByte()) {
      xassert(iter.byteOffset() == offset);
      xassert(iter.byteAt() == (unsigned char)text[offset]);
      offset++;
    }

    xassert(iter.byteOffset() == offset);
    xassert(offset == tdc.lineLengthBytes(i));
  }

  // Confirm we can make an iterator for out of bounds lines.
  {
    TextDocumentCore::LineIterator it(tdc, LineIndex(tdc.numLines()));
    xassert(!it.has());
  }
}


void testAtomicRead()
{
  // Write a file that spans several blocks.
  {
    AutoFILE fp("td-core.tmp", "wb");
    for (int i=0; i < 1000; i++) {
      fputs("                                       \n", fp);  // 40 bytes
    }
  }

  // Read it.
  TextDocumentCore core;
  core.replaceWholeFile(SMFileUtil().readFile("td-core.tmp"));
  xassert(core.numLines() == 1001);
  fullSelfCheck(core);

  remove("td-core.tmp");
}


// `col` here is a misnomer...
void insText(TextDocumentCore &tdc, int line, int col, char const *text)
{
  tdc.insertText(
    TextMCoord(LineIndex(line), ByteIndex(col)),
    text, strlenBC(text));
}

void insLine(TextDocumentCore &tdc, int line, int col, char const *text)
{
  tdc.insertLine(LineIndex(line));
  insText(tdc, line, col, text);
}

void appendLine(TextDocumentCore &tdc, char const *text)
{
  // There is always a line at the end without a newline terminator.  We
  // insert above it.
  int line = tdc.numLines().get() - 1;

  insLine(tdc, line, 0, text);
}


void checkSpaces(TextDocumentCore const &tdc,
  int line, int leading, int trailing)
{
  EXPECT_EQ(tdc.countLeadingSpacesTabs(LineIndex(line)), leading);
  EXPECT_EQ(tdc.countTrailingSpacesTabs(LineIndex(line)), trailing);
}


// Check that the version number of `tdc` is `vnum`.
#define CHECK_VER_SAME(tdc, vnum) \
  EXPECT_EQ(tdc.getVersionNumber(), (vnum))

// Check that the version number of `tdc` has been incremented since it
// was `vnum`.  Then update `vnum` to be the current version number.
#define CHECK_VER_DIFF(tdc, vnum)             \
  {                                           \
    xassert(tdc.getVersionNumber() > (vnum)); \
    (vnum) = tdc.getVersionNumber();          \
  }


TextMCoord tmc(LineIndex line, int byteIndex)
{
  return TextMCoord(line, ByteIndex(byteIndex));
}

TextMCoord tmc(int line, int byteIndex)
{
  return TextMCoord(LineIndex(line), ByteIndex(byteIndex));
}


void testVarious()
{
  TextDocumentCore tdc;
  TD_VersionNumber vnum = tdc.getVersionNumber();

  LineIndex const z(0);

  EXPECT_EQ(tdc.numLines(), 1);
  EXPECT_EQ(tdc.lineLengthBytes(z), 0);
  EXPECT_EQ(tdc.validCoord(tmc(0,0)), true);
  EXPECT_EQ(tdc.validCoord(tmc(0,1)), false);
  EXPECT_EQ(tdc.endCoord(), tmc(0,0));
  EXPECT_EQ(tdc.maxLineLengthBytes(), 0);
  EXPECT_EQ(tdc.numLinesExcludingFinalEmpty(), 0);
  EXPECT_EQ(tdc.getWholeFileString(), "");
  fullSelfCheck(tdc);

  CHECK_VER_SAME(tdc, vnum);
  insLine(tdc, 0,0, "one");
  CHECK_VER_DIFF(tdc, vnum);
  EXPECT_EQ(tdc.numLines(), 2);
  EXPECT_EQ(tdc.numLinesExcludingFinalEmpty(), 1);
  CHECK_VER_SAME(tdc, vnum);
  insLine(tdc, 1,0, "  two");
  CHECK_VER_DIFF(tdc, vnum);
  EXPECT_EQ(tdc.numLines(), 3);
  EXPECT_EQ(tdc.numLinesExcludingFinalEmpty(), 2);
  insLine(tdc, 2,0, "three   ");
  CHECK_VER_DIFF(tdc, vnum);
  insLine(tdc, 3,0, "    four    ");
  CHECK_VER_DIFF(tdc, vnum);
  insLine(tdc, 4,0, "     ");
  CHECK_VER_DIFF(tdc, vnum);
  tdc.insertLine(LineIndex(5));  // Uses NULL representation internally.
  CHECK_VER_DIFF(tdc, vnum);
  insText(tdc, 6,0, "      ");
  CHECK_VER_DIFF(tdc, vnum);
  EXPECT_EQ(tdc.getWholeFileString(),
    "one\n"
    "  two\n"
    "three   \n"
    "    four    \n"
    "     \n"
    "\n"
    "      ");

  EXPECT_EQ(tdc.numLines(), 7);
  EXPECT_EQ(tdc.numLinesExcludingFinalEmpty(), 7);
  EXPECT_EQ(tdc.lineLengthBytes(z), 3);
  EXPECT_EQ(tdc.lineLengthBytes(LineIndex(6)), 6);
  EXPECT_EQ(tdc.validCoord(tmc(z,0)), true);
  EXPECT_EQ(tdc.validCoord(tmc(z,1)), true);
  EXPECT_EQ(tdc.validCoord(tmc(LineIndex(6),6)), true);
  EXPECT_EQ(tdc.validCoord(tmc(LineIndex(6),7)), false);
  EXPECT_EQ(tdc.validCoord(tmc(LineIndex(7),0)), false);
  EXPECT_EQ(tdc.endCoord(), tmc(LineIndex(6),6));
  EXPECT_EQ(tdc.maxLineLengthBytes(), 12);
  fullSelfCheck(tdc);

  checkSpaces(tdc, 0, 0, 0);
  checkSpaces(tdc, 1, 2, 0);
  checkSpaces(tdc, 2, 0, 3);
  checkSpaces(tdc, 3, 4, 4);
  checkSpaces(tdc, 4, 5, 5);
  checkSpaces(tdc, 5, 0, 0);
  checkSpaces(tdc, 6, 6, 6);
  fullSelfCheck(tdc);

  for (int line=0; line <= 6; line++) {
    // Tweak 'line' so it is recent and then repeat the whitespace queries.
    TextMCoord tc(LineIndex(line), ByteIndex(0));
    char c = 'x';
    CHECK_VER_SAME(tdc, vnum);
    tdc.insertText(tc, &c, ByteCount(1));
    CHECK_VER_DIFF(tdc, vnum);
    tdc.deleteTextBytes(tc, ByteCount(1));
    CHECK_VER_DIFF(tdc, vnum);

    checkSpaces(tdc, 0, 0, 0);
    checkSpaces(tdc, 1, 2, 0);
    checkSpaces(tdc, 2, 0, 3);
    checkSpaces(tdc, 3, 4, 4);
    checkSpaces(tdc, 4, 5, 5);
    checkSpaces(tdc, 5, 0, 0);
    checkSpaces(tdc, 6, 6, 6);
    fullSelfCheck(tdc);
  }

  // This is far from a comprehensive test of observers, but I want to
  // at least test 'hasObserver'.
  TextDocumentObserver obs;
  xassert(!tdc.hasObserver(&obs));
  tdc.addObserver(&obs);
  xassert(tdc.hasObserver(&obs));
  tdc.removeObserver(&obs);
  xassert(!tdc.hasObserver(&obs));
  fullSelfCheck(tdc);

  // Test `deleteLine`.
  CHECK_VER_SAME(tdc, vnum);
  tdc.deleteLine(LineIndex(5));
  CHECK_VER_DIFF(tdc, vnum);
  EXPECT_EQ(tdc.numLines(), 6);
  EXPECT_EQ(doubleQuote(vectorOfUCharToString(tdc.getWholeFile())),
            doubleQuote("one\n"
                        "  two\n"
                        "three   \n"
                        "    four    \n"
                        "     \n"
                        "      "));
  fullSelfCheck(tdc);

  // Test conversion to `GDValue`.
  EXPECT_EQ_GDV(tdc, fromGDVN(R"(
    TextDocumentCore[
      version: 27
      lines: [
        "one"
        "  two"
        "three   "
        "    four    "
        "     "
        "      "
      ]
    ]
  )"));
  EXPECT_EQ_GDV(tdc.getAllLines(), fromGDVN(R"(
    [
      "one"
      "  two"
      "three   "
      "    four    "
      "     "
      "      "
    ]
  )"));
  EXPECT_EQ_GDV(tdc.dumpInternals(), fromGDVN(R"(
    TextDocumentCoreInternals[
      lines: [
        "one"
        "  two"
        "three   "
        "    four    "
        "     "
        ""]
      recentIndex: 5
      longestLengthSoFar: 13
      recentLine: "      "
      versionNumber: 27
      numObservers: 0
      iteratorCount: 0
    ]
  )"));
}


// I don't remember what this was meant to test...
void testReadTwice()
{
  for (int looper=0; looper<2; looper++) {
    // build a text file
    {
      AutoFILE fp("td-core.tmp", "w");

      for (int i=0; i<2; i++) {
        for (int j=0; j<53; j++) {
          for (int k=0; k<j; k++) {
            fputc('0' + (k%10), fp);
          }
          fputc('\n', fp);
        }
      }
    }

    {
      // Read it as a text document.
      TextDocumentCore doc;
      doc.replaceWholeFile(SMFileUtil().readFile("td-core.tmp"));

      // dump its repr
      //doc.dumpRepresentation();

      // write it out again
      SMFileUtil().writeFile("td-core.tmp2", doc.getWholeFile());

      if (verbose) {
        DIAG("\nbuffer mem usage stats:");
        doc.printMemStats();
      }
      fullSelfCheck(doc);
    }

    // make sure they're the same
    if (system("diff td-core.tmp td-core.tmp2") != 0) {
      xmessage("the files were different!\n");
    }

    // ok
    if (verbose) {
      IGNORE_RESULT(system("ls -l td-core.tmp"));
    }
    remove("td-core.tmp");
    remove("td-core.tmp2");
  }
}


// Read the td-core.cc source code, just as an example of a real file.
void testReadSourceCode()
{
  DIAG("reading td-core.cc ...");
  TextDocumentCore doc;
  doc.replaceWholeFile(SMFileUtil().readFile("td-core.cc"));
  if (verbose) {
    doc.printMemStats();
  }
  fullSelfCheck(doc);
}


void expectWalkCoordBytes(
  TextDocumentCore &tdc,
  TextMCoord const &tc,
  int distance,
  TextMCoord const &expect)
{
  TextMCoord actual(tc);
  bool res = tdc.walkCoordBytes(actual, ByteDifference(distance));
  EXPECT_EQ(res, true);
  EXPECT_EQ(actual, expect);
}

void expectWalkCoordBytes_false(
  TextDocumentCore &tdc,
  TextMCoord const &tc,
  int distance)
{
  TextMCoord actual(tc);
  bool res = tdc.walkCoordBytes(actual, ByteDifference(distance));
  EXPECT_EQ(res, false);
}

void expectInvalidCoord(
  TextDocumentCore &tdc,
  TextMCoord const &tc)
{
  bool res = tdc.validCoord(tc);
  EXPECT_EQ(res, false);
}

void testWalkCoordBytes()
{
  LineIndex const li0(0);
  LineIndex const li1(1);
  LineIndex const li2(2);
  LineIndex const li3(3);
  LineIndex const li4(4);

  TextDocumentCore tdc;
  tdc.insertLine(li0);
  tdc.insertString(tmc(li0,0), "one");
  tdc.insertLine(li1);
  tdc.insertLine(li2);
  tdc.insertString(tmc(li2,0), "three");

  expectWalkCoordBytes_false(tdc, tmc(li0,0), -1);
  expectWalkCoordBytes      (tdc, tmc(li0,0),  0, tmc(li0,0));
  expectWalkCoordBytes      (tdc, tmc(li0,0),  1, tmc(li0,1));
  expectWalkCoordBytes      (tdc, tmc(li0,0),  2, tmc(li0,2));
  expectWalkCoordBytes      (tdc, tmc(li0,0),  3, tmc(li0,3));
  expectWalkCoordBytes      (tdc, tmc(li0,0),  4, tmc(li1,0));
  expectWalkCoordBytes      (tdc, tmc(li0,0),  5, tmc(li2,0));
  expectWalkCoordBytes      (tdc, tmc(li0,0),  6, tmc(li2,1));
  expectWalkCoordBytes      (tdc, tmc(li0,0),  7, tmc(li2,2));
  expectWalkCoordBytes      (tdc, tmc(li0,0),  8, tmc(li2,3));
  expectWalkCoordBytes      (tdc, tmc(li0,0),  9, tmc(li2,4));
  expectWalkCoordBytes      (tdc, tmc(li0,0), 10, tmc(li2,5));
  expectWalkCoordBytes      (tdc, tmc(li0,0), 11, tmc(li3,0));
  expectWalkCoordBytes_false(tdc, tmc(li0,0), 12);

  expectWalkCoordBytes_false(tdc, tmc(li0,1), -2);
  expectWalkCoordBytes      (tdc, tmc(li0,1), -1, tmc(li0,0));
  expectWalkCoordBytes      (tdc, tmc(li0,1),  0, tmc(li0,1));
  expectWalkCoordBytes      (tdc, tmc(li0,1),  1, tmc(li0,2));
  expectWalkCoordBytes      (tdc, tmc(li0,1), 10, tmc(li3,0));
  expectWalkCoordBytes_false(tdc, tmc(li0,1), 11);

  expectWalkCoordBytes_false(tdc, tmc(li0,3), -4);
  expectWalkCoordBytes      (tdc, tmc(li0,3), -1, tmc(li0,2));
  expectWalkCoordBytes      (tdc, tmc(li0,3),  0, tmc(li0,3));
  expectWalkCoordBytes      (tdc, tmc(li0,3),  1, tmc(li1,0));
  expectWalkCoordBytes      (tdc, tmc(li0,3),  8, tmc(li3,0));
  expectWalkCoordBytes_false(tdc, tmc(li0,3),  9);

  expectWalkCoordBytes_false(tdc, tmc(li2,4),-10);
  expectWalkCoordBytes      (tdc, tmc(li2,4), -9, tmc(li0,0));
  expectWalkCoordBytes      (tdc, tmc(li2,4), -1, tmc(li2,3));
  expectWalkCoordBytes      (tdc, tmc(li2,4),  0, tmc(li2,4));
  expectWalkCoordBytes      (tdc, tmc(li2,4),  1, tmc(li2,5));
  expectWalkCoordBytes      (tdc, tmc(li2,4),  2, tmc(li3,0));
  expectWalkCoordBytes_false(tdc, tmc(li2,4),  3);
  expectWalkCoordBytes_false(tdc, tmc(li2,4), 99);

  expectWalkCoordBytes_false(tdc, tmc(li2,5),-11);
  expectWalkCoordBytes      (tdc, tmc(li2,5),-10, tmc(li0,0));
  expectWalkCoordBytes      (tdc, tmc(li2,5), -1, tmc(li2,4));
  expectWalkCoordBytes      (tdc, tmc(li2,5),  0, tmc(li2,5));
  expectWalkCoordBytes      (tdc, tmc(li2,5),  1, tmc(li3,0));
  expectWalkCoordBytes_false(tdc, tmc(li2,5),  2);

  expectWalkCoordBytes_false(tdc, tmc(li3,0),-12);
  expectWalkCoordBytes      (tdc, tmc(li3,0),-11, tmc(li0,0));
  expectWalkCoordBytes      (tdc, tmc(li3,0), -1, tmc(li2,5));
  expectWalkCoordBytes      (tdc, tmc(li3,0),  0, tmc(li3,0));
  expectWalkCoordBytes_false(tdc, tmc(li3,0),  1);

  expectInvalidCoord(tdc, tmc(li0,4));
  expectInvalidCoord(tdc, tmc(li1,1));
  expectInvalidCoord(tdc, tmc(li2,6));
  expectInvalidCoord(tdc, tmc(li3,1));
  expectInvalidCoord(tdc, tmc(li4,0));
}


void testOneAdjustMCoord_adj(
  TextDocumentCore &tdc, int il, int ib, int ol, int ob)
{
  // These tests were originally written when `LineIndex` did not exist,
  // so the non-negative requirement was imposed by `adjustMCoord`.  Now
  // that `LineIndex` requires a non-negative value, I'll do this to
  // effectively disable those tests, at least for now.
  //
  // Same for `ByteIndex`.
  if (il < 0 || ib < 0) {
    return;
  }

  TEST_CASE_EXPRS("testOneAdjustMCoord_adj", il, ib);

  TextMCoord tc{LineIndex(il), ByteIndex(ib)};
  xassert(tdc.adjustMCoord(tc));

  EXPECT_EQ(tc.m_line, LineIndex(ol));
  EXPECT_EQ(tc.m_byteIndex, ob);
}


void testOneAdjustMCoord_noAdj(
  TextDocumentCore &tdc, int il, int ib)
{
  TextMCoord tc{LineIndex(il), ByteIndex(ib)};
  xassert(!tdc.adjustMCoord(tc));

  EXPECT_EQ(tc.m_line, LineIndex(il));
  EXPECT_EQ(tc.m_byteIndex, ib);
}


void testOneAdjustMCoordRange_adj(
  TextDocumentCore &tdc,
  int isl, int isb, int iel, int ieb,
  int osl, int osb, int oel, int oeb)
{
  // Similar to above.
  if (isl < 0 || isb < 0 || iel < 0 || ieb < 0) {
    return;
  }

  TEST_CASE_EXPRS("testOneAdjustMCoordRange_adj",
    isl, isb, iel, ieb);

  TextMCoordRange range{
    {LineIndex(isl), ByteIndex(isb)},
    {LineIndex(iel), ByteIndex(ieb)}};
  xassert(tdc.adjustMCoordRange(range));

  EXPECT_EQ(range.m_start.m_line, LineIndex(osl));
  EXPECT_EQ(range.m_start.m_byteIndex, osb);
  EXPECT_EQ(range.m_end.m_line, LineIndex(oel));
  EXPECT_EQ(range.m_end.m_byteIndex, oeb);
}


void testOneAdjustMCoordRange_noAdj(
  TextDocumentCore &tdc,
  int isl, int isb, int iel, int ieb)
{
  TextMCoordRange range{
    {LineIndex(isl), ByteIndex(isb)},
    {LineIndex(iel), ByteIndex(ieb)}};
  xassert(!tdc.adjustMCoordRange(range));

  EXPECT_EQ(range.m_start.m_line, LineIndex(isl));
  EXPECT_EQ(range.m_start.m_byteIndex, isb);
  EXPECT_EQ(range.m_end.m_line, LineIndex(iel));
  EXPECT_EQ(range.m_end.m_byteIndex, ieb);
}


void test_adjustMCoord()
{
  TEST_CASE("test_adjustMCoord");

  TextDocumentCore tdc;
  appendLine(tdc, "zero");
  appendLine(tdc, "one");
  appendLine(tdc, "two");
  appendLine(tdc, "three");

  // Single coordinate.

  testOneAdjustMCoord_adj(tdc, -2,0, 0,0);

  testOneAdjustMCoord_adj(tdc, -1,0, 0,0);
  testOneAdjustMCoord_adj(tdc, -1,-1, 0,0);

  testOneAdjustMCoord_adj(tdc, 0,-2, 0,0);
  testOneAdjustMCoord_adj(tdc, 0,-1, 0,0);
  testOneAdjustMCoord_noAdj(tdc, 0,0);
  testOneAdjustMCoord_noAdj(tdc, 0,4);
  testOneAdjustMCoord_adj(tdc, 0,5, 0,4);
  testOneAdjustMCoord_adj(tdc, 0,6, 0,4);

  testOneAdjustMCoord_adj(tdc, 1,-1, 1,0);
  testOneAdjustMCoord_noAdj(tdc, 1,0);
  testOneAdjustMCoord_noAdj(tdc, 1,3);
  testOneAdjustMCoord_adj(tdc, 1,4, 1,3);

  testOneAdjustMCoord_adj(tdc, 2,-1, 2,0);
  testOneAdjustMCoord_noAdj(tdc, 2,0);
  testOneAdjustMCoord_noAdj(tdc, 2,3);
  testOneAdjustMCoord_adj(tdc, 2,4, 2,3);

  testOneAdjustMCoord_adj(tdc, 3,-1, 3,0);
  testOneAdjustMCoord_noAdj(tdc, 3,0);
  testOneAdjustMCoord_noAdj(tdc, 3,5);
  testOneAdjustMCoord_adj(tdc, 3,6, 3,5);

  testOneAdjustMCoord_adj(tdc, 4,-1, 4,0);
  testOneAdjustMCoord_noAdj(tdc, 4,0);
  testOneAdjustMCoord_adj(tdc, 4,1, 4,0);

  testOneAdjustMCoord_adj(tdc, 5,-1, 4,0);
  testOneAdjustMCoord_adj(tdc, 5,0, 4,0);
  testOneAdjustMCoord_adj(tdc, 5,1, 4,0);

  testOneAdjustMCoord_adj(tdc, 6,0, 4,0);

  // Range.

  testOneAdjustMCoordRange_adj(tdc, -1,0, 1,0, 0,0, 1,0);
  testOneAdjustMCoordRange_adj(tdc, -1,0, -1,0, 0,0, 0,0);

  testOneAdjustMCoordRange_noAdj(tdc, 0,0, 0,0);
  testOneAdjustMCoordRange_noAdj(tdc, 0,0, 1,0);
  testOneAdjustMCoordRange_noAdj(tdc, 0,0, 4,0);

  testOneAdjustMCoordRange_adj(tdc, -1,0, 4,1, 0,0, 4,0);

  testOneAdjustMCoordRange_adj(tdc, 0,0, 0,9, 0,0, 0,4);
  testOneAdjustMCoordRange_adj(tdc, 0,0, 4,1, 0,0, 4,0);
  testOneAdjustMCoordRange_adj(tdc, 0,0, 5,0, 0,0, 4,0);

  testOneAdjustMCoordRange_adj(tdc, 1,1, 1,9, 1,1, 1,3);

  // end < start
  testOneAdjustMCoordRange_adj(tdc, 2,2, 1,1, 2,2, 2,2);
  testOneAdjustMCoordRange_adj(tdc, 2,9, 1,1, 2,3, 2,3);
  testOneAdjustMCoordRange_adj(tdc, -5,0, 5,0, 0,0, 4,0);
  testOneAdjustMCoordRange_adj(tdc, -5,0, -1,0, 0,0, 0,0);
}


void test_wholeFileString()
{
  TextDocumentCore doc;
  std::string str = "a\nb\nc";
  doc.replaceWholeFileString(str);
  EXPECT_EQ(doc.getWholeFileString(), str);
}


void replaceRange(
  TextDocumentCore &doc,
  int startLine,
  int startByteIndex,
  int endLine,
  int endByteIndex,
  char const *text)
{
  doc.replaceMultilineRange(
    TextMCoordRange(
      TextMCoord(LineIndex(startLine), ByteIndex(startByteIndex)),
      TextMCoord(LineIndex(endLine), ByteIndex(endByteIndex))),
    text);
}


void testOne_replaceMultilineRange(
  TextDocumentCore &doc,
  int startLine,
  int startByteIndex,
  int endLine,
  int endByteIndex,
  char const *text,
  char const *expect)
{
  TEST_CASE_EXPRS("testOne_replaceMultilineRange",
    startLine,
    startByteIndex,
    endLine,
    endByteIndex,
    text);

  replaceRange(
    doc,
    startLine,
    startByteIndex,
    endLine,
    endByteIndex,
    text);
  EXPECT_EQ(doc.getWholeFileString(), expect);
}


void test_replaceMultilineRange()
{
  TextDocumentCore doc;
  EXPECT_EQ(doc.getWholeFileString(), "");

  testOne_replaceMultilineRange(doc, 0,0,0,0, "zero\none\n",
    "zero\n"
    "one\n");

  testOne_replaceMultilineRange(doc, 2,0,2,0, "two\nthree\n",
    "zero\n"
    "one\n"
    "two\n"
    "three\n");

  testOne_replaceMultilineRange(doc, 1,1,2,2, "XXXX\nYYYY",
    "zero\n"
    "oXXXX\n"
    "YYYYo\n"
    "three\n");

  testOne_replaceMultilineRange(doc, 0,4,3,0, "",
    "zerothree\n");

  testOne_replaceMultilineRange(doc, 0,9,1,0, "",
    "zerothree");

  testOne_replaceMultilineRange(doc, 0,2,0,3, "0\n1\n2\n3",
    "ze0\n"
    "1\n"
    "2\n"
    "3othree");
}


void test_equals()
{
  TextDocumentCore doc1, doc2;
  xassert(op_eq(doc1, doc2));

  replaceRange(doc1, 0,0,0,0, "zero\none\ntwo\n");
  xassert(!op_eq(doc1, doc2));

  replaceRange(doc2, 0,0,0,0, "two\n");
  xassert(!op_eq(doc1, doc2));

  replaceRange(doc2, 0,0,0,0, "zero\n");
  xassert(!op_eq(doc1, doc2));

  replaceRange(doc2, 1,0,1,0, "one\n");
  xassert(op_eq(doc1, doc2));

  replaceRange(doc1, 3,0,3,0, "D\n");
  xassert(!op_eq(doc1, doc2));
  replaceRange(doc2, 3,0,3,0, "D\n");
  xassert(op_eq(doc1, doc2));

  doc1.deleteTextBytes(tmc(LineIndex(3), 0), ByteCount(1));
  xassert(!op_eq(doc1, doc2));
  doc2.deleteTextBytes(tmc(LineIndex(3), 0), ByteCount(1));

  // Both should have line 3 as recent.
  xassert(op_eq(doc1, doc2));

  doc1.deleteLine(LineIndex(3));
  doc2.deleteLine(LineIndex(3));

  // Neither document has a recent line since we deleted the recent
  // lines.
  xassert(op_eq(doc1, doc2));

  replaceRange(doc1, 1,0,1,0, "B");
  xassert(!op_eq(doc1, doc2));
  replaceRange(doc2, 1,0,1,0, "B");

  // Both have a recent line, and it is the same line.
  xassert(op_eq(doc2, doc2));

  replaceRange(doc1, 0,0,0,0, "A");
  xassert(!op_eq(doc1, doc2));
  replaceRange(doc1, 2,0,2,0, "C");
  xassert(!op_eq(doc1, doc2));

  replaceRange(doc2, 2,0,2,0, "C");
  xassert(!op_eq(doc1, doc2));
  replaceRange(doc2, 0,0,0,0, "A");

  // Both documents have a recent line, but it is different.
  xassert(op_eq(doc1, doc2));

  replaceRange(doc1, 0,0,0,0, "A");
  xassert(!op_eq(doc1, doc2));
}


void test_getWholeLineStringOrRangeErrorMessage()
{
  TextDocumentCore doc;
  doc.replaceWholeFileString("zero\none\ntwo\n");

  std::string const fname("the-fname");

  EXPECT_EQ(
    doc.getWholeLineStringOrRangeErrorMessage(LineIndex(2), fname),
    "two");

  EXPECT_EQ(
    doc.getWholeLineStringOrRangeErrorMessage(LineIndex(4), fname),
    "<Line number 5 is out of range for \"the-fname\", "
    "which has 4 lines.>");
}


CLOSE_ANONYMOUS_NAMESPACE


// Called from unit-tests.cc.
void test_td_core(CmdlineArgsSpan args)
{
  testReadTwice();
  testReadSourceCode();
  testAtomicRead();
  testVarious();
  testWalkCoordBytes();
  test_adjustMCoord();
  test_wholeFileString();
  test_replaceMultilineRange();
  test_equals();
  test_getWholeLineStringOrRangeErrorMessage();
}


// EOF
