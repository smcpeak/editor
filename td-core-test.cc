// td-core-test.cc
// Tests for 'td-core' module.

#include "td-core.h"                   // module to test

// smbase
#include "smbase/autofile.h"           // AutoFILE
#include "smbase/exc.h"                // smbase::xmessage
#include "smbase/sm-file-util.h"       // SMFileUtil
#include "smbase/sm-macros.h"          // IGNORE_RESULT
#include "smbase/sm-test.h"            // USUAL_MAIN, EXPECT_EQ
#include "smbase/string-util.h"        // vectorOfUCharToString

// libc
#include <assert.h>                    // assert
#include <stdlib.h>                    // system

using namespace smbase;


// Self-check the 'tdc' and also verify that the iterator works for
// every line.
static void fullSelfCheck(TextDocumentCore const &tdc)
{
  tdc.selfCheck();

  ArrayStack<char> text;
  for (int i=0; i < tdc.numLines(); i++) {
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
    TextDocumentCore::LineIterator it(tdc, -1);
    xassert(!it.has());
  }
  {
    TextDocumentCore::LineIterator it(tdc, tdc.numLines());
    xassert(!it.has());
  }
}


static void testAtomicRead()
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


static void insText(TextDocumentCore &tdc, int line, int col, char const *text)
{
  tdc.insertText(TextMCoord(line, col), text, strlen(text));
}

static void insLine(TextDocumentCore &tdc, int line, int col, char const *text)
{
  tdc.insertLine(line);
  insText(tdc, line, col, text);
}


static void checkSpaces(TextDocumentCore const &tdc,
  int line, int leading, int trailing)
{
  EXPECT_EQ(tdc.countLeadingSpacesTabs(line), leading);
  EXPECT_EQ(tdc.countTrailingSpacesTabs(line), trailing);
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


static void testVarious()
{
  TextDocumentCore tdc;
  TextDocumentCore::VersionNumber vnum = tdc.getVersionNumber();

  EXPECT_EQ(tdc.numLines(), 1);
  EXPECT_EQ(tdc.lineLengthBytes(0), 0);
  EXPECT_EQ(tdc.validCoord(TextMCoord(0,0)), true);
  EXPECT_EQ(tdc.validCoord(TextMCoord(0,1)), false);
  EXPECT_EQ(tdc.endCoord(), TextMCoord(0,0));
  EXPECT_EQ(tdc.maxLineLengthBytes(), 0);
  EXPECT_EQ(tdc.numLinesExceptFinalEmpty(), 0);
  fullSelfCheck(tdc);

  CHECK_VER_SAME(tdc, vnum);
  insLine(tdc, 0,0, "one");
  CHECK_VER_DIFF(tdc, vnum);
  EXPECT_EQ(tdc.numLines(), 2);
  EXPECT_EQ(tdc.numLinesExceptFinalEmpty(), 1);
  CHECK_VER_SAME(tdc, vnum);
  insLine(tdc, 1,0, "  two");
  CHECK_VER_DIFF(tdc, vnum);
  EXPECT_EQ(tdc.numLines(), 3);
  EXPECT_EQ(tdc.numLinesExceptFinalEmpty(), 2);
  insLine(tdc, 2,0, "three   ");
  CHECK_VER_DIFF(tdc, vnum);
  insLine(tdc, 3,0, "    four    ");
  CHECK_VER_DIFF(tdc, vnum);
  insLine(tdc, 4,0, "     ");
  CHECK_VER_DIFF(tdc, vnum);
  tdc.insertLine(5);     // Uses NULL representation internally.
  CHECK_VER_DIFF(tdc, vnum);
  insText(tdc, 6,0, "      ");
  CHECK_VER_DIFF(tdc, vnum);

  EXPECT_EQ(tdc.numLines(), 7);
  EXPECT_EQ(tdc.numLinesExceptFinalEmpty(), 7);
  EXPECT_EQ(tdc.lineLengthBytes(0), 3);
  EXPECT_EQ(tdc.lineLengthBytes(6), 6);
  EXPECT_EQ(tdc.validCoord(TextMCoord(0,0)), true);
  EXPECT_EQ(tdc.validCoord(TextMCoord(0,1)), true);
  EXPECT_EQ(tdc.validCoord(TextMCoord(6,6)), true);
  EXPECT_EQ(tdc.validCoord(TextMCoord(6,7)), false);
  EXPECT_EQ(tdc.validCoord(TextMCoord(7,0)), false);
  EXPECT_EQ(tdc.endCoord(), TextMCoord(6,6));
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
    TextMCoord tc(line, 0);
    char c = 'x';
    CHECK_VER_SAME(tdc, vnum);
    tdc.insertText(tc, &c, 1);
    CHECK_VER_DIFF(tdc, vnum);
    tdc.deleteTextBytes(tc, 1);
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
  tdc.deleteLine(5);
  CHECK_VER_DIFF(tdc, vnum);
  EXPECT_EQ(tdc.numLines(), 6);
  EXPECT_EQ(doubleQuote(vectorOfUCharToString(tdc.getWholeFile())),
            doubleQuote("one\n"
                        "  two\n"
                        "three   \n"
                        "    four    \n"
                        "     \n"
                        "      "));
}


// I don't remember what this was meant to test...
static void testReadTwice()
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

      printf("\nbuffer mem usage stats:\n");
      doc.printMemStats();
      fullSelfCheck(doc);
    }

    // make sure they're the same
    if (system("diff td-core.tmp td-core.tmp2") != 0) {
      xmessage("the files were different!\n");
    }

    // ok
    IGNORE_RESULT(system("ls -l td-core.tmp"));
    remove("td-core.tmp");
    remove("td-core.tmp2");
  }
}


