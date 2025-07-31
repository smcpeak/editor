// td-editor-test.cc
// Tests for td-editor module.

#include "unit-tests.h"                // decl for my entry point

#include "td-editor.h"                 // module to test

// smbase
#include "smbase/datablok.h"           // DataBlock
#include "smbase/exc.h"                // EXN_CONTEXT
#include "smbase/nonport.h"            // removeFile
#include "smbase/sm-macros.h"          // OPEN_ANONYMOUS_NAMESPACE
#include "smbase/sm-test.h"            // EXPECT_EQ, expectEq
#include "smbase/string-util.h"        // doubleQuote

// libc
#include <stdio.h>                     // printf
#include <stdlib.h>                    // system


OPEN_ANONYMOUS_NAMESPACE


// This file is structured as a sequence of mostly-independent
// sections, each focused on testing one aspect of or function in
// TextDocumentEditor.


void checkCoord(TextLCoord actual, TextLCoord expect, char const *label)
{
  expectEq(label, actual, expect);
}


void expectCursor(TextDocumentEditor const &tde, int line, int col)
{
  checkCoord(tde.cursor(), TextLCoord(line, col), "cursor");
}

void expect(TextDocumentEditor const &tde, int line, int col, char const *text)
{
  tde.selfCheck();

  expectCursor(tde, line, col);

  string expect(text);
  string actual = tde.getTextForLRangeString(tde.documentLRange());

  if (expect != actual) {
    cout << "expect: " << doubleQuote(expect) << endl;
    cout << "actual: " << doubleQuote(actual) << endl;
    xfailure("text mismatch");
  }
}


// --------------------- testUndoRedo -----------------------
// Insert each character in 'str' as its own edit action.
void chars(TextDocumentEditor &tde, char const *str)
{
  while (*str) {
    tde.insertText(str, 1);
    str++;
  }
}


void testUndoRedo()
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

  tde.deleteLRColumns(true /*left*/, 6);
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
}


// Specifically test the way undo interacts with clipboard paste.
void testUndoOfPaste()
{
  TextDocumentAndEditor tde;

  chars(tde, "one\n"
             "two\n"
             "three\n");
  expect(tde, 3,0, "one\ntwo\nthree\n");

  tde.moveCursor(true /*relLine*/, -2, true /*relCol*/, +0);
  expect(tde, 1,0, "one\ntwo\nthree\n");

  // Select line with "two".
  tde.turnOnSelection();
  tde.moveMarkBy(+1 /*detaLine*/, 0 /*deltaCol*/);

  // Replace it the way `clipboardPaste` would.
  tde.insertString("TWO\n");
  expect(tde, 2,0, "one\nTWO\nthree\n");

  // The entire replacement should be undone in one step.
  tde.undo();
  expect(tde, 1,0, "one\ntwo\nthree\n");

  // Move beyond EOF.
  tde.moveCursor(false /*relLine*/, 5, false /*relCol*/, 0);
  expect(tde, 5,0, "one\ntwo\nthree\n");

  // Paste text there, which adds fill newlines.
  tde.insertString("six\n");
  expect(tde, 6,0, "one\ntwo\nthree\n\n\nsix\n");

  // That too should all undo at once, including the fill.
  //
  // This leaves the cursor at a different place (3,0) than where we
  // started (5,0) because deleting the fill moves it, and cursor
  // movements aren't explicitly recorded in the undo history.  That's
  // perhaps not ideal, but not a big problem either.
  //
  tde.undo();
  expect(tde, 3,0, "one\ntwo\nthree\n");
}


void testUndoOfBlockIndent()
{
  TextDocumentAndEditor tde;

  chars(tde, "one\n"
             "two\n"
             "three\n"
             "four\n");
  tde.moveCursor(false /*relLine*/, 1, false /*relCol*/, 0);
  tde.turnOnSelection();
  tde.moveMarkBy(+2 /*deltaLine*/, 0 /*deltaCol*/);
  tde.blockIndent(+2);
  expect(tde, 1,0,
    "one\n"
    "  two\n"
    "  three\n"
    "four\n");

  // Undo should undo indentation of both lines.
  tde.undo();
  expect(tde, 1,0,
    "one\n"
    "two\n"
    "three\n"
    "four\n");
}


// --------------------- testTextManipulation -----------------------
// test TextDocumentEditor::getTextForLRange
void testGetRange(TextDocumentEditor &tde, int line1, int col1,
                         int line2, int col2, char const *expect)
{
  tde.selfCheck();

  string actual =
    tde.getTextForLRangeString(TextLCoord(line1, col1), TextLCoord(line2, col2));

  if (actual != expect) {
    tde.debugPrint();
    cout << "getTextForLRange(" << line1 << "," << col1 << ", "
                                << line2 << "," << col2 << "):\n";
    cout << "  actual: " << doubleQuote(actual) << "\n";
    cout << "  expect: " << doubleQuote(expect) << "\n";
    xfailure("testGetRange failed");
  }
}


void testTextManipulation()
{
  TextDocumentAndEditor tde;

  tde.insertNulTermText("foo\nbar\n");
    // result: foo\n
    //         bar\n
  xassert(tde.cursor() == TextLCoord(2, 0));
  xassert(tde.numLines() == 3);    // so final 'line' is valid

  testGetRange(tde, 0,0, 2,0, "foo\nbar\n");
  testGetRange(tde, 0,1, 2,0, "oo\nbar\n");
  testGetRange(tde, 0,1, 1,3, "oo\nbar");
  testGetRange(tde, 0,3, 1,3, "\nbar");
  testGetRange(tde, 1,0, 1,3, "bar");
  testGetRange(tde, 1,2, 1,3, "r");
  testGetRange(tde, 1,3, 1,3, "");

  tde.setCursor(TextLCoord(0, 1));
  tde.insertNulTermText("arf\ngak");
    // result: farf\n
    //         gakoo\n
    //         bar\n
  xassert(tde.cursor() == TextLCoord(1, 3));
  xassert(tde.numLines() == 4);
  testGetRange(tde, 0,0, 3,0, "farf\ngakoo\nbar\n");

  tde.insertNewline();
    // result: farf\n
    //         gak\n
    //         oo\n
    //         bar\n
  xassert(tde.cursor() == TextLCoord(2, 0));
  xassert(tde.numLines() == 5);
  testGetRange(tde, 0,0, 4,0, "farf\ngak\noo\nbar\n");

  // Some ranges that go beyond the defined area.  In the past,
  // 'getTextForLRange' would add newlines and spaces, but I have changed
  // the definition to only return bytes actually in the document.
  testGetRange(tde, 0,0, 5,0, "farf\ngak\noo\nbar\n");
  testGetRange(tde, 0,0, 6,0, "farf\ngak\noo\nbar\n");
  testGetRange(tde, 0,0, 6,2, "farf\ngak\noo\nbar\n");

  testGetRange(tde, 0,0, 2,5, "farf\ngak\noo");
  testGetRange(tde, 0,5, 2,5, "\ngak\noo");
  testGetRange(tde, 2,5, 2,10, "");
  testGetRange(tde, 2,10, 2,10, "");
  testGetRange(tde, 12,5, 12,10, "");
  testGetRange(tde, 12,5, 14,5, "");

  tde.deleteTextLRange(TextLCoord(1,1), TextLCoord(1,2));
    // result: farf\n
    //         gk\n
    //         oo\n
    //         bar\n
  testGetRange(tde, 0,0, 4,0, "farf\ngk\noo\nbar\n");
  xassert(tde.numLines() == 5);

  tde.deleteTextLRange(TextLCoord(0,3), TextLCoord(1,1));
    // result: fark\n
    //         oo\n
    //         bar\n
  testGetRange(tde, 0,0, 3,0, "fark\noo\nbar\n");
  xassert(tde.numLines() == 4);

  tde.deleteTextLRange(TextLCoord(1,3), TextLCoord(1,5));   // nop
    // result: fark\n
    //         oo\n
    //         bar\n
  testGetRange(tde, 0,0, 3,0, "fark\noo\nbar\n");
  xassert(tde.numLines() == 4);

  tde.deleteTextLRange(TextLCoord(2,2), TextLCoord(6,4));
    // result: fark\n
    //         oo\n
    //         ba
  testGetRange(tde, 0,0, 2,2, "fark\noo\nba");
  xassert(tde.numLines() == 3);

  tde.deleteTextLRange(TextLCoord(1,2), TextLCoord(2,2));
    // result: fark\n
    //         oo
  testGetRange(tde, 0,0, 1,2, "fark\noo");
  xassert(tde.numLines() == 2);

  tde.deleteTextLRange(TextLCoord(1,0), TextLCoord(1,2));
    // result: fark\n
  testGetRange(tde, 0,0, 1,0, "fark\n");
  xassert(tde.numLines() == 2);

  tde.deleteTextLRange(TextLCoord(0,0), TextLCoord(1,0));
    // result: <empty>
  testGetRange(tde, 0,0, 0,0, "");
  xassert(tde.numLines() == 1);
  xassert(tde.isEmptyLine(0));
  xassert(tde.lineLengthColumns(0) == 0);
}


