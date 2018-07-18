// test-text-search.cc
// Tests for 'text-search' module.

#include "text-search.h"               // this module

// editor
#include "td-editor.h"                 // TextDocumentAndEditor

// smbase
#include "nonport.h"                   // getMilliseconds
#include "sm-iostream.h"               // cout
#include "test.h"                      // USUAL_TEST_MAIN


static void expectTotalMatches(TextSearch const &ts, int expect)
{
  EXPECT_EQ(ts.countRangeMatches(0, ts.documentLines()), expect);
}


static string dumpMatches(TextSearch const &ts)
{
  stringBuilder sb;

  for (int line=0; line < ts.documentLines(); line++) {
    if (ts.countLineMatches(line)) {
      sb << line << ':';
      ArrayStack<TextSearch::MatchExtent> const &matches =
        ts.getLineMatches(line);
      for (int i=0; i < matches.length(); i++) {
        sb << '[' << matches[i].m_start
           << ',' << matches[i].m_length << ']';
      }
      sb << '\n';
    }
  }

  return sb;
}

static void expectMatches(TextSearch const &ts, string const &expect)
{
  string actual = dumpMatches(ts);
  EXPECT_EQ(actual, expect);
}


static void testEmpty()
{
  TextDocumentAndEditor tde;
  TextSearch ts(tde.getDocumentCore());
  expectTotalMatches(ts, 0);

  ts.setSearchString("foo");
  expectTotalMatches(ts, 0);
}


static void testSimple()
{
  TextDocumentAndEditor tde;
  TextSearch ts(tde.getDocumentCore());
  tde.insertNulTermText(
    "one\n"
    "two\n"
    "three\n"
  );

  // Simple initial search.
  ts.setSearchString("o");
  expectTotalMatches(ts, 2);
  expectMatches(ts,
    "0:[0,1]\n"
    "1:[2,1]\n");

  // Add a letter.
  ts.setSearchString("on");
  expectTotalMatches(ts, 1);
  expectMatches(ts,
    "0:[0,2]\n");

  // Add a letter, find nothing.
  ts.setSearchString("onx");
  expectTotalMatches(ts, 0);
  expectMatches(ts, "");

  // Insert text so it finds things.
  tde.setCursor(TextCoord(0, 2));
  tde.insertNulTermText("xyz");
  expectMatches(ts, "0:[0,3]\n");
  tde.insertNulTermText("onxonx onx");
  expectMatches(ts, "0:[0,3][5,3][12,3]\n");

  // Delete some of those things.
  tde.setCursor(TextCoord(0,0));
  tde.setMark(TextCoord(0,11));
  tde.deleteSelection();
  expectMatches(ts, "0:[1,3]\n");

  // Clear the search string, find nothing.
  ts.setSearchString("");
  expectMatches(ts, "");

  // Find something new.
  ts.setSearchString("r");
  expectMatches(ts, "2:[2,1]\n");

  // Clear the file.  This triggers 'observeTotalChange'.
  tde.writableDoc().clearContentsAndHistory();
  expectMatches(ts, "");
}


static void expectFMOOA(TextSearch &ts, int line, int col,
  bool expectRes, int expectMatchLine,
  int expectMatchStart, int expectMatchLength)
{
  TextSearch::MatchExtent match;
  TextCoord tc(line,col);
  bool actualRes = ts.firstMatchOnOrAfter(match, tc);
  EXPECT_EQ(actualRes, expectRes);
  if (actualRes) {
    EXPECT_EQ(tc.line, expectMatchLine);
    xassert(tc.column == match.m_start);
    EXPECT_EQ(match.m_start, expectMatchStart);
    EXPECT_EQ(match.m_length, expectMatchLength);
  }
}


static void expectFMBOA(TextSearch &ts, bool reverse, int line, int col,
  bool expectRes, int expectMatchLine,
  int expectMatchStart, int expectMatchLength)
{
  TextSearch::MatchExtent match;
  TextCoord tc(line,col);
  bool actualRes = ts.firstMatchBeforeOrAfter(reverse, match, tc);
  EXPECT_EQ(actualRes, expectRes);
  if (actualRes) {
    EXPECT_EQ(tc.line, expectMatchLine);
    xassert(tc.column == match.m_start);
    EXPECT_EQ(match.m_start, expectMatchStart);
    EXPECT_EQ(match.m_length, expectMatchLength);
  }
}


static void expectFMBOOA(TextSearch &ts,
  bool reverse, bool matchAtTC, int line, int col,
  bool expectRes, int expectMatchLine,
  int expectMatchStart, int expectMatchLength)
{
  TextSearch::MatchExtent match;
  TextCoord tc(line,col);
  bool actualRes = ts.firstMatchBeforeOnOrAfter(reverse, matchAtTC, match, tc);
  EXPECT_EQ(actualRes, expectRes);
  if (actualRes) {
    EXPECT_EQ(tc.line, expectMatchLine);
    xassert(tc.column == match.m_start);
    EXPECT_EQ(match.m_start, expectMatchStart);
    EXPECT_EQ(match.m_length, expectMatchLength);
  }
}


static void expectRIM(TextSearch &ts,
  int lineA, int colA, int lineB, int colB, bool expectRes)
{
  TextCoord a(lineA,colA);
  TextCoord b(lineB,colB);
  bool actualRes = ts.rangeIsMatch(a, b);
  EXPECT_EQ(actualRes, expectRes);
}


