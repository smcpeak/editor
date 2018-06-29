// test-td-editor.cc
// Tests for td-editor module.

#include "td-editor.h"                 // module to test

#include "datablok.h"                  // DataBlock
#include "ckheap.h"                    // malloc_stats
#include "strutil.h"                   // quoted

#include <unistd.h>                    // unlink
#include <stdio.h>                     // printf
#include <stdlib.h>                    // system


// This file is structured as a sequence of mostly-independent
// sections, each focused on testing one aspect of or function in
// TextDocumentEditor.


static void checkCoord(TextCoord expect, TextCoord actual, char const *label)
{
  if (expect != actual) {
    cout << "expect: " << expect << endl
         << "actual: " << actual << endl;
    xfailure(stringb(label << " coord mismatch"));
  }
}


static void expectCursor(TextDocumentEditor const &tde, int line, int col)
{
  checkCoord(TextCoord(line, col), tde.cursor(), "cursor");
}

static void expect(TextDocumentEditor const &tde, int line, int col, char const *text)
{
  tde.selfCheck();

  expectCursor(tde, line, col);

  writeFile(tde.core(), "td.tmp");
  DataBlock block;
  block.readFromFile("td.tmp");

  // compare contents to what is expected
  if (0!=memcmp(text, block.getDataC(), block.getDataLen()) ||
      (int)strlen(text)!=(int)block.getDataLen()) {
    cout << "expect: " << quoted(text) << endl;
    cout << "actual: \""
         << encodeWithEscapes(block.getDataC(), block.getDataLen())
         << '"' << endl;
    xfailure("text mismatch");
  }
}


// --------------------- testUndoRedo -----------------------
// Insert each character in 'str' as its own edit action.
static void chars(TextDocumentEditor &tde, char const *str)
{
  while (*str) {
    tde.insertText(str, 1);
    str++;
  }
}


static void testUndoRedo()
{
  TextDocumentAndEditor tde;

  chars(tde, "abcd");
  //tde.printHistory();
  expect(tde, 0,4, "abcd");

  tde.undo();
  //tde.printHistory();
  expect(tde, 0,3, "abc");

  chars(tde, "e");
  //tde.printHistory();
  expect(tde, 0,4, "abce");

  chars(tde, "\nThis is the second line.\n");
  expect(tde, 2,0, "abce\n"
                   "This is the second line.\n"
                   "");

  tde.moveCursor(true /*relLine*/, -1, true /*relCol*/, 2);
  chars(tde, "z");
  expect(tde, 1,3, "abce\n"
                   "Thzis is the second line.\n"
                   "");

  tde.undo();
  tde.moveCursor(true /*relLine*/, +1, true /*relCol*/, -2);
  chars(tde, "now on third");
  expect(tde, 2,12, "abce\n"
                    "This is the second line.\n"
                    "now on third");

  tde.undo();
  tde.undo();
  tde.undo();
  expect(tde, 2,9,  "abce\n"
                    "This is the second line.\n"
                    "now on th");

  tde.redo();
  tde.moveCursor(true /*relLine*/, +0, true /*relCol*/, +1);
  expect(tde, 2,10, "abce\n"
                    "This is the second line.\n"
                    "now on thi");

  tde.redo();
  tde.moveCursor(true /*relLine*/, +0, true /*relCol*/, +1);
  expect(tde, 2,11, "abce\n"
                    "This is the second line.\n"
                    "now on thir");

  tde.deleteLR(true /*left*/, 6);
  expect(tde, 2,5,  "abce\n"
                    "This is the second line.\n"
                    "now o");

  chars(tde, "z");
  expect(tde, 2,6,  "abce\n"
                    "This is the second line.\n"
                    "now oz");

  tde.undo();
  tde.undo();
  tde.moveCursor(true /*relLine*/, +0, true /*relCol*/, +6);
  expect(tde, 2,11, "abce\n"
                    "This is the second line.\n"
                    "now on thir");
  //tde.printHistory();

  tde.beginUndoGroup();
  chars(tde, "abc");
  tde.endUndoGroup();
  expect(tde, 2,14, "abce\n"
                    "This is the second line.\n"
                    "now on thirabc");

  tde.undo();
  expect(tde, 2,11, "abce\n"
                    "This is the second line.\n"
                    "now on thir");

  tde.beginUndoGroup();
  chars(tde, "y");
  tde.endUndoGroup();
  expect(tde, 2,12, "abce\n"
                    "This is the second line.\n"
                    "now on thiry");

  tde.undo();
  expect(tde, 2,11, "abce\n"
                    "This is the second line.\n"
                    "now on thir");

  //tde.printHistory();
  //tde.printHistoryStats();

  unlink("td.tmp");
}


// --------------------- testTextManipulation -----------------------
// test TextDocumentEditor::getTextRange
static void testGetRange(TextDocumentEditor &tde, int line1, int col1,
                         int line2, int col2, char const *expect)
{
  tde.selfCheck();

  string actual = tde.getTextRange(TextCoord(line1, col1), TextCoord(line2, col2));
  if (!actual.equals(expect)) {
    tde.core().dumpRepresentation();
    cout << "getTextRange(" << line1 << "," << col1 << ", "
                            << line2 << "," << col2 << "):\n";
    cout << "  actual: " << quoted(actual) << "\n";
    cout << "  expect: " << quoted(expect) << "\n";
    xfailure("testGetRange failed");
  }
}


// Test findString.
static void testFind(TextDocumentEditor const &tde, int line, int col,
                     char const *text, int ansLine, int ansCol,
                     TextDocumentEditor::FindStringFlags flags)
{
  tde.selfCheck();

  bool expect = ansLine>=0;
  bool actual;
  {
    TextCoord tc(line, col);
    actual = tde.findString(tc, text, flags);
    line = tc.line;
    col = tc.column;
  }

  if (expect != actual) {
    cout << "find(\"" << text << "\"): expected " << expect
         << ", got " << actual << endl;
    xfailure("testFind failed");
  }

  if (actual && (line!=ansLine || col!=ansCol)) {
    cout << "find(\"" << text << "\"): expected " << ansLine << ":" << ansCol
         << ", got " << line << ":" << col << endl;
    xfailure("testFind failed");
  }
}