// --------------------- testBlockIndent -----------------------
// Expect, including that the mark is inactive.
void expectNM(TextDocumentEditor const &tde, int line, int col, char const *text)
{
  expect(tde, line, col, text);
  xassert(!tde.markActive());
}


void expectMark(TextDocumentEditor const &tde, int line, int col)
{
  xassert(tde.markActive());
  checkCoord(tde.mark(), TextLCoord(line, col), "mark");
}

// Expect, and mark is active.
void expectM(TextDocumentEditor const &tde,
  int cursorLine, int cursorCol,
  int markLine, int markCol,
  char const *text)
{
  expect(tde, cursorLine, cursorCol, text);
  expectMark(tde, markLine, markCol);
}


void expectBlockIndent(
  TextDocumentEditor &tde,
  int amt,
  int cursorLine, int cursorCol,
  int markLine, int markCol,
  char const *expectText)
{
  tde.setCursor(TextLCoord(cursorLine, cursorCol));
  tde.setMark(TextLCoord(markLine, markCol));
  tde.blockIndent(amt);
  expectM(tde, cursorLine, cursorCol, markLine, markCol, expectText);
}


void testBlockIndent()
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

  tde.setMark(TextLCoord(1, 0));
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
  tde.setCursor(TextLCoord(1, 40));
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
  tde.setCursor(TextLCoord(0,3));
  tde.setMark(TextLCoord(1,7));
  xassert(tde.getSelectedText() == "ne\n      t");

  // Test 'insertNewline' while beyond EOF.
  tde.clearMark();
  tde.setCursor(TextLCoord(6,6));
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


void testBlockIndent2()
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
  tde.setCursor(TextLCoord(3,0));
  tde.insertNulTermText("  \n");
  expectBlockIndent(tde, -1, 0,0, 4,0,
    " one\n"
    "\n"
    " three\n"
    " \n");      // one space now
}


// --------------------- testFillToCursor -----------------------
void expectFillToCursor(
  TextDocumentEditor &tde,
  int cursorLine, int cursorCol,
  char const *expectText)
{
  tde.setCursor(TextLCoord(cursorLine, cursorCol));
  tde.fillToCursor();
  expect(tde, cursorLine, cursorCol, expectText);
}


void testFillToCursor()
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
void expectFV(TextDocumentEditor const &tde,
  int cursorLine, int cursorCol,
  int fvLine, int fvCol,
  int visLines, int visColumns)
{
  tde.selfCheck();

  checkCoord(tde.cursor(), TextLCoord(cursorLine, cursorCol), "cursor");
  checkCoord(tde.firstVisible(), TextLCoord(fvLine, fvCol), "firstVisible");
  xassert(visLines == tde.visLines());
  xassert(visColumns == tde.visColumns());
}

void testScrollToCursor()
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

  // Insert a test for getSelectLayoutRange with mark inactive.
  {
    TextLCoordRange range = tde.getSelectLayoutRange();
    xassert(range.m_start == TextLCoord(3, 0));
    xassert(range.m_end == TextLCoord(3, 0));
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
  tde.setCursor(TextLCoord(6, 20));
  expectFV(tde, 6,20, 2,0, 5,10);      // did not scroll yet
  tde.scrollToCursor();
  expectFV(tde, 6,20, 2,11, 5,10);

  // Test with edgeGap > 0.
  tde.scrollToCursor(1 /*edgeGap*/);
  expectFV(tde, 6,20, 3,12, 5,10);

  xassert(tde.cursorAtEnd() == false); // beyond end

  // Back to the start with edgeGap>0, which will have no effect.
  tde.setCursor(TextLCoord(0,0));
  tde.scrollToCursor(1 /*edgeGap*/);
  expectFV(tde, 0,0, 0,0, 5,10);

  xassert(tde.cursorAtEnd() == false); // at start

  // Test with -1 edgeGap.
  tde.setCursor(TextLCoord(20, 20));    // offscreen
  tde.scrollToCursor(-1 /*edgeGap*/);
  expectFV(tde, 20,20, 18,15, 5,10);

  // Test with -1 and a coordinate just barely offscreen.  This kills
  // a testing mutant where, in 'stcHelper', we do not reset the gap
  // to 0 in the -1 case.
  tde.setCursor(TextLCoord(17,15));     // just above FV
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
  tde.setFirstVisible(TextLCoord(10,10));
  expectFV(tde, 3,0, 10,10, 5,10);
  tde.moveFirstVisibleAndCursor(0, +1);    // scroll to cursor, then shift right
  expectFV(tde, 3,1, 3,1, 5,10);
  tde.setCursor(TextLCoord(4,2));           // one in from left/top
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
  tde.setCursor(TextLCoord(50,50));
  tde.centerVisibleOnCursorLine();         // cursor at right edge
  expectFV(tde, 50,50, 48,41, 5,10);
  tde.setCursor(TextLCoord(5,1));
  tde.centerVisibleOnCursorLine();         // back near top-left
  expectFV(tde, 5,1, 3,0, 5,10);

  // Test with a gap size bigger than the viewport.
  tde.setCursor(TextLCoord(10,0));
  tde.scrollToCursor(10 /*gap*/);
  expectFV(tde, 10,0, 8,0, 5,10);

  // Again, but near the top edge (don't go negative!).
  tde.setCursor(TextLCoord(1,0));
  tde.scrollToCursor(10 /*gap*/);
  expectFV(tde, 1,0, 0,0, 5,10);
}


// ---------------------- testGetWordAfter ----------------------
void testOneWordAfter(TextDocumentEditor &tde,
  int line, int col, char const *expect)
{
  string actual = tde.getWordAfter(TextLCoord(line, col));
  xassert(actual == expect);
}


