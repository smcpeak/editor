// text-search-test.cc
// Tests for 'text-search' module.

#include "text-search.h"               // this module

// editor
#include "td-editor.h"                 // TextDocumentAndEditor

// smbase
#include "smbase/nonport.h"            // getMilliseconds
#include "smbase/sm-iostream.h"        // cout
#include "smbase/sm-test.h"            // USUAL_TEST_MAIN
#include "smbase/trace.h"              // TRACE_ARGS


static void expectTotalMatches(TextSearch const &ts, int expect)
{
  EXPECT_EQ(ts.countRangeMatches(0, ts.documentLines()), expect);
}


static string dumpMatches(TextSearch const &ts)
{
  std::ostringstream sb;

  for (int line=0; line < ts.documentLines(); line++) {
    if (ts.countLineMatches(line)) {
      sb << line << ':';
      ArrayStack<TextSearch::MatchExtent> const &matches =
        ts.getLineMatches(line);
      for (int i=0; i < matches.length(); i++) {
        sb << '[' << matches[i].m_startByte
           << ',' << matches[i].m_lengthBytes << ']';
      }
      sb << '\n';
    }
  }

  return sb.str();
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
  tde.setCursor(TextLCoord(0, 2));
  tde.insertNulTermText("xyz");
  expectMatches(ts, "0:[0,3]\n");
  tde.insertNulTermText("onxonx onx");
  expectMatches(ts, "0:[0,3][5,3][12,3]\n");

  // Delete some of those things.
  tde.setCursor(TextLCoord(0,0));
  tde.setMark(TextLCoord(0,11));
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


static void expectRIM(TextSearch &ts,
  int lineA, int colA, int lineB, int colB, bool expectRes)
{
  TextMCoord a(lineA,colA);
  TextMCoord b(lineB,colB);
  bool actualRes = ts.rangeIsMatch(a, b);
  EXPECT_EQ(actualRes, expectRes);
}


static void expectNM_true(TextSearch const &ts,
  int cursorLine, int cursorCol,
  int markLine, int markCol,
  bool reverse,
  int expectCursorLine, int expectCursorCol,
  int expectMarkLine, int expectMarkCol)
{
  for (int i=0; i < 2; i++) {
    TextMCoord cursor(cursorLine, cursorCol);
    TextMCoord mark(markLine, markCol);
    if (i==1) {
      // The result should be independent of the order of 'cursor' and
      // 'mark'.
      swap(cursor, mark);
    }

    TextMCoordRange range(cursor, mark);
    bool actualRes = ts.nextMatch(reverse, range);
    EXPECT_EQ(actualRes, true);
    EXPECT_EQ(range.m_start.m_line, expectCursorLine);
    EXPECT_EQ(range.m_start.m_byteIndex, expectCursorCol);
    EXPECT_EQ(range.m_end.m_line, expectMarkLine);
    EXPECT_EQ(range.m_end.m_byteIndex, expectMarkCol);
  }
}


static void expectNM_false(TextSearch const &ts,
  int cursorLine, int cursorCol,
  int markLine, int markCol,
  bool reverse)
{
  for (int i=0; i < 2; i++) {
    TextMCoord cursor(cursorLine, cursorCol);
    TextMCoord mark(markLine, markCol);
    if (i==1) {
      // The result should be independent of the order of 'cursor' and
      // 'mark'.
      swap(cursor, mark);
    }

    TextMCoordRange range(cursor, mark);
    bool actualRes = ts.nextMatch(reverse, range);
    EXPECT_EQ(actualRes, false);

    // Output values of 'cursor' and 'mark' are unspecified.
  }
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

  // Test 'rangeIsMatch'.
  expectRIM(ts, 0,0, 0,0, false);
  expectRIM(ts, 0,0, 0,2, true);
  expectRIM(ts, 0,2, 0,0, true);
  expectRIM(ts, 2,7, 2,9, true);
  expectRIM(ts, 2,6, 2,9, false);
  expectRIM(ts, 2,7, 3,9, false);

  // Test 'nextMatch'.

  // Cursor near first match, going forward.
  expectNM_true (ts, 0,0, 0,0, false, 0,0, 0,2);  // create/expand sel
  expectNM_true (ts, 0,0, 0,1, false, 0,0, 0,2);  // expand sel
  expectNM_true (ts, 0,0, 0,2, false, 1,1, 1,3);  // selected; next match
  expectNM_true (ts, 0,0, 0,3, false, 1,1, 1,3);  // mark past; next match

  expectNM_true (ts, 0,1, 0,1, false, 1,1, 1,3);  // cursor after start; next
  expectNM_true (ts, 0,1, 0,2, false, 1,1, 1,3);  // cursor after start; next
  expectNM_true (ts, 0,1, 0,3, false, 1,1, 1,3);  // cursor after start; next

  expectNM_true (ts, 0,2, 0,2, false, 1,1, 1,3);  // cursor at end; next
  expectNM_true (ts, 0,2, 0,3, false, 1,1, 1,3);  // cursor at end; next

  // Cursor near first match, going backward
  expectNM_false(ts, 0,0, 0,0, true);             // cursor at start; prev; none
  expectNM_false(ts, 0,0, 0,1, true);             // cursor at start; prev; none
  expectNM_false(ts, 0,0, 0,2, true);             // match selected; prev; none
  expectNM_true (ts, 0,0, 0,3, true, 0,0, 0,2);   // mark past; prev

  expectNM_true (ts, 0,1, 0,1, true, 0,0, 0,2);   // cursor past; prev
  expectNM_true (ts, 0,1, 0,2, true, 0,0, 0,2);   // cursor past; prev

  // Repeat the matches just for ease of reference within this test.
  expectMatches(ts,
    "0:[0,2]\n"
    "1:[1,2]\n"
    "2:[0,2][7,2]\n"
    "3:[2,2][9,2]\n"
  );

  // Cursor near second match, going forward.
  expectNM_true (ts, 1,0, 1,0, false, 1,1, 1,3);  // cursor before; next
  expectNM_true (ts, 1,0, 1,1, false, 1,1, 1,3);  // cursor before; next
  expectNM_true (ts, 1,0, 1,2, false, 1,1, 1,3);  // cursor before; next
  expectNM_true (ts, 1,0, 1,3, false, 1,1, 1,3);  // cursor before; next
  expectNM_true (ts, 1,0, 1,4, false, 1,1, 1,3);  // cursor before; next

  expectNM_true (ts, 1,1, 1,1, false, 1,1, 1,3);  // cursor on start; expand
  expectNM_true (ts, 1,1, 1,2, false, 1,1, 1,3);  // expand
  expectNM_true (ts, 1,1, 1,3, false, 2,0, 2,2);  // selected; next
  expectNM_true (ts, 1,1, 1,4, false, 2,0, 2,2);  // mark past; next

  expectNM_true (ts, 1,2, 1,2, false, 2,0, 2,2);  // cursor past start; next
  expectNM_true (ts, 1,2, 1,3, false, 2,0, 2,2);  // cursor past start; next
  expectNM_true (ts, 1,2, 1,4, false, 2,0, 2,2);  // cursor past start; next

  // Near second, going backward.
  expectNM_true (ts, 1,0, 1,0, true, 0,0, 0,2);   // cursor before; back
  expectNM_true (ts, 1,0, 1,1, true, 0,0, 0,2);   // cursor before; back
  expectNM_true (ts, 1,0, 1,2, true, 0,0, 0,2);   // cursor before; back
  expectNM_true (ts, 1,0, 1,3, true, 0,0, 0,2);   // cursor before; back
  expectNM_true (ts, 1,0, 1,4, true, 0,0, 0,2);   // cursor before; back

  expectNM_true (ts, 1,1, 1,1, true, 0,0, 0,2);   // cursor on start; back
  expectNM_true (ts, 1,1, 1,2, true, 0,0, 0,2);   // partial sel; back
  expectNM_true (ts, 1,1, 1,3, true, 0,0, 0,2);   // selected; back
  expectNM_true (ts, 1,1, 1,4, true, 1,1, 1,3);   // mark past end; shrink sel

  expectNM_true (ts, 1,2, 1,2, true, 1,1, 1,3);   // cursor past; back
  expectNM_true (ts, 1,2, 1,3, true, 1,1, 1,3);   // cursor past; back
  expectNM_true (ts, 1,2, 1,4, true, 1,1, 1,3);   // cursor past; back

  // Repeat the matches just for ease of reference within this test.
  expectMatches(ts,
    "0:[0,2]\n"
    "1:[1,2]\n"
    "2:[0,2][7,2]\n"
    "3:[2,2][9,2]\n"
  );

  // Near last, going forward.
  expectNM_true (ts, 3,8, 3,8, false, 3,9, 3,11);  // cursor before; next
  expectNM_true (ts, 3,8, 3,9, false, 3,9, 3,11);  // cursor before; next
  expectNM_true (ts, 3,8, 3,10, false, 3,9, 3,11); // cursor before; next
  expectNM_true (ts, 3,8, 3,11, false, 3,9, 3,11); // cursor before; next
  expectNM_true (ts, 3,8, 3,12, false, 3,9, 3,11); // cursor before; next

  expectNM_true (ts, 3,9, 3,9, false, 3,9, 3,11);  // cursor on; expand
  expectNM_true (ts, 3,9, 3,10, false, 3,9, 3,11); // cursor on; expand
  expectNM_false(ts, 3,9, 3,11, false);            // selected; next; none
  expectNM_false(ts, 3,9, 3,12, false);            // mark past; next; none

  expectNM_false(ts, 3,10, 3,10, false);           // cursor past; next; none
  expectNM_false(ts, 3,10, 3,11, false);           // cursor past; next; none
  expectNM_false(ts, 3,10, 3,12, false);           // cursor past; next; none

  // Starting well beyond EOF, we should still find matches when doing
  // reverse search.
  expectNM_true (ts, 12,7, 12,7, true, 3,9, 3,11);  // beyond EOF; back
  expectNM_false(ts, 12,7, 12,7, false);            // beyond EOF; next; none
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


static void expectGRT(TextSearch const &ts,
  string const &existing,
  string const &replaceSpec,
  string const &expect)
{
  string actual = ts.getReplacementText(existing, replaceSpec);
  EXPECT_EQ(actual, expect);
}

static void testGetReplacementText()
{
  TextDocumentAndEditor tde;
  TextSearch ts(tde.getDocumentCore());

  ts.setSearchStringFlags(TextSearch::SS_REGEX);
  ts.setSearchString("foo\\((\\w+)\\)");

  expectGRT(ts, "foo(bar)", "oof(\\1)", "oof(bar)");
  expectGRT(ts, "foo(bar)", "\\1\\2\\0", "barfoo(bar)");
  expectGRT(ts, "foo(bar)", "\\t\\n\\r", "\t\n\r");
  expectGRT(ts, "foo(bar)", "\\z\\", "z\\");

  ts.setSearchStringFlags(TextSearch::SS_NONE);
  ts.setSearchString("foo(bar)");

  expectGRT(ts, "foo(bar)", "oof(\\1)", "oof(\\1)");
  expectGRT(ts, "foo(bar)", "\\1\\2\\0", "\\1\\2\\0");
  expectGRT(ts, "foo(bar)", "\\t\\n\\r", "\\t\\n\\r");
  expectGRT(ts, "foo(bar)", "\\z\\", "\\z\\");
}


static void populateDocument(TextDocumentEditor &tde, int lines)
{
  for (int i=0; i < lines; i++) {
    // Each line has a line number to ensure the strings are not exactly
    // identical, which something under the hood might notice and
    // exploit, making the test not representative.
    tde.insertString(stringb(i << ". " <<
      "Animals need lots of room and roads to roam.  "
      "C++::has->(*funny)(*punctuation).\n"));
  }
}


static void testPerformance()
{
  TextDocumentAndEditor tde;
  TextSearch ts(tde.getDocumentCore());
  ts.setSearchString("roam");

  int const NUM_LINES = 1000;      // Right at match limit.
  populateDocument(tde, NUM_LINES);

  for (int opts=0; opts <= TextSearch::SS_ALL; opts++) {
    ts.setSearchStringFlags((TextSearch::SearchStringFlags)opts);

    long start = getMilliseconds();
    int const ITERS = 200;
    for (int i=0; i < ITERS; i++) {
      // Trigger a complete re-evaluation.
      ts.observeTotalChange(tde.writableDoc().getCore());
      xassert(ts.countAllMatches() == NUM_LINES);
      xassert(ts.hasIncompleteMatches() == false);
    }
    long end = getMilliseconds();

    cout << "perf: opts=" << opts
         << " lines=" << NUM_LINES
         << " iters=" << ITERS
         << " ms=" << (end-start) << endl;
  }

  // Exercise hitting the match limit.
  ts.setMatchCountLimit(100);
  ts.observeTotalChange(tde.writableDoc().getCore());
  xassert(100 <= ts.countAllMatches() &&
                 ts.countAllMatches() < NUM_LINES);
  xassert(ts.hasIncompleteMatches() == true);

  // Then un-hit it.
  ts.setMatchCountLimit(NUM_LINES);
  ts.observeTotalChange(tde.writableDoc().getCore());
  xassert(ts.countAllMatches() == NUM_LINES);
  xassert(ts.hasIncompleteMatches() == false);
}


static void testRegexPerf2(bool nolimit)
{
  TextDocumentAndEditor tde;
  TextSearch ts(tde.getDocumentCore());

  int const NUM_LINES = 10000;
  populateDocument(tde, NUM_LINES);

  if (nolimit) {
    ts.setMatchCountLimit(100000000);
  }

  ts.setSearchStringFlags(TextSearch::SS_REGEX);

  long start = getMilliseconds();
  ts.setSearchString(".");
  long elapsed = getMilliseconds() - start;

  cout << "perf2 init: " << elapsed << endl;

  int const ITERS = 10;
  for (int i=0; i < ITERS; i++) {
    start = getMilliseconds();
    ts.setSearchString("");
    elapsed = getMilliseconds() - start;
    cout << "perf2 reset " << i << ": " << elapsed << endl;

    start = getMilliseconds();
    ts.setSearchString(".");
    elapsed = getMilliseconds() - start;
    cout << "perf2 iter " << i << ": " << elapsed << endl;
  }
}


#define PRINT_ELAPSED(stmt)                     \
  {                                             \
    elapsed = 0;                                \
    GetMillisecondsAccumulator acc(elapsed);    \
    stmt;                                       \
  }                                             \
  cout << #stmt ": " << elapsed << endl /* user ; */


static void testPerf3()
{
  cout << "testPerf3\n";

  TextDocumentAndEditor tde;
  TextSearch ts(tde.getDocumentCore());

  long elapsed;

  int const NUM_LINES = 300000;
  PRINT_ELAPSED(populateDocument(tde, NUM_LINES));

  ts.setMatchCountLimit(NUM_LINES * 2);

  // String that matches.
  PRINT_ELAPSED(ts.setSearchString("need lots of room"));
  PVAL(ts.countAllMatches());

  // String that does not match.
  PRINT_ELAPSED(ts.setSearchString("need lots of zoom"));
  PVAL(ts.countAllMatches());

  PRINT_ELAPSED(ts.setSearchString("need lots of xoom"));
  PVAL(ts.countAllMatches());

  PRINT_ELAPSED(ts.setSearchString("need lots of room"));
  PVAL(ts.countAllMatches());

  PRINT_ELAPSED(ts.setSearchString("eed lots of room "));
  PVAL(ts.countAllMatches());
}


static void entry(int argc, char **argv)
{
  TRACE_ARGS();

  if (argc >= 2 && 0==strcmp(argv[1], "perf2")) {
    testRegexPerf2(false /*nolimit*/);
    return;
  }

  if (argc >= 2 && 0==strcmp(argv[1], "perf2nl")) {
    testRegexPerf2(true /*nolimit*/);
    return;
  }

  if (argc >= 2 && 0==strcmp(argv[1], "perf3")) {
    testPerf3();
    return;
  }

  // NOTE: Currently these tests do not exercise any deviation between
  // TextLCoord and TextMCoord.

  testEmpty();
  testSimple();
  testCaseInsensitive();
  testRegex();
  testGetReplacementText();
  testPerformance();
  testRegexPerf2(false /*nolimit*/);

  cout << "text-search-test ok" << endl;
}

ARGS_TEST_MAIN

// EOF