static void testTextManipulation()
{
  TextDocumentAndEditor tde;

  tde.insertNulTermText("foo\nbar\n");
    // result: foo\n
    //         bar\n
  xassert(tde.cursor() == TextCoord(2, 0));
  xassert(tde.numLines() == 3);    // so final 'line' is valid

  testGetRange(tde, 0,0, 2,0, "foo\nbar\n");
  testGetRange(tde, 0,1, 2,0, "oo\nbar\n");
  testGetRange(tde, 0,1, 1,3, "oo\nbar");
  testGetRange(tde, 0,3, 1,3, "\nbar");
  testGetRange(tde, 1,0, 1,3, "bar");
  testGetRange(tde, 1,2, 1,3, "r");
  testGetRange(tde, 1,3, 1,3, "");

  tde.setCursor(TextCoord(0, 1));
  tde.insertNulTermText("arf\ngak");
    // result: farf\n
    //         gakoo\n
    //         bar\n
  xassert(tde.cursor() == TextCoord(1, 3));
  xassert(tde.numLines() == 4);
  testGetRange(tde, 0,0, 3,0, "farf\ngakoo\nbar\n");

  tde.insertNewline();
    // result: farf\n
    //         gak\n
    //         oo\n
    //         bar\n
  xassert(tde.cursor() == TextCoord(2, 0));
  xassert(tde.numLines() == 5);
  testGetRange(tde, 0,0, 4,0, "farf\ngak\noo\nbar\n");

  // some ranges that go beyond the defined area
  testGetRange(tde, 0,0, 5,0, "farf\ngak\noo\nbar\n\n");
  testGetRange(tde, 0,0, 6,0, "farf\ngak\noo\nbar\n\n\n");
  testGetRange(tde, 0,0, 6,2, "farf\ngak\noo\nbar\n\n\n  ");

  testGetRange(tde, 0,0, 2,5, "farf\ngak\noo   ");
  testGetRange(tde, 0,5, 2,5, "\ngak\noo   ");
  testGetRange(tde, 2,5, 2,10, "     ");
  testGetRange(tde, 2,10, 2,10, "");
  testGetRange(tde, 12,5, 12,10, "     ");
  testGetRange(tde, 12,5, 14,5, "\n\n     ");

  tde.deleteTextRange(TextCoord(1,1), TextCoord(1,2));
    // result: farf\n
    //         gk\n
    //         oo\n
    //         bar\n
  testGetRange(tde, 0,0, 4,0, "farf\ngk\noo\nbar\n");
  xassert(tde.numLines() == 5);

  tde.deleteTextRange(TextCoord(0,3), TextCoord(1,1));
    // result: fark\n
    //         oo\n
    //         bar\n
  testGetRange(tde, 0,0, 3,0, "fark\noo\nbar\n");
  xassert(tde.numLines() == 4);

  tde.deleteTextRange(TextCoord(1,3), TextCoord(1,5));   // nop
    // result: fark\n
    //         oo\n
    //         bar\n
  testGetRange(tde, 0,0, 3,0, "fark\noo\nbar\n");
  xassert(tde.numLines() == 4);

  tde.deleteTextRange(TextCoord(2,2), TextCoord(6,4));
    // result: fark\n
    //         oo\n
    //         ba
  testGetRange(tde, 0,0, 2,2, "fark\noo\nba");
  xassert(tde.numLines() == 3);

  tde.deleteTextRange(TextCoord(1,2), TextCoord(2,2));
    // result: fark\n
    //         oo
  testGetRange(tde, 0,0, 1,2, "fark\noo");
  xassert(tde.numLines() == 2);

  tde.deleteTextRange(TextCoord(1,0), TextCoord(1,2));
    // result: fark\n
  testGetRange(tde, 0,0, 1,0, "fark\n");
  xassert(tde.numLines() == 2);

  tde.deleteTextRange(TextCoord(0,0), TextCoord(1,0));
    // result: <empty>
  testGetRange(tde, 0,0, 0,0, "");
  xassert(tde.numLines() == 1);
  xassert(tde.lineLength(0) == 0);


  TextDocumentEditor::FindStringFlags
    none = TextDocumentEditor::FS_NONE,
    insens = TextDocumentEditor::FS_CASE_INSENSITIVE,
    back = TextDocumentEditor::FS_BACKWARDS,
    advance = TextDocumentEditor::FS_ADVANCE_ONCE,
    oneLine = TextDocumentEditor::FS_ONE_LINE;

  tde.setCursor(TextCoord(0,0));
  tde.insertNulTermText("foofoofbar\n"
                        "ooFoo arg\n");
  testFind(tde, 0,0, "foo", 0,0, none);
  testFind(tde, 0,1, "foo", 0,3, none);
  testFind(tde, 0,3, "foof", 0,3, none);
  testFind(tde, 0,4, "foof", -1,-1, none);
  testFind(tde, 0,0, "foofgraf", -1,-1, none);

  testFind(tde, 0,7, "foo", -1,-1, none);
  testFind(tde, 0,7, "foo", 1,2, insens);
  testFind(tde, 0,0, "foo", 0,3, advance);
  testFind(tde, 0,2, "foo", 0,0, back);
  testFind(tde, 0,3, "foo", 0,0, back|advance);
  testFind(tde, 0,4, "foo", 0,3, back|advance);
  testFind(tde, 1,3, "foo", 0,3, back);
  testFind(tde, 1,3, "foo", 1,2, back|insens);
  testFind(tde, 1,2, "foo", 0,3, back|insens|advance);
  testFind(tde, 1,3, "goo", -1,-1, back|insens|advance);
  testFind(tde, 1,3, "goo", -1,-1, back|insens);
  testFind(tde, 1,3, "goo", -1,-1, back);
  testFind(tde, 1,3, "goo", -1,-1, none);

  testFind(tde, 0,0, "arg", 1,6, none);
  testFind(tde, 0,0, "arg", -1,-1, oneLine);

  // Search that starts beyond EOL.
  testFind(tde, 0,20, "arg", 1,6, none);
  testFind(tde, 0,20, "arg", 1,6, advance);
}