void testGetWordAfter()
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
void testOneGAI(TextDocumentEditor &tde, int line,
  int expectIndCols, string const &expectIndText)
{
  string actualIndText;
  int actualIndCols =
    tde.getAboveIndentationColumns(line, actualIndText /*OUT*/);
  EXPECT_EQ(actualIndCols, expectIndCols);
  EXPECT_EQ(actualIndText, expectIndText);
}

void testGetAboveIndentation()
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

  testOneGAI(tde, -1, 0, "");
  testOneGAI(tde, 0, 0, "");
  testOneGAI(tde, 1, 0, "");
  testOneGAI(tde, 2, 2, "  ");
  testOneGAI(tde, 3, 2, "  ");
  testOneGAI(tde, 4, 4, "    ");
  testOneGAI(tde, 5, 0, "");
  testOneGAI(tde, 6, 0, "");
  testOneGAI(tde, 7, 2, "  ");
  testOneGAI(tde, 8, 2, "  ");
  testOneGAI(tde, 9, 2, "  ");
  testOneGAI(tde, 10, 4, "    ");
  testOneGAI(tde, 11, 4, "    ");
  testOneGAI(tde, 12, 4, "    ");
  testOneGAI(tde, 13, 4, "    ");
}


// ---------------------- testMoveCursor ------------------------
void testMoveCursor()
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
  tde.setCursor(TextLCoord(44,44));
  tde.selectCursorLine();
  expectCursor(tde, 44,0); expectMark(tde, 45,0);

  // Test 'advanceWithWrap'.
  tde.advanceWithWrap(false /*backwards*/);
  expectCursor(tde, 45,0);
  tde.advanceWithWrap(true /*backwards*/);
  expectCursor(tde, 44,0);

  tde.setCursor(TextLCoord(1,1));
  tde.advanceWithWrap(false /*backwards*/);
  expectCursor(tde, 1,2);
  tde.advanceWithWrap(false /*backwards*/);
  expectCursor(tde, 1,3);
  tde.advanceWithWrap(false /*backwards*/);
  expectCursor(tde, 2,0);
  tde.advanceWithWrap(true /*backwards*/);
  expectCursor(tde, 1,3);

  tde.setCursor(TextLCoord(1, 45));
  tde.advanceWithWrap(false /*backwards*/);
  expectCursor(tde, 2,0);

  tde.setCursor(TextLCoord(1, 45));
  tde.advanceWithWrap(true /*backwards*/);
  expectCursor(tde, 1,44);

  tde.setCursor(TextLCoord(0, 0));
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
void testBackspaceFunction()
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
  tde.setMark(TextLCoord(0,1));
  tde.setCursor(TextLCoord(0,2));
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
  tde.setCursor(TextLCoord(4,0));
  tde.backspaceFunction();
  expect(tde, 3,0,
    "e\n"
    "two  \n"
    "three");

  // Backspace at left edge to join two lines.
  tde.setCursor(TextLCoord(1,0));
  tde.backspaceFunction();
  expect(tde, 0,1,
    "etwo  \n"
    "three");

  // Backspace beyond EOL: move left.
  tde.setCursor(TextLCoord(0,7));
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
  tde.setCursor(TextLCoord(1,0));
  tde.setFirstVisible(TextLCoord(1,0));
  tde.backspaceFunction();
  expect(tde, 0,5,
    "etwo three");
  checkCoord(tde.firstVisible(), TextLCoord(0,0), "first visible");
}


// ------------------- testDeleteKeyFunction --------------------
void testDeleteKeyFunction()
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
  tde.setMark(TextLCoord(0,1));
  tde.setCursor(TextLCoord(0,2));
  tde.deleteKeyFunction();
  expect(tde, 0,1,
    "oe\n"
    "two  \n"
    "three\n");

  // Delete beyond EOL: fill then splice.
  tde.setCursor(TextLCoord(1, 10));
  tde.deleteKeyFunction();
  expect(tde, 1,10,
    "oe\n"
    "two       three\n");

  // Delete well beyond EOF: no-op.
  tde.setCursor(TextLCoord(10, 10));
  tde.deleteKeyFunction();
  expect(tde, 10,10,
    "oe\n"
    "two       three\n");

  // Selection that is partly offscreen such that after
  // deletion scrolling changes visible region.
  tde.setCursor(TextLCoord(1, 10));
  tde.setMark(TextLCoord(1, 0));
  tde.setFirstVisible(TextLCoord(1, 10));
  tde.deleteSelection();
  expectNM(tde, 1,0,
    "oe\n"
    "three\n");
  checkCoord(tde.firstVisible(), TextLCoord(1,0), "first visible");
}


// ---------------------- testClipboard -------------------------
void testClipboard()
{
  TextDocumentAndEditor tde;
  tde.insertNulTermText(
    "one\n"
    "two  \n"
    "three\n");

  // Try with empty strings.
  xassert(tde.clipboardCopy().empty());
  xassert(tde.clipboardCut().empty());
  tde.clipboardPaste("", 0);
  expectNM(tde, 3,0,
    "one\n"
    "two  \n"
    "three\n");

  // Copy.
  tde.setCursor(TextLCoord(0,1));
  tde.setMark(TextLCoord(1,2));
  xassert(tde.clipboardCopy() == "ne\ntw");
  expectNM(tde, 0,1,
    "one\n"
    "two  \n"
    "three\n");

  // Cut with cursor ahead of mark.
  tde.setCursor(TextLCoord(2,4));
  tde.setMark(TextLCoord(2,2));
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
  tde.setMark(TextLCoord(1,2));
  tde.clipboardPaste("xyz", 3);
  expectNM(tde, 1,5,
    "one\n"
    "twxyze\n");

  // Paste while beyond EOL.
  tde.setCursor(TextLCoord(0, 5));
  tde.clipboardPaste("123", 3);
  expectNM(tde, 0,8,
    "one  123\n"
    "twxyze\n");
}


// ---------------- testInsertNewlineAutoIndent ------------------
void testInsertNewlineAutoIndent()
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
  tde.setCursor(TextLCoord(2, 0));
  tde.insertNewlineAutoIndent();
  expectNM(tde, 3,0,
    "one\n"
    "two  \n"
    "\n"
    "three\n"
    "\n");

  // Enter to break a line.
  tde.setCursor(TextLCoord(3, 2));
  tde.insertNewlineAutoIndent();
  expectNM(tde, 4,0,
    "one\n"
    "two  \n"
    "\n"
    "th\n"
    "ree\n"
    "\n");

  // Not adding extra spaces when beyond EOL.
  tde.setCursor(TextLCoord(1, 10));
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
void testInsertNewlineAutoIndent2()
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
  tde.setCursor(TextLCoord(2, 0));
  tde.insertNewlineAutoIndent();
  expectNM(tde, 3,3,
    "  one\n"
    "   two  \n"
    "\n"
    "    three\n"
    "\n");

  // Enter to break a line.
  tde.setCursor(TextLCoord(3, 6));
  tde.insertNewlineAutoIndent();
  expectNM(tde, 4,4,
    "  one\n"
    "   two  \n"
    "\n"
    "    th\n"
    "    ree\n"
    "\n");

  // Not adding extra spaces when beyond EOL.
  tde.setCursor(TextLCoord(1, 10));
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
  tde.setMark(TextLCoord(2,0));
  tde.setCursor(TextLCoord(8,4));
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
  tde.deleteTextLRange(TextLCoord(0,0), tde.endLCoord());
  tde.setCursor(TextLCoord(0,0));
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
  tde.setCursor(TextLCoord(2,0));
  tde.setMark(TextLCoord(8,4));
  tde.insertNewlineAutoIndent();
  expectNM(tde, 3,3,
    "  one\n"
    "   two  \n"
    "\n"
    "   th\n"
    "    ree\n"
    "\n");
}