static void testCaseInsensitive()
{
  TextDocumentAndEditor tde;
  TextSearch ts(tde.getDocumentCore());
  ts.setSearchString("a");
  tde.insertNulTermText(
    "abc\n"
    " ABC\n"
    "ABRACADABRA\n"
    "  abracadabra  "    // No newline.
  );
  expectMatches(ts,
    "0:[0,1]\n"
    "3:[2,1][5,1][7,1][9,1][12,1]\n"
  );

  ts.setSearchStringFlags(TextSearch::SS_CASE_INSENSITIVE);
  expectMatches(ts,
    "0:[0,1]\n"
    "1:[1,1]\n"
    "2:[0,1][3,1][5,1][7,1][10,1]\n"
    "3:[2,1][5,1][7,1][9,1][12,1]\n"
  );

  ts.setSearchString("ab");
  expectMatches(ts,
    "0:[0,2]\n"
    "1:[1,2]\n"
    "2:[0,2][7,2]\n"
    "3:[2,2][9,2]\n"
  );

  ts.setSearchString("AB");
  expectMatches(ts,
    "0:[0,2]\n"
    "1:[1,2]\n"
    "2:[0,2][7,2]\n"
    "3:[2,2][9,2]\n"
  );

  ts.setSearchString("aB");
  expectMatches(ts,
    "0:[0,2]\n"
    "1:[1,2]\n"
    "2:[0,2][7,2]\n"
    "3:[2,2][9,2]\n"
  );

  // Text 'firstMatch'.
  expectFMOOA(ts, 0,0, true, 0,0,2);
  expectFMOOA(ts, 0,1, true, 1,1,2);
  expectFMOOA(ts, 3,9, true, 3,9,2);
  expectFMOOA(ts, 3,10, false, 0,0,0);

  expectFMBOA(ts, false, 0,0, true, 1,1,2);
  expectFMBOA(ts, true, 0,0, false, 0,0,0);

  expectFMBOOA(ts, false, true, 0,0, true, 0,0,2);
  expectFMBOOA(ts, false, false, 0,0, true, 1,1,2);
  expectFMBOOA(ts, true, true, 0,0, true, 0,0,2);
  expectFMBOOA(ts, true, false, 0,0, false, 0,0,0);

  expectFMBOOA(ts, false, true, 2,7, true, 2,7,2);
  expectFMBOOA(ts, false, false, 2,7, true, 3,2,2);
  expectFMBOOA(ts, true, true, 2,7, true, 2,7,2);
  expectFMBOOA(ts, true, false, 2,7, true, 2,0,2);

  // Starting well beyond EOF, we should still find matches when doing
  // reverse search.
  expectFMBOOA(ts, true, false, 12,7, true, 3,9,2);

  // Test 'rangeIsMatch'.
  expectRIM(ts, 0,0, 0,0, false);
  expectRIM(ts, 0,0, 0,2, true);
  expectRIM(ts, 0,2, 0,0, true);
  expectRIM(ts, 2,7, 2,9, true);
  expectRIM(ts, 2,6, 2,9, false);
  expectRIM(ts, 2,7, 3,9, false);
}


static void testRegex()
{
  TextDocumentAndEditor tde;
  TextSearch ts(tde.getDocumentCore());
  tde.insertNulTermText(
    "abc\n"              // 0
    " ABC\n"             // 1
    "ABRACADABRA\n"      // 2
    "    advertiser\n"   // 3
    "  abracadabra  "    // 4: No newline.
  );

  ts.setSearchStringFlags(TextSearch::SS_REGEX);
  ts.setSearchString("a[bd]");
  xassert(ts.searchStringIsValid());
  expectMatches(ts,
    "0:[0,2]\n"
    "3:[4,2]\n"
    "4:[2,2][7,2][9,2]\n"
  );

  ts.setSearchStringFlags(
    TextSearch::SS_REGEX | TextSearch::SS_CASE_INSENSITIVE);
  xassert(ts.searchStringIsValid());
  expectMatches(ts,
    "0:[0,2]\n"
    "1:[1,2]\n"
    "2:[0,2][5,2][7,2]\n"
    "3:[4,2]\n"
    "4:[2,2][7,2][9,2]\n"
  );

  // Invalid string.  Should not match anything, but also not blow up.
  ts.setSearchString("a[");
  xassert(!ts.searchStringIsValid());
  EXPECT_EQ(ts.searchStringErrorOffset(), 2);  // Error because string ends early.
  expectMatches(ts, "");
  cout << "Expected error message:" << endl;
  PVAL(ts.searchStringSyntaxError());
}


static void testPerformance()
{
  TextDocumentAndEditor tde;
  TextSearch ts(tde.getDocumentCore());
  ts.setSearchString("roam");

  // Populate the document.
  int const NUM_LINES = 1000;
  for (int i=0; i < NUM_LINES; i++) {
    // Each line has a line number to ensure the strings are not exactly
    // identical, which something under the hood might notice and
    // exploit, making the test not representative.
    tde.insertString(stringb(i << ". " <<
      "Animals need lots of room and roads to roam.  "
      "C++::has->(*funny)(*punctuation).\n"));
  }

  for (int opts=0; opts <= TextSearch::SS_ALL; opts++) {
    ts.setSearchStringFlags((TextSearch::SearchStringFlags)opts);

    long start = getMilliseconds();
    int const ITERS = 200;
    for (int i=0; i < ITERS; i++) {
      // Trigger a complete re-evaluation.
      ts.observeTotalChange(tde.writableDoc().getCore());
      xassert(ts.countAllMatches() == NUM_LINES);
    }
    long end = getMilliseconds();

    cout << "perf: opts=" << opts
         << " lines=" << NUM_LINES
         << " iters=" << ITERS
         << " ms=" << (end-start) << endl;
  }
}


static void entry()
{
  testEmpty();
  testSimple();
  testCaseInsensitive();
  testRegex();
  testPerformance();

  cout << "test-text-search ok" << endl;
}

USUAL_TEST_MAIN

// EOF