static void testFindInLongLine()
{
  TextDocumentAndEditor tde;

  // This test kills the mutant in 'findString' that arises from
  // removing the call to 'ensureIndexDoubler'.  Without that call,
  // the subsequent 'getLine' overwrites an array bounds,
  // corrupting memory.  Of course, whether that is detected depends
  // on details of the C library, among other things, but fortunately
  // it seems to be reliably detected under mingw at least.
  tde.insertNulTermText(
    "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxZZZ\n");
  testFind(tde, 0,0, "ZZZ", 0,60, TextDocumentEditor::FS_NONE);
}


// --------------------- testBlockIndent -----------------------
// Expect, including that the mark is inactive.
static void expectNM(TextDocumentEditor const &tde, int line, int col, char const *text)
{
  expect(tde, line, col, text);
  xassert(!tde.markActive());
}


static void expectMark(TextDocumentEditor const &tde, int line, int col)
{
  xassert(tde.markActive());
  checkCoord(TextCoord(line, col), tde.mark(), "mark");
}

// Expect, and mark is active.
static void expectM(TextDocumentEditor const &tde,
  int cursorLine, int cursorCol,
  int markLine, int markCol,
  char const *text)
{
  expect(tde, cursorLine, cursorCol, text);
  expectMark(tde, markLine, markCol);
}


static void expectBlockIndent(
  TextDocumentEditor &tde,
  int amt,
  int cursorLine, int cursorCol,
  int markLine, int markCol,
  char const *expectText)
{
  tde.setCursor(TextCoord(cursorLine, cursorCol));
  tde.setMark(TextCoord(markLine, markCol));
  tde.blockIndent(amt);
  expectM(tde, cursorLine, cursorCol, markLine, markCol, expectText);
}


static void testBlockIndent()
{
  TextDocumentAndEditor tde;

  // Starter text.  Use 'insertString' for this one just to exercise it.
  tde.insertString(string(
    "one\n"
    "two\n"
    "three\n"));
  expectNM(tde, 3,0,
    "one\n"
    "two\n"
    "three\n");

  tde.setMark(TextCoord(1, 0));
  expectM(tde, 3,0, 1,0,
    "one\n"
    "two\n"
    "three\n");

  tde.blockIndent(+2);
  expectM(tde, 3,0, 1,0,
    "one\n"
    "  two\n"
    "  three\n");

  expectBlockIndent(tde, +2, 1,0, 2,0,
    "one\n"
    "    two\n"
    "  three\n");

  expectBlockIndent(tde, -2, 0,0, 3,0,
    "one\n"
    "  two\n"
    "three\n");

  expectBlockIndent(tde, -2, 0,0, 3,0,
    "one\n"
    "two\n"
    "three\n");

  expectBlockIndent(tde, -2, 0,0, 3,0,
    "one\n"
    "two\n"
    "three\n");

  expectBlockIndent(tde, +2, 0,0, 3,0,
    "  one\n"
    "  two\n"
    "  three\n");

  expectBlockIndent(tde, +2, 0,3, 1,3,
    "    one\n"
    "    two\n"
    "  three\n");

  expectBlockIndent(tde, +2, 1,0, 2,5,
    "    one\n"
    "      two\n"
    "    three\n");

  expectBlockIndent(tde, -2, 0,1, 0,2,
    "  one\n"
    "      two\n"
    "    three\n");

  tde.clearMark();
  expectNM(tde, 0,1,
    "  one\n"
    "      two\n"
    "    three\n");

  tde.blockIndent(+2);     // no-op, mark not active
  expectNM(tde, 0,1,
    "  one\n"
    "      two\n"
    "    three\n");

  // Selection goes beyond EOF; extra ignored.
  expectBlockIndent(tde, -2, 2,5, 5,2,
    "  one\n"
    "      two\n"
    "  three\n");

  // Test 'insertNewline' while beyond EOL.
  tde.clearMark();
  tde.setCursor(TextCoord(1, 40));
  tde.insertNewline();
  expectNM(tde, 2,0,
    "  one\n"
    "      two\n"
    "\n"
    "  three\n");

  // Test 'insertSpaces'.
  tde.insertSpaces(2);
  expectNM(tde, 2,2,
    "  one\n"
    "      two\n"
    "  \n"
    "  three\n");

  // Test block indent entirely beyond EOF.
  expectBlockIndent(tde, +2, 5,0, 5,2,
    "  one\n"
    "      two\n"
    "  \n"
    "  three\n");

  // Test 'getSelectedText'.
  tde.clearMark();
  xassert(tde.getSelectedText() == "");
  tde.setCursor(TextCoord(0,3));
  tde.setMark(TextCoord(1,7));
  xassert(tde.getSelectedText() == "ne\n      t");

  // Test 'insertNewline' while beyond EOF.
  tde.clearMark();
  tde.setCursor(TextCoord(6,6));
  tde.insertNewline();
  expectNM(tde, 7,0,
    "  one\n"
    "      two\n"
    "  \n"
    "  three\n"
    "\n"
    "\n"
    "\n");
}


static void testBlockIndent2()
{
  // Test block indent with blank lines.  Should not add spaces to them.
  TextDocumentAndEditor tde;
  tde.insertNulTermText(
    "one\n"
    "\n"
    "three\n");
  expectBlockIndent(tde, +2, 0,0, 3,0,
    "  one\n"
    "\n"
    "  three\n");

  // Meanwhile, when there is a line that only has spaces on it, and
  // we unindent, that should remove spaces.
  tde.setCursor(TextCoord(3,0));
  tde.insertNulTermText("  \n");
  expectBlockIndent(tde, -1, 0,0, 4,0,
    " one\n"
    "\n"
    " three\n"
    " \n");      // one space now
}