void testInsertNewlineAutoIndent3()
{
  TextDocumentAndEditor tde;
  tde.insertNulTermText(
    "one\n"
    "two\n");

  // Hit Enter while beyond EOF.
  tde.setCursor(TextLCoord(4,0));
  tde.insertNewlineAutoIndent();
  expectNM(tde, 5,0,
    "one\n"
    "two\n"
    "\n"
    "\n"
    "\n");
  tde.undo();

  // Now with selected text, entirely beyond EOF.
  tde.setMark(TextLCoord(4,0));
  tde.setCursor(TextLCoord(4,4));
  tde.insertNewlineAutoIndent();
  expectNM(tde, 5,0,
    "one\n"
    "two\n"
    "\n"
    "\n"
    "\n");
  tde.undo();

  // Selected text straddling EOF.
  tde.setMark(TextLCoord(1,1));
  tde.setCursor(TextLCoord(4,4));
  tde.insertNewlineAutoIndent();
  expectNM(tde, 2,0,
    "one\n"
    "t\n");
  tde.undo();
}


void testInsertNewlineAutoIndent4()
{
  TextDocumentAndEditor tde;
  tde.setVisibleSize(5, 10);
  tde.insertNulTermText(
    "  a\n"
    "  b\n");

  // Start with the display scrolled to the right.  It should
  // return to the left edge.
  tde.setFirstVisible(TextLCoord(0,1));
  tde.setCursor(TextLCoord(1,3));
  tde.insertNewlineAutoIndent();
  expectFV(tde, 2,2, 0,0, 5,10);
  expectNM(tde, 2,2,
    "  a\n"
    "  b\n"
    "\n");
}


void testInsertNewlineAutoIndentWithTab()
{
  TextDocumentAndEditor tde;
  tde.insertNulTermText(
    "a\n"
    "\tb\n");

  // Auto-indent itself should *not* add a Tab.
  tde.setCursor(TextLCoord(1, 9));
  xassert(tde.cursorAtLineEnd());
  tde.insertNewlineAutoIndent();
  expectNM(tde, 2,8,
    "a\n"
    "\tb\n"
    "\n");

  // But a Tab should appear once we insert some text.
  tde.insertNulTermText("c");
  expectNM(tde, 2,9,
    "a\n"
    "\tb\n"
    "\tc\n");

  // Delete the character so the rest of the test operates as it did
  // before I changed how auto-indent works with tabs.
  tde.backspaceFunction();
  expectNM(tde, 2,8,
    "a\n"
    "\tb\n"
    "\t\n");

  // Again.
  tde.insertNewlineAutoIndent();
  expectNM(tde, 3,8,
    "a\n"
    "\tb\n"
    "\t\n"
    "\n");
  tde.insertNulTermText("d");
  tde.backspaceFunction();
  expectNM(tde, 3,8,
    "a\n"
    "\tb\n"
    "\t\n"
    "\t\n");

  // Adding another tab to this line will not affect indentation because
  // it is entirely whitespace.
  tde.insertNulTermText("\t");
  tde.insertNewlineAutoIndent();
  tde.insertNulTermText("e");
  tde.backspaceFunction();
  expectNM(tde, 4,8,
    "a\n"
    "\tb\n"
    "\t\n"
    "\t\t\n"
    "\t\n");

  // But adding a Tab and another character will.
  tde.insertNulTermText("\tc");
  tde.insertNewlineAutoIndent();
  expectNM(tde, 5,16,
    "a\n"
    "\tb\n"
    "\t\n"
    "\t\t\n"
    "\t\tc\n"
    "\n");
  tde.insertNulTermText("x");
  tde.backspaceFunction();
  expectNM(tde, 5,16,
    "a\n"
    "\tb\n"
    "\t\n"
    "\t\t\n"
    "\t\tc\n"
    "\t\t\n");

  // Mix of Tabs and spaces.
  tde.insertNulTermText(" \t d");
  tde.insertNewlineAutoIndent();
  expectNM(tde, 6,25,
    "a\n"
    "\tb\n"
    "\t\n"
    "\t\t\n"
    "\t\tc\n"
    "\t\t \t d\n"
    "\n");
  tde.insertNulTermText("x");
  tde.backspaceFunction();
  expectNM(tde, 6,25,
    "a\n"
    "\tb\n"
    "\t\n"
    "\t\t\n"
    "\t\tc\n"
    "\t\t \t d\n"
    "\t\t \t \n");

  // Go beyond the end of the document and type.
  tde.setCursor(TextLCoord(8,25));
  tde.insertNulTermText("y");
  expectNM(tde, 8,26,
    "a\n"
    "\tb\n"
    "\t\n"
    "\t\t\n"
    "\t\tc\n"
    "\t\t \t d\n"
    "\t\t \t \n"
    "\n"
    "\t\t \t y");
}


// -------------------- testSetVisibleSize ----------------------
void testSetVisibleSize()
{
  TextDocumentAndEditor tde;

  // Try with negative sizes.
  tde.setVisibleSize(-1, -1);
  checkCoord(tde.firstVisible(), TextLCoord(0,0), "firstVisible");
  checkCoord(tde.lastVisible(), TextLCoord(0,0), "lastVisible");

  // See if things work at this size.
  tde.insertNulTermText(
    "  one\n"
    "   two  \n"
    " three");
  checkCoord(tde.firstVisible(), TextLCoord(2,6), "firstVisible");
  checkCoord(tde.lastVisible(), TextLCoord(2,6), "lastVisible");

  // Cursor movement does not automatically scroll.
  tde.moveCursorBy(-1,0);
  checkCoord(tde.firstVisible(), TextLCoord(2,6), "firstVisible");
  tde.scrollToCursor();
  checkCoord(tde.firstVisible(), TextLCoord(1,6), "firstVisible");
}


// -------------------- testCursorRestorer ----------------------
void testCursorRestorer()
{
  TextDocumentAndEditor tde;
  tde.setVisibleSize(5, 10);
  tde.insertNulTermText(
    "one\n"
    "two\n"
    "three\n");

  // Restore an active mark and a scroll position.
  tde.setMark(TextLCoord(2,1));
  tde.setCursor(TextLCoord(2,2));
  tde.setFirstVisible(TextLCoord(1,1));
  {
    CursorRestorer restorer(tde);
    tde.clearMark();
    tde.setCursor(TextLCoord(4,4));
    tde.setFirstVisible(TextLCoord(0,0));
  }
  expectMark(tde, 2,1);
  expectFV(tde, 2,2, 1,1, 5,10);

  // Ensure inactive mark is restored as such.
  tde.clearMark();
  {
    CursorRestorer restorer(tde);
    tde.setMark(TextLCoord(0,0));
  }
  expectNM(tde, 2,2,
    "one\n"
    "two\n"
    "three\n");
}


// ----------------------- testSetMark --------------------------
void testSetMark()
{
  TextDocumentAndEditor tde;
  tde.insertNulTermText(
    "one\n"
    "two\n"
    "three\n");
  xassert(!tde.markActive());

  tde.setMark(TextLCoord(1,1));
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
  tde.setCursor(TextLCoord(2,2));
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
  tde.setMark(TextLCoord(2,3));
  tde.turnOffSelectionIfEmpty();
  expectMark(tde, 2,3);
}


