// test-td-editor.cc
// Tests for td-editor module.

#include "td-editor.h"                 // module to test

#include "datablok.h"                  // DataBlock
#include "ckheap.h"                    // malloc_stats
#include "strutil.h"                   // quoted

#include <unistd.h>                    // unlink
#include <stdio.h>                     // printf
#include <stdlib.h>                    // system


static void checkCoord(TextCoord expect, TextCoord actual, char const *label)
{
  if (expect != actual) {
    cout << "expect: " << expect << endl
         << "actual: " << actual << endl;
    xfailure(stringb(label << " coord mismatch"));
  }
}


static void expect(TextDocumentEditor const &tde, int line, int col, char const *text)
{
  TextCoord tc(line, col);
  checkCoord(tc, tde.cursor(), "cursor");

  writeFile(tde.doc()->getCore(), "td.tmp");
  DataBlock block;
  block.readFromFile("td.tmp");

  // compare contents to what is expected
  if (0!=memcmp(text, block.getDataC(), block.getDataLen()) ||
      (int)strlen(text)!=(int)block.getDataLen()) {
    xfailure("text mismatch");
  }
}


static void printHistory(TextDocumentEditor const &tde)
{
  stringBuilder sb;
  tde.doc()->printHistory(sb);
  cout << sb;
  cout.flush();
}


static void chars(TextDocumentEditor &tde, char const *str)
{
  while (*str) {
    tde.insertLR(false /*left*/, str, 1);
    str++;
  }
}


static void testUndoRedo()
{
  TextDocumentAndEditor tde;

  // This is just to avoid getting an "unused function" warning since
  // the calls are commented out.
  printHistory(tde);

  chars(tde, "abcd");
  //printHistory(tde);
  expect(tde, 0,4, "abcd");

  tde.undo();
  //printHistory(tde);
  expect(tde, 0,3, "abc");

  chars(tde, "e");
  //printHistory(tde);
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
  //printHistory(tde);

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

  //printHistory(tde);
  //tde.doc()->printHistoryStats();

  unlink("td.tmp");
}



// test TextDocumentEditor::getTextRange
static void testGetRange(TextDocumentEditor &tde, int line1, int col1,
                         int line2, int col2, char const *expect)
{
  string actual = tde.getTextRange(TextCoord(line1, col1), TextCoord(line2, col2));
  if (!actual.equals(expect)) {
    tde.doc()->getCore().dumpRepresentation();
    cout << "getTextRange(" << line1 << "," << col1 << ", "
                            << line2 << "," << col2 << "):\n";
    cout << "  actual: " << quoted(actual) << "\n";
    cout << "  expect: " << quoted(expect) << "\n";
    xfailure("testGetRange failed");
  }
}


// Test ::findString.
static void testFind(TextDocumentEditor const &buf, int line, int col,
                     char const *text, int ansLine, int ansCol,
                     TextDocumentEditor::FindStringFlags flags)
{
  bool expect = ansLine>=0;
  bool actual;
  {
    TextCoord tc(line, col);
    actual = buf.findString(tc, text, flags);
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

  tde.insertText("foo\nbar\n");
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
  tde.insertText("arf\ngak");
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
    advance = TextDocumentEditor::FS_ADVANCE_ONCE;

  tde.setCursor(TextCoord(0,0));
  tde.insertText("foofoofbar\n"
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
}


// Expect, including that the mark is inactive.
static void expectNM(TextDocumentEditor const &tde, int line, int col, char const *text)
{
  expect(tde, line, col, text);
  xassert(!tde.markActive());
}


// Expect, and mark is active.
static void expectM(TextDocumentEditor const &tde,
  int cursorLine, int cursorCol,
  int markLine, int markCol,
  char const *text)
{
  expect(tde, cursorLine, cursorCol, text);
  xassert(tde.markActive());
  checkCoord(TextCoord(markLine, markCol), tde.mark(), "mark");
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

  tde.insertText(
    "one\n"
    "two\n"
    "three\n");
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
}


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

  tde.insertText(
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


int main()
{
  try {
    testUndoRedo();
    testTextManipulation();
    testBlockIndent();
    testFillToCursor();

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