// --------------------- testFillToCursor -----------------------
static void expectFillToCursor(
  TextDocumentEditor &tde,
  int cursorLine, int cursorCol,
  char const *expectText)
{
  tde.setCursor(TextCoord(cursorLine, cursorCol));
  tde.fillToCursor();
  expect(tde, cursorLine, cursorCol, expectText);
}


static void testFillToCursor()
{
  TextDocumentAndEditor tde;

  tde.insertNulTermText(
    "one\n"
    "two\n"
    "three\n");
  expect(tde, 3,0,
    "one\n"
    "two\n"
    "three\n");

  expectFillToCursor(tde, 3,0,
    "one\n"
    "two\n"
    "three\n");

  expectFillToCursor(tde, 1,5,
    "one\n"
    "two  \n"
    "three\n");

  expectFillToCursor(tde, 1,5,
    "one\n"
    "two  \n"
    "three\n");

  expectFillToCursor(tde, 5,0,
    "one\n"
    "two  \n"
    "three\n"
    "\n"
    "\n");

  expectFillToCursor(tde, 5,3,
    "one\n"
    "two  \n"
    "three\n"
    "\n"
    "\n"
    "   ");

  expectFillToCursor(tde, 4,5,
    "one\n"
    "two  \n"
    "three\n"
    "\n"
    "     \n"
    "   ");
}


// --------------------- testScrollToCursor -----------------------
// Check firstVisible and cursor.  The text itself is ignored since
// we assume that tests above have exercised that adequately.
static void expectFV(TextDocumentEditor const &tde,
  int cursorLine, int cursorCol,
  int fvLine, int fvCol,
  int visLines, int visColumns)
{
  tde.selfCheck();

  checkCoord(TextCoord(cursorLine, cursorCol), tde.cursor(), "cursor");
  checkCoord(TextCoord(fvLine, fvCol), tde.firstVisible(), "firstVisible");
  xassert(visLines == tde.visLines());
  xassert(visColumns == tde.visColumns());
}

static void testScrollToCursor()
{
  TextDocumentAndEditor tde;
  tde.setVisibleSize(5, 10);

  xassert(tde.cursorAtEnd() == true);

  // Starter text.
  tde.insertNulTermText(
    "one\n"
    "two\n"
    "three\n");
  expectFV(tde, 3,0, 0,0, 5,10);

  xassert(tde.cursorAtEnd() == true);

  // Insert a test for getSelectRegion with mark inactive.
  {
    TextCoord tc1, tc2;
    tde.getSelectRegion(tc1, tc2);
    xassert(tc1 == TextCoord(3, 0));
    xassert(tc2 == TextCoord(3, 0));
  }

  // Add enough text to start scrolling vertically.
  tde.insertNulTermText(
    "four\n"
    "five\n");
  expectFV(tde, 5,0, 1,0, 5,10);

  // Now make it scroll to the right.
  tde.insertNulTermText("six 1234567890");
  expectFV(tde, 5,14, 1,5, 5,10);

  // And back to the left.
  tde.insertNulTermText("\n");
  expectFV(tde, 6,0, 2,0, 5,10);

  xassert(tde.cursorAtEnd() == true);

  // Put the cursor beyond EOF.
  tde.setCursor(TextCoord(6, 20));
  expectFV(tde, 6,20, 2,0, 5,10);      // did not scroll yet
  tde.scrollToCursor();
  expectFV(tde, 6,20, 2,11, 5,10);

  // Test with edgeGap > 0.
  tde.scrollToCursor(1 /*edgeGap*/);
  expectFV(tde, 6,20, 3,12, 5,10);

  xassert(tde.cursorAtEnd() == false); // beyond end

  // Back to the start with edgeGap>0, which will have no effect.
  tde.setCursor(TextCoord(0,0));
  tde.scrollToCursor(1 /*edgeGap*/);
  expectFV(tde, 0,0, 0,0, 5,10);

  xassert(tde.cursorAtEnd() == false); // at start

  // Test with -1 edgeGap.
  tde.setCursor(TextCoord(20, 20));    // offscreen
  tde.scrollToCursor(-1 /*edgeGap*/);
  expectFV(tde, 20,20, 18,15, 5,10);

  // Test with -1 and a coordinate just barely offscreen.  This kills
  // a testing mutant where, in 'stcHelper', we do not reset the gap
  // to 0 in the -1 case.
  tde.setCursor(TextCoord(17,15));     // just above FV
  expectFV(tde, 17,15, 18,15, 5,10);
  tde.scrollToCursor(-1 /*edgeGap*/);
  expectFV(tde, 17,15, 15,15, 5,10);

  // Test 'moveCursor' with relLine=false.
  tde.moveCursor(false /*relLine*/, 3, false /*relCol*/, 0);
  tde.scrollToCursor();
  expectFV(tde, 3,0, 3,0, 5,10);

  // Test 'moveFirstVisibleBy'.
  tde.moveFirstVisibleBy(0, +1);
  expectFV(tde, 3,0, 3,1, 5,10);
  tde.moveFirstVisibleBy(+1, 0);
  expectFV(tde, 3,0, 4,1, 5,10);
  tde.moveFirstVisibleBy(-3, -3);
  expectFV(tde, 3,0, 1,0, 5,10);
  tde.moveFirstVisibleBy(-3, -3);
  expectFV(tde, 3,0, 0,0, 5,10);

  // Test 'moveFirstVisibleAndCursor'.
  tde.setFirstVisible(TextCoord(10,10));
  expectFV(tde, 3,0, 10,10, 5,10);
  tde.moveFirstVisibleAndCursor(0, +1);    // scroll to cursor, then shift right
  expectFV(tde, 3,1, 3,1, 5,10);
  tde.setCursor(TextCoord(4,2));           // one in from left/top
  expectFV(tde, 4,2, 3,1, 5,10);
  tde.moveFirstVisibleAndCursor(+2, +1);
  expectFV(tde, 6,3, 5,2, 5,10);
  tde.moveFirstVisibleAndCursor(0, -10);   // hit left edge
  expectFV(tde, 6,1, 5,0, 5,10);
  tde.moveFirstVisibleAndCursor(-10, 0);   // hit top edge
  expectFV(tde, 1,1, 0,0, 5,10);

  // Test 'centerVisibleOnCursorLine'.
  tde.centerVisibleOnCursorLine();         // no-op
  expectFV(tde, 1,1, 0,0, 5,10);
  tde.setCursor(TextCoord(50,50));
  tde.centerVisibleOnCursorLine();         // cursor at right edge
  expectFV(tde, 50,50, 48,41, 5,10);
  tde.setCursor(TextCoord(5,1));
  tde.centerVisibleOnCursorLine();         // back near top-left
  expectFV(tde, 5,1, 3,0, 5,10);
}