// ----------------- testConfineCursorToVisible -----------------
void testConfineCursorToVisible()
{
  TextDocumentAndEditor tde;
  tde.setVisibleSize(3,3);

  // Already visible.
  tde.confineCursorToVisible();
  expectCursor(tde, 0,0);

  // Pull in from corner.
  tde.setFirstVisible(TextLCoord(1,1));
  tde.confineCursorToVisible();
  expectCursor(tde, 1,1);

  // From top.
  tde.setCursor(TextLCoord(0,2));
  tde.confineCursorToVisible();
  expectCursor(tde, 1,2);

  // From bottom.
  tde.setCursor(TextLCoord(4,2));
  tde.confineCursorToVisible();
  expectCursor(tde, 3,2);

  // From left.
  tde.setCursor(TextLCoord(2,0));
  tde.confineCursorToVisible();
  expectCursor(tde, 2,1);

  // From right.
  tde.setCursor(TextLCoord(2,4));
  tde.confineCursorToVisible();
  expectCursor(tde, 2,3);
}


// ------------------- testJustifyNearCursor --------------------
// There are already extensive tests of the justification algorithm in
// justify-test.cc, so here I just do a quick check.
void testJustifyNearCursor()
{
  TextDocumentAndEditor tde;
  tde.insertNulTermText(
    "one two three four five six seven\n"
    "\n"
    "eight nine ten\n"
    "eleven twelve\n");

  // Cursor not on anything, no-op.
  tde.justifyNearCursor(10);
  expect(tde, 4,0,
    "one two three four five six seven\n"
    "\n"
    "eight nine ten\n"
    "eleven twelve\n");

  // Cursor on first paragraph.
  tde.setCursor(TextLCoord(0,5));
  tde.justifyNearCursor(10);
  expect(tde, 4,0,
    "one two\n"
    "three four\n"
    "five six\n"
    "seven\n"
    "\n"
    "eight nine ten\n"
    "eleven twelve\n");

  // Cursor on second paragraph.
  tde.setCursor(TextLCoord(6,0));
  tde.justifyNearCursor(10);
  expect(tde, 8,0,
    "one two\n"
    "three four\n"
    "five six\n"
    "seven\n"
    "\n"
    "eight nine\n"
    "ten eleven\n"
    "twelve\n");
}


// --------------------- testInsertDateTime ---------------------
void testInsertDateTime()
{
  TextDocumentAndEditor tde;

  // First test with a specific time.
  FixedDateTimeProvider fdtp(1000000000, 0);
  tde.insertDateTime(&fdtp);
  expect(tde, 0, 16,
    "2001-09-09 01:46");
  tde.insertNewline();

  // Test inserting while text is selected.
  tde.insertNulTermText("xyz\n");
  tde.setCursor(TextLCoord(1,1));
  tde.setMark(TextLCoord(1,2));
  tde.insertDateTime(&fdtp);
  expectNM(tde, 1, 17,
    "2001-09-09 01:46\n"
    "x2001-09-09 01:46z\n");

  // Test inserting beyond EOF.
  tde.setCursor(TextLCoord(5, 2));
  tde.insertDateTime(&fdtp);
  expectNM(tde, 5, 18,
    "2001-09-09 01:46\n"
    "x2001-09-09 01:46z\n"
    "\n"
    "\n"
    "\n"
    "  2001-09-09 01:46");

  // Test with current date/time, validating size only.
  tde.setCursor(TextLCoord(2,0));
  tde.insertDateTime();
  expectCursor(tde, 2, 16);
}


// --------------------- testReplaceText ---------------------
// Select from line/col1 to line/col2, replace with 'text'.
void replaceText(
  TextDocumentEditor &tde,
  int line1, int col1, int line2, int col2, bool swapCM,
  char const *text,
  TextDocumentEditor::InsertTextFlags flags =
    TextDocumentEditor::ITF_NONE)
{
  tde.setCursor(TextLCoord(line1, col1));
  tde.setMark(TextLCoord(line2, col2));
  if (swapCM) {
    tde.swapCursorAndMark();
  }
  tde.insertNulTermText(text, flags);
}

// This tests 'insertText', but specifically exercising the aspect
// that does 'deleteSelection' first if the mark is active.
//
// If 'swapCM', we swap cursor and mark before each insertion.
void testReplaceText(bool swapCM)
{
  TextDocumentAndEditor tde;

  tde.insertNulTermText(
    "one\n"
    "two\n"
    "three\n");
  expectNM(tde, 3, 0,
    "one\n"
    "two\n"
    "three\n");

  // Start beyond EOL and span line boundary.
  replaceText(tde, 1,4, 2,4, swapCM, "abc");
  expectNM(tde, 1, 7,
    "one\n"
    "two abce\n");

  // Span EOF.
  replaceText(tde, 1,6, 3,4, swapCM, "q\nr");
  expectNM(tde, 2, 1,
    "one\n"
    "two abq\n"
    "r");

  // Selection entirely beyond EOL.
  replaceText(tde, 1,10, 1,15, swapCM, "shazam");
  expectNM(tde, 1, 16,
    "one\n"
    "two abq   shazam\n"
    "r");

  // Selection entirely beyond EOF.
  replaceText(tde, 4,2, 5,1, swapCM, "nein");
  expectNM(tde, 4, 6,
    "one\n"
    "two abq   shazam\n"
    "r\n"
    "\n"
    "  nein");

  // Selection covers entire file.
  replaceText(tde, 0,0, 4,6, swapCM, "gro\nk\n");
  expectNM(tde, 2,0,
    "gro\n"
    "k\n");

  // More beyond EOL stuff.
  replaceText(tde, 0,10, 1,10, swapCM, "x");
  expectNM(tde, 0,11,
    "gro       x\n");

  // More beyond EOF stuff.
  replaceText(tde, 3,10, 15,210, swapCM, "x");
  expectNM(tde, 3,11,
    "gro       x\n"
    "\n"
    "\n"
    "          x");
  replaceText(tde, 5,2, 15,210, swapCM, "x");
  expectNM(tde, 5,3,
    "gro       x\n"
    "\n"
    "\n"
    "          x\n"
    "\n"
    "  x");
}


// Test 'insertText' with ITF_SELECT_AFTERWARD.
void testReplaceAndSelect(bool swapCM)
{
  TextDocumentAndEditor tde;
  TextDocumentEditor::InsertTextFlags itf =
    TextDocumentEditor::ITF_SELECT_AFTERWARD;

  tde.insertNulTermText(
    "one\n"
    "two\n"
    "three\n",
    itf);
  expectM(tde, 3, 0, 0, 0,
    "one\n"
    "two\n"
    "three\n");

  replaceText(tde, 1,0, 2,0, swapCM, "x\n", itf);
  expectM(tde, 2, 0, 1, 0,
    "one\n"
    "x\n"
    "three\n");
}


void expectCountSpace(TextDocumentEditor &tde,
  int line, int expectLeading, int expectTrailing)
{
  int leading = tde.countLeadingSpacesTabs(line);
  EXPECT_EQ(leading, expectLeading);

  int trailing = tde.countTrailingSpacesTabsColumns(line);
  EXPECT_EQ(trailing, expectTrailing);
}