// Read the td-core.cc source code, just as an example of a real file.
static void testReadSourceCode()
{
  printf("reading td-core.cc ...\n");
  TextDocumentCore doc;
  doc.replaceWholeFile(SMFileUtil().readFile("td-core.cc"));
  doc.printMemStats();
  fullSelfCheck(doc);
}


static void expectWalkCoordBytes(
  TextDocumentCore &tdc,
  TextMCoord const &tc,
  int distance,
  TextMCoord const &expect)
{
  TextMCoord actual(tc);
  bool res = tdc.walkCoordBytes(actual, distance);
  EXPECT_EQ(res, true);
  EXPECT_EQ(actual, expect);
}

static void expectWalkCoordBytes_false(
  TextDocumentCore &tdc,
  TextMCoord const &tc,
  int distance)
{
  TextMCoord actual(tc);
  bool res = tdc.walkCoordBytes(actual, distance);
  EXPECT_EQ(res, false);
}

static void expectInvalidCoord(
  TextDocumentCore &tdc,
  TextMCoord const &tc)
{
  bool res = tdc.validCoord(tc);
  EXPECT_EQ(res, false);
}

static void testWalkCoordBytes()
{
  TextDocumentCore tdc;
  tdc.insertLine(0);
  tdc.insertString(TextMCoord(0,0), "one");
  tdc.insertLine(1);
  tdc.insertLine(2);
  tdc.insertString(TextMCoord(2,0), "three");

  expectWalkCoordBytes_false(tdc, TextMCoord(0,0), -1);
  expectWalkCoordBytes      (tdc, TextMCoord(0,0),  0, TextMCoord(0,0));
  expectWalkCoordBytes      (tdc, TextMCoord(0,0),  1, TextMCoord(0,1));
  expectWalkCoordBytes      (tdc, TextMCoord(0,0),  2, TextMCoord(0,2));
  expectWalkCoordBytes      (tdc, TextMCoord(0,0),  3, TextMCoord(0,3));
  expectWalkCoordBytes      (tdc, TextMCoord(0,0),  4, TextMCoord(1,0));
  expectWalkCoordBytes      (tdc, TextMCoord(0,0),  5, TextMCoord(2,0));
  expectWalkCoordBytes      (tdc, TextMCoord(0,0),  6, TextMCoord(2,1));
  expectWalkCoordBytes      (tdc, TextMCoord(0,0),  7, TextMCoord(2,2));
  expectWalkCoordBytes      (tdc, TextMCoord(0,0),  8, TextMCoord(2,3));
  expectWalkCoordBytes      (tdc, TextMCoord(0,0),  9, TextMCoord(2,4));
  expectWalkCoordBytes      (tdc, TextMCoord(0,0), 10, TextMCoord(2,5));
  expectWalkCoordBytes      (tdc, TextMCoord(0,0), 11, TextMCoord(3,0));
  expectWalkCoordBytes_false(tdc, TextMCoord(0,0), 12);

  expectWalkCoordBytes_false(tdc, TextMCoord(0,1), -2);
  expectWalkCoordBytes      (tdc, TextMCoord(0,1), -1, TextMCoord(0,0));
  expectWalkCoordBytes      (tdc, TextMCoord(0,1),  0, TextMCoord(0,1));
  expectWalkCoordBytes      (tdc, TextMCoord(0,1),  1, TextMCoord(0,2));
  expectWalkCoordBytes      (tdc, TextMCoord(0,1), 10, TextMCoord(3,0));
  expectWalkCoordBytes_false(tdc, TextMCoord(0,1), 11);

  expectWalkCoordBytes_false(tdc, TextMCoord(0,3), -4);
  expectWalkCoordBytes      (tdc, TextMCoord(0,3), -1, TextMCoord(0,2));
  expectWalkCoordBytes      (tdc, TextMCoord(0,3),  0, TextMCoord(0,3));
  expectWalkCoordBytes      (tdc, TextMCoord(0,3),  1, TextMCoord(1,0));
  expectWalkCoordBytes      (tdc, TextMCoord(0,3),  8, TextMCoord(3,0));
  expectWalkCoordBytes_false(tdc, TextMCoord(0,3),  9);

  expectWalkCoordBytes_false(tdc, TextMCoord(2,4),-10);
  expectWalkCoordBytes      (tdc, TextMCoord(2,4), -9, TextMCoord(0,0));
  expectWalkCoordBytes      (tdc, TextMCoord(2,4), -1, TextMCoord(2,3));
  expectWalkCoordBytes      (tdc, TextMCoord(2,4),  0, TextMCoord(2,4));
  expectWalkCoordBytes      (tdc, TextMCoord(2,4),  1, TextMCoord(2,5));
  expectWalkCoordBytes      (tdc, TextMCoord(2,4),  2, TextMCoord(3,0));
  expectWalkCoordBytes_false(tdc, TextMCoord(2,4),  3);
  expectWalkCoordBytes_false(tdc, TextMCoord(2,4), 99);

  expectWalkCoordBytes_false(tdc, TextMCoord(2,5),-11);
  expectWalkCoordBytes      (tdc, TextMCoord(2,5),-10, TextMCoord(0,0));
  expectWalkCoordBytes      (tdc, TextMCoord(2,5), -1, TextMCoord(2,4));
  expectWalkCoordBytes      (tdc, TextMCoord(2,5),  0, TextMCoord(2,5));
  expectWalkCoordBytes      (tdc, TextMCoord(2,5),  1, TextMCoord(3,0));
  expectWalkCoordBytes_false(tdc, TextMCoord(2,5),  2);

  expectWalkCoordBytes_false(tdc, TextMCoord(3,0),-12);
  expectWalkCoordBytes      (tdc, TextMCoord(3,0),-11, TextMCoord(0,0));
  expectWalkCoordBytes      (tdc, TextMCoord(3,0), -1, TextMCoord(2,5));
  expectWalkCoordBytes      (tdc, TextMCoord(3,0),  0, TextMCoord(3,0));
  expectWalkCoordBytes_false(tdc, TextMCoord(3,0),  1);

  expectInvalidCoord(tdc, TextMCoord(-1,0));
  expectInvalidCoord(tdc, TextMCoord(0,-1));
  expectInvalidCoord(tdc, TextMCoord(0,4));
  expectInvalidCoord(tdc, TextMCoord(1,-1));
  expectInvalidCoord(tdc, TextMCoord(1,1));
  expectInvalidCoord(tdc, TextMCoord(2,-1));
  expectInvalidCoord(tdc, TextMCoord(2,6));
  expectInvalidCoord(tdc, TextMCoord(3,-1));
  expectInvalidCoord(tdc, TextMCoord(3,1));
  expectInvalidCoord(tdc, TextMCoord(4,0));
}


static void entry()
{
  testReadTwice();
  testReadSourceCode();
  testAtomicRead();
  testVarious();
  testWalkCoordBytes();

  printf("\ntd-core-test is ok\n");
}

USUAL_TEST_MAIN


// EOF