// ---------------------- testGetWordAfter ----------------------
static void testOneWordAfter(TextDocumentEditor &tde,
  int line, int col, char const *expect)
{
  string actual = tde.getWordAfter(TextCoord(line, col));
  xassert(actual == expect);
}


static void testGetWordAfter()
{
  TextDocumentAndEditor tde;
  tde.insertNulTermText(
    "one\n"
    "two three\n"
    "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_\n"
    "x.x,x%x(x)--x\n");

  testOneWordAfter(tde, -1,0, "");
  testOneWordAfter(tde, 11,0, "");

  testOneWordAfter(tde, 0,0, "one");

  testOneWordAfter(tde, 1,0, "two");
  testOneWordAfter(tde, 1,3, " three");
  testOneWordAfter(tde, 1,4, "three");
  testOneWordAfter(tde, 1,5, "hree");

  testOneWordAfter(tde, 2,0,
    "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_");

  testOneWordAfter(tde, 3,0, "x");
  testOneWordAfter(tde, 3,1, ".x");
  testOneWordAfter(tde, 3,2, "x");
  testOneWordAfter(tde, 3,4, "x");
  testOneWordAfter(tde, 3,6, "x");
  testOneWordAfter(tde, 3,8, "x");
  testOneWordAfter(tde, 3,12, "x");
}


// ------------------ testGetAboveIndentation -------------------
static void testOneGAI(TextDocumentEditor &tde, int line, int expect)
{
  int actual = tde.getAboveIndentation(line);
  xassert(actual == expect);
}

static void testGetAboveIndentation()
{
  TextDocumentAndEditor tde;
  tde.insertNulTermText(
    "\n"                               // line 0
    "\n"
    "  hi\n"
    "\n"
    "    there\n"
    "this\n"                           // line 5
    "is\n"
    "  some\n"
    "  indented\n"
    "\n"
    "    text\n");                     // line 10

  testOneGAI(tde, -1, 0);
  testOneGAI(tde, 0, 0);
  testOneGAI(tde, 1, 0);
  testOneGAI(tde, 2, 2);
  testOneGAI(tde, 3, 2);
  testOneGAI(tde, 4, 4);
  testOneGAI(tde, 5, 0);
  testOneGAI(tde, 6, 0);
  testOneGAI(tde, 7, 2);
  testOneGAI(tde, 8, 2);
  testOneGAI(tde, 9, 2);
  testOneGAI(tde, 10, 4);
  testOneGAI(tde, 11, 4);
  testOneGAI(tde, 12, 4);
  testOneGAI(tde, 13, 4);
}


// ---------------------- testMoveCursor ------------------------
static void testMoveCursor()
{
  TextDocumentAndEditor tde;
  tde.insertNulTermText(
    "1\n"
    "two\n"
    "three\n");
  expectCursor(tde, 3,0);

  // Test 'moveCursorBy'.
  tde.moveCursorBy(-1, +1);
  expectCursor(tde, 2,1);

  // Test 'setCursorColumn'.
  tde.setCursorColumn(4);
  expectCursor(tde, 2,4);

  // Test 'moveToPrevLineEnd'.
  tde.moveToPrevLineEnd();
  expectCursor(tde, 1,3);
  tde.moveToPrevLineEnd();
  expectCursor(tde, 0,1);

  // Test 'moveToNextLineStart'.
  tde.moveToNextLineStart();
  expectCursor(tde, 1,0);
  tde.moveToNextLineStart();
  expectCursor(tde, 2,0);
  tde.moveToNextLineStart();
  tde.moveToNextLineStart();           // Test beyond EOF.
  tde.moveToNextLineStart();
  expectCursor(tde, 5,0);

  // Now come back from EOF using 'moveToPrevLineEnd'.
  tde.moveToPrevLineEnd();
  expectCursor(tde, 4,0);
  tde.moveToPrevLineEnd();
  tde.moveToPrevLineEnd();
  tde.moveToPrevLineEnd();
  tde.moveToPrevLineEnd();
  expectCursor(tde, 0,1);
  tde.moveToPrevLineEnd();             // Bump up against BOF.
  expectCursor(tde, 0,1);

  // Test 'selectCursorLine'.
  tde.selectCursorLine();
  expectCursor(tde, 0,0); expectMark(tde, 1,0);
  tde.setCursor(TextCoord(44,44));
  tde.selectCursorLine();
  expectCursor(tde, 44,0); expectMark(tde, 45,0);

  // Test 'advanceWithWrap'.
  tde.advanceWithWrap(false /*backwards*/);
  expectCursor(tde, 45,0);
  tde.advanceWithWrap(true /*backwards*/);
  expectCursor(tde, 44,0);

  tde.setCursor(TextCoord(1,1));
  tde.advanceWithWrap(false /*backwards*/);
  expectCursor(tde, 1,2);
  tde.advanceWithWrap(false /*backwards*/);
  expectCursor(tde, 1,3);
  tde.advanceWithWrap(false /*backwards*/);
  expectCursor(tde, 2,0);
  tde.advanceWithWrap(true /*backwards*/);
  expectCursor(tde, 1,3);

  tde.setCursor(TextCoord(1, 45));
  tde.advanceWithWrap(false /*backwards*/);
  expectCursor(tde, 2,0);

  tde.setCursor(TextCoord(1, 45));
  tde.advanceWithWrap(true /*backwards*/);
  expectCursor(tde, 1,44);

  tde.setCursor(TextCoord(0, 0));
  tde.advanceWithWrap(true /*backwards*/);
  expectCursor(tde, 0,0);

  // Test 'moveCursorToTop/Bottom' with a tiny window.
  tde.setVisibleSize(2,2);
  tde.moveCursorToBottom();
  expectFV(tde, 3,0, 2,0, 2,2);
  tde.moveCursorToTop();
  expectFV(tde, 0,0, 0,0, 2,2);

  // Test 'moveCursorBy' attempting to move to negative values.
  tde.moveCursorBy(-1, -1);
  expectCursor(tde, 0,0);
}