void testCountSpaceChars()
{
  TextDocumentAndEditor tde;
  tde.insertNulTermText(
    "one\n"
    "  two\n"
    "three   \n"
    "    four    \n"
    "     \n"
    "      ");

  expectCountSpace(tde, 0, 0, 0);
  expectCountSpace(tde, 1, 2, 0);
  expectCountSpace(tde, 2, 0, 3);
  expectCountSpace(tde, 3, 4, 4);
  expectCountSpace(tde, 4, 5, 5);
  expectCountSpace(tde, 5, 6, 6);
  expectCountSpace(tde, 6, 0, 0);
  expectCountSpace(tde, 7, 0, 0);
}

void testCountSpaceCharsWithTabs()
{
  TextDocumentAndEditor tde;
  tde.insertNulTermText(
    "one\n"
    "two\t\n"
    "three\t\t\n"
    "four \t\n"
    "five\t \n"
    "");

  expectCountSpace(tde, 0, 0, 0);
  expectCountSpace(tde, 1, 0, 5);
  expectCountSpace(tde, 2, 0, 11);
  expectCountSpace(tde, 3, 0, 4);
  expectCountSpace(tde, 4, 0, 5);
}


void expectGSOI_nm(TextDocumentEditor &tde, int line, int col,
                          string const &expect)
{
  tde.setCursor(TextLCoord(line, col));
  tde.clearMark();
  string actual = tde.getSelectedOrIdentifier();
  EXPECT_EQ(actual, expect);
}

void testGetSelectedOrIdentifier()
{
  TextDocumentAndEditor tde;
  tde.insertNulTermText(
    "\n"
    " \n"
    "abc\n"
    " abc \n"
    " azAZ_09 \n"
    "$azAZ_09-\n"
    "      \n"
    "\ta");

  expectGSOI_nm(tde, 0,0, "");
  expectGSOI_nm(tde, 0,1, "");
  expectGSOI_nm(tde, 1,0, "");
  expectGSOI_nm(tde, 2,0, "abc");
  expectGSOI_nm(tde, 2,2, "abc");
  expectGSOI_nm(tde, 2,3, "");
  expectGSOI_nm(tde, 3,0, "");
  expectGSOI_nm(tde, 3,1, "abc");
  expectGSOI_nm(tde, 3,2, "abc");
  expectGSOI_nm(tde, 3,3, "abc");
  expectGSOI_nm(tde, 3,4, "");
  expectGSOI_nm(tde, 4,4, "azAZ_09");
  expectGSOI_nm(tde, 5,4, "azAZ_09");
  expectGSOI_nm(tde, 6,4, "");

  // The column is specified in layout coordinates.
  expectGSOI_nm(tde, 7,7, "a");

  // Test with a selection.
  tde.setCursor(TextLCoord(4, 2));
  tde.setMark(TextLCoord(4, 4));
  string actual = tde.getSelectedOrIdentifier();
  EXPECT_EQ(actual, string("zA"));
}


void testReadOnly()
{
  TextDocumentAndEditor tde;

  // The only thing to verify at this level is it acts like a flag.
  EXPECT_EQ(tde.isReadOnly(), false);
  tde.setReadOnly(true);
  EXPECT_EQ(tde.isReadOnly(), true);
  tde.setReadOnly(false);
  EXPECT_EQ(tde.isReadOnly(), false);
}


void innerExpectLayoutWindow(TextDocumentEditor &tde,
  int fvLine, int fvCol,
  int lvLine, int lvCol,
  string const &preExpect)
{
  // The 'preExpect' string uses '^' instead of '\t' so the visual
  // alignment is not disrupted.
  string expect(replaceAll(preExpect, "^", "\t"));

  ArrayStack<char> text;
  int width = lvCol - fvCol + 1;
  std::ostringstream sb;
  for (int line = fvLine; line <= lvLine; line++) {
    text.clear();
    tde.getLineLayout(TextLCoord(line, fvCol), text, width);
    sb << string(text.getArray(), width);
  }
  string actual = sb.str();

  EXPECT_EQ(actual, expect);
}

void expectLayoutWindow(TextDocumentEditor &tde,
  int fvLine, int fvCol,
  int lvLine, int lvCol,
  string const &preExpect)
{
  innerExpectLayoutWindow(tde, fvLine, fvCol, lvLine, lvCol, preExpect);

  // Probe all of the layout coordinates in the window.
  for (int line = fvLine; line <= lvLine; line++) {
    for (int col = fvCol; col <= fvCol; col++) {
      TextLCoord lc(line, col);
      TextMCoord mc(tde.toMCoord(lc));
      xassert(lc.m_line == mc.m_line);

      // Check (approximate) inverse.
      TextLCoord lc2(tde.toLCoord(mc));
      xassert(lc2.m_line == mc.m_line);
      xassert(lc2.m_column >= lc.m_column);

      TextMCoord mc2(tde.toMCoord(lc2));
      xassert(mc2 == mc);
    }
  }

  // Try all values of 'fvCol' up to 'lvCol'.
  int oldWidth = lvCol - fvCol + 1;
  for (int newFvCol = fvCol; newFvCol <= lvCol; newFvCol++) {
    // Construct new expected layout by trimming columns.
    int newWidth = lvCol - newFvCol + 1;
    std::ostringstream newPreExpect;
    for (int line = fvLine; line <= lvLine; line++) {
      newPreExpect <<
        preExpect.substr((line-fvLine)*oldWidth + (newFvCol-fvCol),
                         newWidth);
    }

    innerExpectLayoutWindow(tde,
      fvLine, newFvCol,
      lvLine, lvCol,
      newPreExpect.str());
  }

  // Try all values of 'lvCol' down to 'fvCol'.
  for (int newLvCol = lvCol; newLvCol >= fvCol; newLvCol--) {
    // Construct new expected layout by trimming columns.
    int newWidth = newLvCol - fvCol + 1;
    std::ostringstream newPreExpect;
    for (int line = fvLine; line <= lvLine; line++) {
      newPreExpect <<
        preExpect.substr((line-fvLine)*oldWidth,
                         newWidth);
    }

    innerExpectLayoutWindow(tde,
      fvLine, fvCol,
      lvLine, newLvCol,
      newPreExpect.str());
  }
}

void expectLCoord(TextDocumentEditor &tde,
  int line, int byteIndex,
  int row, int col)
{
  TextMCoord mc(line, byteIndex);
  TextLCoord expect(row, col);
  TextLCoord actual = tde.toLCoord(mc);
  EXPECT_EQ(actual, expect);

  // 'toMCoord' should be the inverse of 'toLCoord'.
  TextMCoord mc2(tde.toMCoord(actual));
  EXPECT_EQ(mc2, mc);
}

void expectMCoord(TextDocumentEditor &tde,
  int row, int col,
  int line, int byteIndex)
{
  TextLCoord lc(row, col);
  TextMCoord expect(line, byteIndex);
  TextMCoord actual = tde.toMCoord(lc);
  EXPECT_EQ(actual, expect);

  // Conversion to model should be idempotent.
  TextLCoord lc2(tde.toLCoord(actual));
  TextMCoord actual2(tde.toMCoord(lc2));
  EXPECT_EQ(actual2, expect);
}

