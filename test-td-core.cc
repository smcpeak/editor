// test-td-core.cc
// Tests for 'td-core' module.

#include "td-core.h"                   // module to test

// smbase
#include "autofile.h"                  // AutoFILE
#include "ckheap.h"                    // malloc_stats
#include "test.h"                      // USUAL_MAIN, EXPECT_EQ

// libc
#include <assert.h>                    // assert
#include <stdlib.h>                    // system


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
  core.readFile("td-core.tmp");
  xassert(core.numLines() == 1001);

  // Read it again with an injected error.
  try {
    TextDocumentCore::s_injectedErrorCountdown = 10000;
    cout << "This should throw:" << endl;
    core.readFile("td-core.tmp");
    assert(!"should not get here");
  }
  catch (xBase &x) {
    // As expected.
  }

  // Should have consumed this.
  xassert(TextDocumentCore::s_injectedErrorCountdown == 0);

  // Confirm that the original contents are still there.
  xassert(core.numLines() == 1001);

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


static void testVarious()
{
  TextDocumentCore tdc;

  EXPECT_EQ(tdc.numLines(), 1);
  EXPECT_EQ(tdc.lineLengthBytes(0), 0);
  EXPECT_EQ(tdc.validCoord(TextMCoord(0,0)), true);
  EXPECT_EQ(tdc.validCoord(TextMCoord(0,1)), false);
  EXPECT_EQ(tdc.endCoord(), TextMCoord(0,0));
  EXPECT_EQ(tdc.maxLineLengthBytes(), 0);
  EXPECT_EQ(tdc.numLinesExceptFinalEmpty(), 0);

  insLine(tdc, 0,0, "one");
  EXPECT_EQ(tdc.numLines(), 2);
  EXPECT_EQ(tdc.numLinesExceptFinalEmpty(), 1);
  insLine(tdc, 1,0, "  two");
  EXPECT_EQ(tdc.numLines(), 3);
  EXPECT_EQ(tdc.numLinesExceptFinalEmpty(), 2);
  insLine(tdc, 2,0, "three   ");
  insLine(tdc, 3,0, "    four    ");
  insLine(tdc, 4,0, "     ");
  tdc.insertLine(5);     // Uses NULL representation internally.
  insText(tdc, 6,0, "      ");

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

  checkSpaces(tdc, 0, 0, 0);
  checkSpaces(tdc, 1, 2, 0);
  checkSpaces(tdc, 2, 0, 3);
  checkSpaces(tdc, 3, 4, 4);
  checkSpaces(tdc, 4, 5, 5);
  checkSpaces(tdc, 5, 0, 0);
  checkSpaces(tdc, 6, 6, 6);

  for (int line=0; line <= 6; line++) {
    // Tweak 'line' so it is recent and then repeat the whitespace queries.
    TextMCoord tc(line, 0);
    char c = 'x';
    tdc.insertText(tc, &c, 1);
    tdc.deleteText(tc, 1);

    checkSpaces(tdc, 0, 0, 0);
    checkSpaces(tdc, 1, 2, 0);
    checkSpaces(tdc, 2, 0, 3);
    checkSpaces(tdc, 3, 4, 4);
    checkSpaces(tdc, 4, 5, 5);
    checkSpaces(tdc, 5, 0, 0);
    checkSpaces(tdc, 6, 6, 6);
  }

  // This is far from a comprehensive test of observers, but I want to
  // at least test 'hasObserver'.
  TextDocumentObserver obs;
  xassert(!tdc.hasObserver(&obs));
  tdc.addObserver(&obs);
  xassert(tdc.hasObserver(&obs));
  tdc.removeObserver(&obs);
  xassert(!tdc.hasObserver(&obs));
}


static void entry()
{
  for (int looper=0; looper<2; looper++) {
    printf("stats before:\n");
    malloc_stats();

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
      doc.readFile("td-core.tmp");

      // dump its repr
      //doc.dumpRepresentation();

      // write it out again
      doc.writeFile("td-core.tmp2");

      printf("stats before dealloc:\n");
      malloc_stats();

      printf("\nbuffer mem usage stats:\n");
      doc.printMemStats();
    }

    // make sure they're the same
    if (system("diff td-core.tmp td-core.tmp2") != 0) {
      xbase("the files were different!\n");
    }

    // ok
    system("ls -l td-core.tmp");
    remove("td-core.tmp");
    remove("td-core.tmp2");

    printf("stats after:\n");
    malloc_stats();
  }

  {
    printf("reading td-core.cc ...\n");
    TextDocumentCore doc;
    doc.readFile("td-core.cc");
    doc.printMemStats();
  }

  testAtomicRead();
  testVarious();

  printf("stats after:\n");
  malloc_stats();

  printf("\ntd-core is ok\n");
}

USUAL_TEST_MAIN


// EOF