// ------------------- testBackspaceFunction --------------------
static void testBackspaceFunction()
{
  TextDocumentAndEditor tde;
  tde.insertNulTermText(
    "one\n"
    "two  \n"
    "three\n");
  expect(tde, 3,0,
    "one\n"
    "two  \n"
    "three\n");

  // Backspace the final newline.
  tde.backspaceFunction();
  expect(tde, 2,5,
    "one\n"
    "two  \n"
    "three");

  // Backspace selected text.
  tde.setMark(TextCoord(0,1));
  tde.setCursor(TextCoord(0,2));
  tde.backspaceFunction();
  expect(tde, 0,1,
    "oe\n"
    "two  \n"
    "three");

  // Backspace the first character.
  tde.backspaceFunction();
  expect(tde, 0,0,
    "e\n"
    "two  \n"
    "three");

  // Backspace at top: no-op.
  tde.backspaceFunction();
  expect(tde, 0,0,
    "e\n"
    "two  \n"
    "three");

  // Backspace beyond EOF: move up.
  tde.setCursor(TextCoord(4,0));
  tde.backspaceFunction();
  expect(tde, 3,0,
    "e\n"
    "two  \n"
    "three");

  // Backspace at left edge to join two lines.
  tde.setCursor(TextCoord(1,0));
  tde.backspaceFunction();
  expect(tde, 0,1,
    "etwo  \n"
    "three");

  // Backspace beyond EOL: move left.
  tde.setCursor(TextCoord(0,7));
  tde.backspaceFunction();
  expect(tde, 0,6,
    "etwo  \n"
    "three");

  // Backspace at EOL: delete left.
  tde.backspaceFunction();
  expect(tde, 0,5,
    "etwo \n"
    "three");

  // Scroll induced by backspace.
  tde.setCursor(TextCoord(1,0));
  tde.setFirstVisible(TextCoord(1,0));
  tde.backspaceFunction();
  expect(tde, 0,5,
    "etwo three");
  checkCoord(TextCoord(0,0), tde.firstVisible(), "first visible");
}


// ------------------- testDeleteKeyFunction --------------------
static void testDeleteKeyFunction()
{
  TextDocumentAndEditor tde;
  tde.setVisibleSize(5, 10);
  tde.insertNulTermText(
    "one\n"
    "two  \n"
    "three\n");
  expect(tde, 3,0,
    "one\n"
    "two  \n"
    "three\n");

  // Delete at EOF: no-op.
  tde.deleteKeyFunction();
  expect(tde, 3,0,
    "one\n"
    "two  \n"
    "three\n");

  // Delete with selection.
  tde.setMark(TextCoord(0,1));
  tde.setCursor(TextCoord(0,2));
  tde.deleteKeyFunction();
  expect(tde, 0,1,
    "oe\n"
    "two  \n"
    "three\n");

  // Delete beyond EOL: fill then splice.
  tde.setCursor(TextCoord(1, 10));
  tde.deleteKeyFunction();
  expect(tde, 1,10,
    "oe\n"
    "two       three\n");

  // Delete well beyond EOF: no-op.
  tde.setCursor(TextCoord(10, 10));
  tde.deleteKeyFunction();
  expect(tde, 10,10,
    "oe\n"
    "two       three\n");

  // Selection that is partly offscreen such that after
  // deletion scrolling changes visible region.
  tde.setCursor(TextCoord(1, 10));
  tde.setMark(TextCoord(1, 0));
  tde.setFirstVisible(TextCoord(1, 10));
  tde.deleteSelection();
  expectNM(tde, 1,0,
    "oe\n"
    "three\n");
  checkCoord(TextCoord(1,0), tde.firstVisible(), "first visible");
}


// ---------------------- testClipboard -------------------------
static void testClipboard()
{
  TextDocumentAndEditor tde;
  tde.insertNulTermText(
    "one\n"
    "two  \n"
    "three\n");

  // Try with empty strings.
  xassert(tde.clipboardCopy().isempty());
  xassert(tde.clipboardCut().isempty());
  tde.clipboardPaste("", 0);
  expectNM(tde, 3,0,
    "one\n"
    "two  \n"
    "three\n");

  // Copy.
  tde.setCursor(TextCoord(0,1));
  tde.setMark(TextCoord(1,2));
  xassert(tde.clipboardCopy() == "ne\ntw");
  expectNM(tde, 0,1,
    "one\n"
    "two  \n"
    "three\n");

  // Cut with cursor ahead of mark.
  tde.setCursor(TextCoord(2,4));
  tde.setMark(TextCoord(2,2));
  xassert(tde.clipboardCut() == "re");
  expectNM(tde, 2,2,
    "one\n"
    "two  \n"
    "the\n");

  // Paste with nothing selected.
  tde.clipboardPaste("ab\nc", 4);
  expectNM(tde, 3,1,
    "one\n"
    "two  \n"
    "thab\n"
    "ce\n");

  // Paste, overwriting a selection.
  tde.setMark(TextCoord(1,2));
  tde.clipboardPaste("xyz", 3);
  expectNM(tde, 1,5,
    "one\n"
    "twxyze\n");

  // Paste while beyond EOL.
  tde.setCursor(TextCoord(0, 5));
  tde.clipboardPaste("123", 3);
  expectNM(tde, 0,8,
    "one  123\n"
    "twxyze\n");
}