void testLineLayout()
{
  TextDocumentAndEditor tde;
  tde.insertNulTermText(
    "one\n"
    "two\tthree\n"
    "four\tfive\tsix\n"
    "\tseven\n"
    "\t\teight\n"
    "1\t12\t123\t1234\t12345\t123456\tx\n"
    "12345\t123456\tx\n"
    "1234567\t12345678\t123456789\n"
    "");
  expectLayoutWindow(tde, 0,0, 8,39,
  //           1         2         3
  // 0123456789012345678901234567890123456789
    "one                                     "    // 0
    "two^    three                           "    // 1
    "four^   five^   six                     "    // 2
    "^       seven                           "    // 3
    "^       ^       eight                   "    // 4
    "1^      12^     123^    1234^   12345^  "    // 5
    "12345^  123456^ x                       "    // 6
    "1234567^12345678^       123456789       "    // 7
    "                                        "    // 8
  );

  expectLCoord(tde, 0,0, 0,0);
  expectLCoord(tde, 2,10, 2,16);     // 's' in "six"

  expectMCoord(tde, 5,12, 5,5);      // gap between "12" and "123"
  expectLCoord(tde, 5,5, 5,16);      // '1' in "123"

  expectMCoord(tde, -1,60, 0,0);     // before start
  expectMCoord(tde, 3,60, 3,6);      // beyond EOL
  expectMCoord(tde, 8,12, 8,0);      // beyond EOF

  expectLayoutWindow(tde, 1,3, 7,10,
  //           1         2         3
  // 0123456789012345678901234567890123456789
       "^    thr"    // 1
       "r^   fiv"    // 2
       "     sev"    // 3
       "     ^  "    // 4
       "     12^"    // 5
       "45^  123"    // 6
       "4567^123"    // 7
  );
}


void expectVisibleWindow(TextDocumentEditor &tde,
  int cursorLine, int cursorCol,
  string const &preExpect)
{
  TextLCoord expectCursor(cursorLine, cursorCol);
  EXPECT_EQ(tde.cursor(), expectCursor);

  expectLayoutWindow(tde,
    tde.firstVisible().m_line, tde.firstVisible().m_column,
    tde.lastVisible().m_line, tde.lastVisible().m_column,
    preExpect);
}

void testEditingWithTabs()
{
  TextDocumentAndEditor tde;
  tde.insertNulTermText(
    "one\n"
    "two\tthree\n"
    "four\tfive\tsix\n"
    "\tseven\n"
    "\t\teight\n"
    "1\t12\t123\t1234\t12345\t123456\tx\n"
    "12345\t123456\tx\n"
    "1234567\t12345678\t123456789\n"
    "");

  tde.setCursor(TextLCoord(1,0));
  tde.setFirstVisible(TextLCoord(0, 0));
  tde.setLastVisible(TextLCoord(3, 19));
  expectVisibleWindow(tde, 1,0,
  //           1
  // 01234567890123456789
    "one                 "    // 0
    "two^    three       "    // 1
    "four^   five^   six "    // 2
    "^       seven       "    // 3
  );

  tde.insertNulTermText("x");
  expectVisibleWindow(tde, 1,1,
  //           1
  // 01234567890123456789
    "one                 "    // 0
    "xtwo^   three       "    // 1
    "four^   five^   six "    // 2
    "^       seven       "    // 3
  );

  tde.insertNulTermText("\t");
  expectVisibleWindow(tde, 1,8,
  //           1
  // 01234567890123456789
    "one                 "    // 0
    "x^      two^    thre"    // 1
    "four^   five^   six "    // 2
    "^       seven       "    // 3
  );

  tde.backspaceFunction();
  expectVisibleWindow(tde, 1,1,
  //           1
  // 01234567890123456789
    "one                 "    // 0
    "xtwo^   three       "    // 1
    "four^   five^   six "    // 2
    "^       seven       "    // 3
  );

  // Backspace while in the Tab span deletes it.
  tde.setCursor(TextLCoord(1,6));
  tde.backspaceFunction();
  expectVisibleWindow(tde, 1,4,
  //           1
  // 01234567890123456789
    "one                 "    // 0
    "xtwothree           "    // 1
    "four^   five^   six "    // 2
    "^       seven       "    // 3
  );

  tde.undo();
  expectVisibleWindow(tde, 1,4,
  //           1
  // 01234567890123456789
    "one                 "    // 0
    "xtwo^   three       "    // 1
    "four^   five^   six "    // 2
    "^       seven       "    // 3
  );

  // Delete in the Tab span also deletes it.
  tde.deleteKeyFunction();
  expectVisibleWindow(tde, 1,4,
  //           1
  // 01234567890123456789
    "one                 "    // 0
    "xtwothree           "    // 1
    "four^   five^   six "    // 2
    "^       seven       "    // 3
  );

  // Select text starting and ending within Tab spans.  The Tab on the
  // left is deleted, along with enclosed text, but not the Tab on the
  // right.
  tde.setCursor(TextLCoord(2,6));
  tde.setMark(TextLCoord(2,14));
  tde.deleteKeyFunction();
  expectVisibleWindow(tde, 2,6,
  //           1
  // 01234567890123456789
    "one                 "    // 0
    "xtwothree           "    // 1
    "four^   six         "    // 2
    "^       seven       "    // 3
  );

  // Automatic indentation merely positions the cursor.
  tde.setCursor(TextLCoord(3, 13));
  tde.insertNewlineAutoIndent();
  expectVisibleWindow(tde, 4,8,
  //           1
  // 01234567890123456789
    "xtwothree           "    // 1
    "four^   six         "    // 2
    "^       seven       "    // 3
    "                    "    // 4
  );

  // Inserting a character also inserts a tab for indentation.
  tde.insertNulTermText("e");
  expectVisibleWindow(tde, 4,9,
  //           1
  // 01234567890123456789
    "xtwothree           "    // 1
    "four^   six         "    // 2
    "^       seven       "    // 3
    "^       e           "    // 4
  );

  // This works even when there is an intervening blank line.
  tde.insertNewlineAutoIndent();
  tde.insertNewlineAutoIndent();
  tde.insertNulTermText("x");
  expectVisibleWindow(tde, 6,9,
  //           1
  // 01234567890123456789
    "^       seven       "    // 3
    "^       e           "    // 4
    "                    "    // 5
    "^       x           "    // 6
  );
}


void expectMTLS(TextDocumentAndEditor &tde,
  int line,
  LineCategories const &expectLayoutCategories,
  LineCategories const &modelCategories)
{
  LineCategories actualLayoutCategories(TC_NORMAL);
  tde.modelToLayoutSpans(line, actualLayoutCategories, modelCategories);
  if (expectLayoutCategories != actualLayoutCategories) {
    cout << "expect: " << expectLayoutCategories.asString() << endl;
    cout << "actual: " << actualLayoutCategories.asString() << endl;
    xfailure("mismatch");
  }
}


void testModelToLayoutSpans()
{
  TextDocumentAndEditor tde;

  // This is nearly the same text as in test/has-tabs.c.
  tde.insertNulTermText(
    /*0*/"\n"
    /*1*/"int main()\n"
    /*2*/"{\n"
    /*3*/"\tint a = 4;\n"
    /*4*/"\tprintf(\"a: %d\\n\", a);\n"
    /*5*/"\tif (a)\n"
    /*6*/"\t\ta++;\n"
    /*7*/"\treturn\t0;\n"
    /*8*/"}\n"
    "");

  // Spans in model-based coordinates.  (I could invoke the C/C++
  // highlighter, but since the expected output will be written this
  // way, I think it is clearer to directly express the inputs too.)
  LineCategories l1mc(TC_NORMAL);
  l1mc.append(TC_KEYWORD, 3);
  l1mc.append(TC_NORMAL, 4);
  l1mc.append(TC_OPERATOR, 2);

  LineCategories l2mc(TC_NORMAL);
  l2mc.append(TC_OPERATOR, 1);

  LineCategories l3mc(TC_NORMAL);
  l3mc.append(TC_NORMAL, 1);     // The single tab character.
  l3mc.append(TC_KEYWORD, 3);
  l3mc.append(TC_NORMAL, 3);
  l3mc.append(TC_OPERATOR, 1);
  l3mc.append(TC_NORMAL, 1);
  l3mc.append(TC_NUMBER, 1);
  l3mc.append(TC_OPERATOR, 1);

  LineCategories l4mc(TC_NORMAL);
  l4mc.append(TC_NORMAL, 7);     // "\tprintf"
  l4mc.append(TC_OPERATOR, 1);
  l4mc.append(TC_STRING, 9);
  l4mc.append(TC_OPERATOR, 1);
  l4mc.append(TC_NORMAL, 2);
  l4mc.append(TC_OPERATOR, 2);

  LineCategories l5mc(TC_NORMAL);
  l5mc.append(TC_NORMAL, 1);
  l5mc.append(TC_KEYWORD, 2);
  l5mc.append(TC_NORMAL, 1);
  l5mc.append(TC_OPERATOR, 1);
  l5mc.append(TC_NORMAL, 1);
  l5mc.append(TC_OPERATOR, 1);

  LineCategories l6mc(TC_NORMAL);
  l6mc.append(TC_NORMAL, 3);
  l6mc.append(TC_OPERATOR, 3);

  LineCategories l7mc(TC_NORMAL);
  l7mc.append(TC_NORMAL, 1);     // Tab.
  l7mc.append(TC_KEYWORD, 6);
  l7mc.append(TC_NORMAL, 1);     // Tab after "return".
  l7mc.append(TC_NUMBER, 1);
  l7mc.append(TC_OPERATOR, 1);

  LineCategories l8mc(TC_NORMAL);
  l8mc.append(TC_OPERATOR, 1);

  // Spans in layout coordinates.
  LineCategories l1lc(TC_NORMAL);
  l1lc.append(TC_KEYWORD, 3);
  l1lc.append(TC_NORMAL, 4);
  l1lc.append(TC_OPERATOR, 2);

  LineCategories l2lc(TC_NORMAL);
  l2lc.append(TC_OPERATOR, 1);

  LineCategories l3lc(TC_NORMAL);
  l3lc.append(TC_NORMAL, 8);     // The single tab character.
  l3lc.append(TC_KEYWORD, 3);
  l3lc.append(TC_NORMAL, 3);
  l3lc.append(TC_OPERATOR, 1);
  l3lc.append(TC_NORMAL, 1);
  l3lc.append(TC_NUMBER, 1);
  l3lc.append(TC_OPERATOR, 1);

  LineCategories l4lc(TC_NORMAL);
  l4lc.append(TC_NORMAL, 14);    // "\tprintf"
  l4lc.append(TC_OPERATOR, 1);
  l4lc.append(TC_STRING, 9);
  l4lc.append(TC_OPERATOR, 1);
  l4lc.append(TC_NORMAL, 2);
  l4lc.append(TC_OPERATOR, 2);

  LineCategories l5lc(TC_NORMAL);
  l5lc.append(TC_NORMAL, 8);
  l5lc.append(TC_KEYWORD, 2);
  l5lc.append(TC_NORMAL, 1);
  l5lc.append(TC_OPERATOR, 1);
  l5lc.append(TC_NORMAL, 1);
  l5lc.append(TC_OPERATOR, 1);

  LineCategories l6lc(TC_NORMAL);
  l6lc.append(TC_NORMAL, 17);
  l6lc.append(TC_OPERATOR, 3);

  LineCategories l7lc(TC_NORMAL);
  l7lc.append(TC_NORMAL, 8);     // Tab.
  l7lc.append(TC_KEYWORD, 6);
  l7lc.append(TC_NORMAL, 2);     // Tab after "return".
  l7lc.append(TC_NUMBER, 1);
  l7lc.append(TC_OPERATOR, 1);

  LineCategories l8lc(TC_NORMAL);
  l8lc.append(TC_OPERATOR, 1);

  expectMTLS(tde, 1, l1lc, l1mc);
  expectMTLS(tde, 2, l2lc, l2mc);
  expectMTLS(tde, 3, l3lc, l3mc);
  expectMTLS(tde, 4, l4lc, l4mc);
  expectMTLS(tde, 5, l5lc, l5mc);
  expectMTLS(tde, 6, l6lc, l6mc);
  expectMTLS(tde, 7, l7lc, l7mc);
  expectMTLS(tde, 8, l8lc, l8mc);
}


// ------------------------- testLineEndLCoord -------------------------
void expectLELC(
  TextDocumentAndEditor &tde,
  int line,
  int expectCol)
{
  TextLCoord actual = tde.lineEndLCoord(line);
  EXPECT_EQ(actual.m_line, line);
  EXPECT_EQ(actual.m_column, expectCol);
}


void testLineEndLCoord()
{
  TextDocumentAndEditor tde;
  tde.insertNulTermText(
    "one\n"
    "two\t\n"
    "\tthree");

  expectLELC(tde, 0, 3);
  expectLELC(tde, 1, 8);
  expectLELC(tde, 2, 13);

  // Probing beyond EOF is explicitly allowed.
  expectLELC(tde, 3, 0);
  expectLELC(tde, 4, 0);
}


// ----------------------- testSelectEntireFile ------------------------
void testSelectEntireFile()
{
  EXN_CONTEXT("testSelectEntireFile");

  TextDocumentAndEditor tde;
  tde.setVisibleSize(2,3);

  tde.selectEntireFile();
  expectCursor(tde, 0,0); expectMark(tde, 0,0);
  checkCoord(tde.firstVisible(), TextLCoord(0,0), "firstVisible");

  tde.insertNulTermText(
    "one\n"
    "two\n"
    "three\n");

  // Due to the small window dimensions, inserting the above text should
  // leave the first visible line as "three".
  checkCoord(tde.firstVisible(), TextLCoord(2,0), "firstVisible");

  tde.selectEntireFile();
  expectCursor(tde, 0,0); expectMark(tde, 3,0);

  // `selectEntireFile` should scroll to make the cursor visible.
  checkCoord(tde.firstVisible(), TextLCoord(0,0), "firstVisible");

  tde.insertNulTermText(
    "one\n"
    "two");
  checkCoord(tde.firstVisible(), TextLCoord(0,1), "firstVisible");
  tde.selectEntireFile();
  expectCursor(tde, 0,0); expectMark(tde, 1,3);
  checkCoord(tde.firstVisible(), TextLCoord(0,0), "firstVisible");
}


CLOSE_ANONYMOUS_NAMESPACE


// Called from unit-tests.cc.
void test_td_editor(CmdlineArgsSpan args)
{
  testUndoRedo();
  testUndoOfPaste();
  testUndoOfBlockIndent();
  testTextManipulation();
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
  testInsertNewlineAutoIndentWithTab();
  testSetVisibleSize();
  testCursorRestorer();
  testSetMark();
  testConfineCursorToVisible();
  testJustifyNearCursor();
  testInsertDateTime();
  testReplaceText(false);
  testReplaceText(true);
  testReplaceAndSelect(false);
  testReplaceAndSelect(true);
  testCountSpaceChars();
  testCountSpaceCharsWithTabs();
  testGetSelectedOrIdentifier();
  testReadOnly();
  testLineLayout();
  testEditingWithTabs();
  testModelToLayoutSpans();
  testLineEndLCoord();
  testSelectEntireFile();
}


// EOF