// ---------------- testInsertNewlineAutoIndent ------------------
static void testInsertNewlineAutoIndent()
{
  TextDocumentAndEditor tde;
  tde.insertNulTermText(
    "one\n"
    "two  \n"
    "three\n");

  // Adding to EOF.
  tde.insertNewlineAutoIndent();
  expectNM(tde, 4,0,
    "one\n"
    "two  \n"
    "three\n"
    "\n");

  // Enter at left edge, middle of document.
  tde.setCursor(TextCoord(2, 0));
  tde.insertNewlineAutoIndent();
  expectNM(tde, 3,0,
    "one\n"
    "two  \n"
    "\n"
    "three\n"
    "\n");

  // Enter to break a line.
  tde.setCursor(TextCoord(3, 2));
  tde.insertNewlineAutoIndent();
  expectNM(tde, 4,0,
    "one\n"
    "two  \n"
    "\n"
    "th\n"
    "ree\n"
    "\n");

  // Not adding extra spaces when beyond EOL.
  tde.setCursor(TextCoord(1, 10));
  tde.insertNewlineAutoIndent();
  expectNM(tde, 2,0,
    "one\n"
    "two  \n"
    "\n"
    "\n"
    "th\n"
    "ree\n"
    "\n");
}


// Like above, but with some indented lines.
static void testInsertNewlineAutoIndent2()
{
  TextDocumentAndEditor tde;
  tde.setVisibleSize(3, 3);
  tde.insertNulTermText(
    "  one\n"
    "   two  \n"
    " three\n");

  // Adding to EOF.
  tde.insertNewlineAutoIndent();
  expectNM(tde, 4,1,
    "  one\n"
    "   two  \n"
    " three\n"
    "\n");

  // Enter at left edge, middle of document.
  tde.setCursor(TextCoord(2, 0));
  tde.insertNewlineAutoIndent();
  expectNM(tde, 3,3,
    "  one\n"
    "   two  \n"
    "\n"
    "    three\n"
    "\n");

  // Enter to break a line.
  tde.setCursor(TextCoord(3, 6));
  tde.insertNewlineAutoIndent();
  expectNM(tde, 4,4,
    "  one\n"
    "   two  \n"
    "\n"
    "    th\n"
    "    ree\n"
    "\n");

  // Not adding extra spaces when beyond EOL.
  tde.setCursor(TextCoord(1, 10));
  tde.insertNewlineAutoIndent();
  expectNM(tde, 2,3,
    "  one\n"
    "   two  \n"
    "\n"
    "\n"
    "    th\n"
    "    ree\n"
    "\n");

  // Enter while on blank line beyond EOL below indented line.
  tde.insertNewlineAutoIndent();
  expectNM(tde, 3,3,
    "  one\n"
    "   two  \n"
    "\n"
    "\n"
    "\n"
    "    th\n"
    "    ree\n"
    "\n");

  // Make sure we scroll, including checking that we can see the
  // indented cursor even if that means not seeing the left edge.
  tde.insertNewlineAutoIndent();
  tde.insertNewlineAutoIndent();
  tde.insertNewlineAutoIndent();
  expectNM(tde, 6,3,
    "  one\n"
    "   two  \n"
    "\n"
    "\n"
    "\n"
    "\n"
    "\n"
    "\n"
    "    th\n"
    "    ree\n"
    "\n");
  expectFV(tde, 6,3, 4,1, 3,3);

  // Hit Enter while something is selected.
  tde.setMark(TextCoord(2,0));
  tde.setCursor(TextCoord(8,4));
  tde.insertNewlineAutoIndent();
  expectNM(tde, 3,3,
    "  one\n"
    "   two  \n"
    "\n"
    "   th\n"
    "    ree\n"
    "\n");

  // Do the above again but with cursor and mark swapped;
  // result should be the same.
  tde.deleteTextRange(TextCoord(0,0), tde.endCoord());
  tde.setCursor(TextCoord(0,0));
  tde.insertNulTermText(
    "  one\n"
    "   two  \n"
    "\n"
    "\n"
    "\n"
    "\n"
    "\n"
    "\n"
    "    th\n"
    "    ree\n"
    "\n");
  tde.setCursor(TextCoord(2,0));
  tde.setMark(TextCoord(8,4));
  tde.insertNewlineAutoIndent();
  expectNM(tde, 3,3,
    "  one\n"
    "   two  \n"
    "\n"
    "   th\n"
    "    ree\n"
    "\n");
}


static void testInsertNewlineAutoIndent3()
{
  TextDocumentAndEditor tde;
  tde.insertNulTermText(
    "one\n"
    "two\n");

  // Hit Enter while beyond EOF.
  tde.setCursor(TextCoord(4,0));
  tde.insertNewlineAutoIndent();
  expectNM(tde, 5,0,
    "one\n"
    "two\n"
    "\n"
    "\n"
    "\n");
  tde.undo();

  // Now with selected text, entirely beyond EOF.
  tde.setMark(TextCoord(4,0));
  tde.setCursor(TextCoord(4,4));
  tde.insertNewlineAutoIndent();
  expectNM(tde, 5,0,
    "one\n"
    "two\n"
    "\n"
    "\n"
    "\n");
  tde.undo();

  // Selected text straddling EOF.
  tde.setMark(TextCoord(1,1));
  tde.setCursor(TextCoord(4,4));
  tde.insertNewlineAutoIndent();
  expectNM(tde, 2,0,
    "one\n"
    "t\n");
  tde.undo();
}


static void testInsertNewlineAutoIndent4()
{
  TextDocumentAndEditor tde;
  tde.setVisibleSize(5, 10);
  tde.insertNulTermText(
    "  a\n"
    "  b\n");

  // Start with the display scrolled to the right.  It should
  // return to the left edge.
  tde.setFirstVisible(TextCoord(0,1));
  tde.setCursor(TextCoord(1,3));
  tde.insertNewlineAutoIndent();
  expectFV(tde, 2,2, 0,0, 5,10);
  expectNM(tde, 2,2,
    "  a\n"
    "  b\n"
    "\n");
}


// -------------------- testSetVisibleSize ----------------------
static void testSetVisibleSize()
{
  TextDocumentAndEditor tde;

  // Try with negative sizes.
  tde.setVisibleSize(-1, -1);
  checkCoord(TextCoord(0,0), tde.firstVisible(), "firstVisible");
  checkCoord(TextCoord(0,0), tde.lastVisible(), "lastVisible");

  // See if things work at this size.
  tde.insertNulTermText(
    "  one\n"
    "   two  \n"
    " three");
  checkCoord(TextCoord(2,6), tde.firstVisible(), "firstVisible");
  checkCoord(TextCoord(2,6), tde.lastVisible(), "lastVisible");

  // Cursor movement does not automatically scroll.
  tde.moveCursorBy(-1,0);
  checkCoord(TextCoord(2,6), tde.firstVisible(), "firstVisible");
  tde.scrollToCursor();
  checkCoord(TextCoord(1,6), tde.firstVisible(), "firstVisible");
}


// -------------------- testCursorRestorer ----------------------
static void testCursorRestorer()
{
  TextDocumentAndEditor tde;
  tde.setVisibleSize(5, 10);
  tde.insertNulTermText(
    "one\n"
    "two\n"
    "three\n");

  // Restore an active mark and a scroll position.
  tde.setMark(TextCoord(2,1));
  tde.setCursor(TextCoord(2,2));
  tde.setFirstVisible(TextCoord(1,1));
  {
    CursorRestorer restorer(tde);
    tde.clearMark();
    tde.setCursor(TextCoord(4,4));
    tde.setFirstVisible(TextCoord(0,0));
  }
  expectMark(tde, 2,1);
  expectFV(tde, 2,2, 1,1, 5,10);

  // Ensure inactive mark is restored as such.
  tde.clearMark();
  {
    CursorRestorer restorer(tde);
    tde.setMark(TextCoord(0,0));
  }
  expectNM(tde, 2,2,
    "one\n"
    "two\n"
    "three\n");
}


// ----------------------- testSetMark --------------------------
static void testSetMark()
{
  TextDocumentAndEditor tde;
  tde.insertNulTermText(
    "one\n"
    "two\n"
    "three\n");
  xassert(!tde.markActive());

  tde.setMark(TextCoord(1,1));
  expectMark(tde, 1,1);

  tde.moveMarkBy(+1,+1);
  expectMark(tde, 2,2);

  tde.moveMarkBy(+3,+4);
  expectMark(tde, 5,6);

  tde.moveMarkBy(-10,+1);
  expectMark(tde, 0,7);

  tde.moveMarkBy(0,-10);
  expectMark(tde, 0,0);

  // Test 'turnOnSelection' with mark already active.
  tde.turnOnSelection();
  expectMark(tde, 0,0);

  // Test 'turnOnSelection' with mark inactive.
  tde.clearMark();
  tde.setCursor(TextCoord(2,2));
  xassert(!tde.markActive());
  tde.turnOnSelection();
  expectMark(tde, 2,2);

  // Test 'turnOffSelectionIfEmpty' with empty selection.
  tde.turnOffSelectionIfEmpty();
  xassert(!tde.markActive());

  // Test 'turnOffSelectionIfEmpty' with inactive mark.
  tde.turnOffSelectionIfEmpty();
  xassert(!tde.markActive());

  // Test 'turnOffSelectionIfEmpty' with non-empty selection.
  tde.setMark(TextCoord(2,3));
  tde.turnOffSelectionIfEmpty();
  expectMark(tde, 2,3);
}


// ----------------- testConfineCursorToVisible -----------------
static void testConfineCursorToVisible()
{
  TextDocumentAndEditor tde;
  tde.setVisibleSize(3,3);

  // Already visible.
  tde.confineCursorToVisible();
  expectCursor(tde, 0,0);

  // Pull in from corner.
  tde.setFirstVisible(TextCoord(1,1));
  tde.confineCursorToVisible();
  expectCursor(tde, 1,1);

  // From top.
  tde.setCursor(TextCoord(0,2));
  tde.confineCursorToVisible();
  expectCursor(tde, 1,2);

  // From bottom.
  tde.setCursor(TextCoord(4,2));
  tde.confineCursorToVisible();
  expectCursor(tde, 3,2);

  // From left.
  tde.setCursor(TextCoord(2,0));
  tde.confineCursorToVisible();
  expectCursor(tde, 2,1);

  // From right.
  tde.setCursor(TextCoord(2,4));
  tde.confineCursorToVisible();
  expectCursor(tde, 2,3);
}


// --------------------------- main -----------------------------
int main()
{
  try {
    testUndoRedo();
    testTextManipulation();
    testFindInLongLine();
    testBlockIndent();
    testBlockIndent2();
    testFillToCursor();
    testScrollToCursor();
    testGetWordAfter();
    testGetAboveIndentation();
    testMoveCursor();
    testBackspaceFunction();
    testDeleteKeyFunction();
    testClipboard();
    testInsertNewlineAutoIndent();
    testInsertNewlineAutoIndent2();
    testInsertNewlineAutoIndent3();
    testInsertNewlineAutoIndent4();
    testSetVisibleSize();
    testCursorRestorer();
    testSetMark();
    testConfineCursorToVisible();

    xassert(TextDocumentEditor::s_objectCount == 0);

    malloc_stats();
    cout << "\ntest-td-editor is ok" << endl;
    return 0;
  }
  catch (xBase &x) {
    cout << x << endl;
    return 4;
  }
}


// EOF
