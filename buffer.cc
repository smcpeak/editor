// buffer.cc
// code for buffer.h

#include "buffer.h"        // this module

#include "strutil.h"       // quoted
#include "array.h"         // GrowArray

#include <string.h>        // strncasecmp
#include <ctype.h>         // isalnum, isspace


// ---------------------- Buffer -------------------------
Buffer::Buffer()
  : HistoryBuffer()
{
  //insertLine(0);     // can I remove this hack?
}


STATICDEF void Buffer::pos(int line, int col)
{
  xassert(line >= 0);
  xassert(col >= 0);
}

void Buffer::bc(int line, int col) const
{
  pos(line, col);
  xassert(line < numLines());
  xassert(col <= lineLength(line));
}


// ---------------- Buffer: queries ------------------

int Buffer::lineLengthLoose(int line) const
{
  xassert(line >= 0);
  if (line < numLines()) {
    return lineLength(line);
  }
  else {
    return 0;
  }
}


void Buffer::getLineLoose(int line, int col, char *dest, int destLen) const
{
  pos(line, col);

  // how much of the source is in the defined region?
  int undef = destLen;
  int def = 0;

  if (line < numLines() &&
      col < lineLength(line)) {
    //       <----- lineLength -------->
    // line: [-------------------------][..spaces...]
    //       <------ col -----><----- destlen ------>
    //                         <- def -><-- undef -->
    // dest:                   [--------------------]

    undef = max(0, (col+destLen) - lineLength(line));
    def = max(0, destLen - undef);
  }

  // initial part in defined region
  if (def) {
    getLine(line, col, dest, def);
  }

  // spaces past defined region
  memset(dest+def, ' ', undef);
}


string Buffer::getTextRange(int line1, int col1, int line2, int col2) const
{
  pos(line1, col1);
  pos(line2, col2);

  // this function uses the line1==line2 case as a base case of a two
  // level recursion; it's not terribly efficient

  if (line1 == line2) {
    // extracting text from a single line
    xassert(col1 <= col2);
    string ret(col2-col1);     // NUL terminator added automatically
    getLineLoose(line1, col1, ret.pchar(), col2-col1);
    return ret;
  }

  xassert(line1 < line2);

  // build up returned string
  stringBuilder sb;

  // final fragment of line1
  sb = getTextRange(line1, col1, line1, max(col1, lineLengthLoose(line1)));

  // full lines between line1 and line2
  for (int i=line1+1; i < line2; i++) {
    sb << "\n";
    sb << getTextRange(i, 0, i, lineLengthLoose(i));
  }

  // initial fragment of line2
  sb << "\n";
  sb << getTextRange(line2, 0, line2, col2);

  return sb;
}


string Buffer::getWordAfter(int line, int col) const
{
  stringBuilder sb;
         
  if (!( 0 <= line && line < numLines() )) {
    return "";
  }
                                  
  bool seenWordChar = false;
  while (col < lineLength(line)) {
    char ch = getTextRange(line, col, line, col+1)[0];
    if (isalnum(ch) || ch=='_') {
      seenWordChar = true;
      sb << ch;
    }
    else if (seenWordChar) {
      // done, this is the end of the word
      break;
    }
    else {
      // consume this character, it precedes any word characters
      sb << ch;
    }
    col++;
  }

  return sb;
}


void Buffer::getLastPos(int &line, int &col) const
{
  line = numLines()-1;
  if (line >= 0) {
    col = lineLength(line);
  }
  else {
    col = 0;
  }
}


int Buffer::getIndentation(int line) const
{
  string contents = getWholeLine(line);
  for (char const *p = contents.pcharc(); *p; p++) {
    if (!isspace(*p)) {
      // found non-ws char
      return p - contents.pcharc();
    }
  }
  return -1;   // no non-ws
}


int Buffer::getAboveIndentation(int line) const
{
  while (line >= 0) {
    int ind = getIndentation(line);
    if (ind >= 0) {
      return ind;
    }
    line--;
  }
  return 0;
}


bool Buffer::findString(int &userLine, int &userCol, char const *text,
                        FindStringFlags flags) const
{
  int line = userLine;
  int col = userCol;
  int textLen = strlen(text);

  truncateCursor(core(), line, col);

  if (flags & FS_ADVANCE_ONCE) {
    walkCursor(core(), line, col,
               flags&FS_BACKWARDS? -1 : +1);
  }

  // contents of current line, in a growable buffer
  GrowArray<char> contents(10);

  while (0 <= line && line < numLines()) {
    // get line contents
    int lineLen = lineLength(line);
    contents.ensureIndexDoubler(lineLen);
    getLine(line, 0, contents.getDangerousWritableArray(), lineLen);

    // search for 'text' using naive algorithm, starting at 'col'
    while (0 <= col && col+textLen <= lineLen) {
      bool found =
        (flags & FS_CASE_INSENSITIVE) ?
          (0==strncasecmp(contents.getArray()+col, text, textLen)) :
          (0==strncmp(contents.getArray()+col, text, textLen))     ;

      if (found) {
        // found match
        userLine = line;
        userCol = col;
        return true;
      }

      if (flags & FS_BACKWARDS) {
        col--;
      }
      else {
        col++;
      }
    }

    if (flags & FS_ONE_LINE) {
      break;
    }

    // wrap to next line
    if (flags & FS_BACKWARDS) {
      line--;
      if (line >= 0) {
        col = lineLength(line)-textLen;
      }
    }
    else {
      col = 0;
      line++;
    }
  }

  return false;
}


// ---------------- Buffer: modifications ------------------

void Buffer::moveRelCursor(int deltaLine, int deltaCol)
{
  moveCursor(true /*relLine*/, deltaLine,
             true /*relCol*/, deltaCol);
}

void Buffer::moveAbsCursor(int newLine, int newCol)
{
  moveCursor(false /*relLine*/, newLine,
             false /*relCol*/, newCol);
}


void Buffer::moveRelCursorTo(int newLine, int newCol)
{
  moveRelCursor(newLine-line(), newCol-col());
}

void Buffer::moveToNextLineStart()
{
  moveCursor(true /*relLine*/, +1,
             false /*relCol*/, 0);
}

void Buffer::moveToPrevLineEnd()
{
  moveCursor(true /*relLine*/, -1,
             false /*relCol*/, lineLength(line()-1));
}


void Buffer::advanceWithWrap(bool backwards)
{
  if (!backwards) {
    if (0 <= line() &&
        line() < numLines() &&
        col() < lineLength(line())) {
      moveRelCursor(0, 1);
    }
    else {
      moveToNextLineStart();
    }
  }

  else {
    if (0 <= line() &&
        line() < numLines() &&
        col() >= 0) {
      moveRelCursor(0, -1);
    }
    else if (line() > 0) {
      moveToPrevLineEnd();
    }
    else {
      // cursor at buffer start.. do nothing, I guess
    }
  }
}


void Buffer::fillToCursor()
{
  int rowfill, colfill;
  computeSpaceFill(core(), line(), col(), rowfill, colfill);

  if (rowfill==0 && colfill==0) {
    return;     // nothing to do
  }

  int origLine = line();
  int origCol = col();

  // move back to defined area
  moveRelCursor(-rowfill, -colfill);
  bc();

  // add newlines
  while (rowfill--) {
    insertText("\n");
  }

  // add spaces
  while (colfill--) {
    insertSpace();
  }

  // should have ended up in the same place we started
  xassert(origLine==line() && origCol==col());
}


void Buffer::insertText(char const *text)
{
  insertLR(false /*left*/, text, strlen(text));
}



void Buffer::insertNewline()
{
  int overEdge = col() - lineLengthLoose(line());
  if (overEdge > 0) {
    // move back to the end of this line
    moveRelCursor(0, -overEdge);
  }

  fillToCursor();      // might add newlines up to this point
  insertText("\n");
}


#if 0     // old?
void Buffer::spliceNextLine(int line)
{
  xassert(0 <= line && line < numLines());

  // splice this line with the next
  if (line+1 < numLines()) {
    // append the contents of the next line
    int len = lineLength(line+1);
    Array<char> temp(len);
    getLine(line+1, 0 /*col*/, temp, len);
    insertText(line, lineLength(line), temp, len);

    // now remove the next line
    deleteText(line+1, 0 /*col*/, len);
    deleteLine(line+1);
  }
}
#endif // 0


void Buffer::deleteText(int len)
{
  deleteLR(false /*left*/, len);
}


void Buffer::deleteTextRange(int line1, int col1, int line2, int col2)
{
  pos(line1, col1);
  pos(line2, col2);
  xassert(line1 < line2 ||
          (line1==line2 && col1<=col2));

  // truncate the endpoints
  truncateCursor(core(), line1, col1);
  truncateCursor(core(), line2, col2);

  // go to line2/col2, which is probably where the cursor already is
  moveRelCursorTo(line2, col2);

  // compute # of chars in span
  int length = computeSpanLength(core(), line1, col1, line2, col2);

  // delete them as a left deletion; the idea is I suspect the
  // original and final cursor are line2/col2, in which case the
  // cursor movement can be elided (by automatic history compression)
  deleteLR(true /*left*/, length);

  // the cursor automatically ends up at line1/col1, as our spec
  // demands
}


void Buffer::indentLines(int start, int lines, int ind)
{
  if (start >= numLines() ||   // entire range beyond defined area
      lines <= 0 ||            // empty range
      ind == 0) {              // no actual change to the lines
    return;
  }

  CursorRestorer cr(*this);

  for (int line=start; line < start+lines &&
                       line < numLines(); line++) {
    moveRelCursorTo(line, 0);

    if (ind > 0) {
      for (int i=0; i<ind; i++) {
        insertSpace();
      }
    }

    else {
      int lineInd = getIndentation(line);
      for (int i=0; i<ind && i<lineInd; i++) {
        deleteChar();
      }
    }
  }
}


// -------------------- CursorRestorer ------------------
CursorRestorer::CursorRestorer(Buffer &b)
  : buf(b),
    origLine(b.line()),
    origCol(b.col())
{}

CursorRestorer::~CursorRestorer()
{
  buf.moveRelCursorTo(origLine, origCol);
}


// --------------------- test code -----------------------
#ifdef TEST_BUFFER

#include "ckheap.h"        // malloc_stats
#include "test.h"          // USUAL_MAIN

#include <stdlib.h>        // system

// test Buffer::getTextRange
void testGetRange(Buffer &buf, int line1, int col1, int line2, int col2,
                               char const *expect)
{
  string actual = buf.getTextRange(line1, col1, line2, col2);
  if (!actual.equals(expect)) {
    buf.core().dumpRepresentation();
    cout << "getTextRange(" << line1 << "," << col1 << ", "
                            << line2 << "," << col2 << "):\n";
    cout << "  actual: " << quoted(actual) << "\n";
    cout << "  expect: " << quoted(expect) << "\n";
    xfailure("testGetRange failed");
  }
}


// test Buffer::findString
void testFind(Buffer const &buf, int line, int col, char const *text,
              int ansLine, int ansCol, Buffer::FindStringFlags flags)
{
  bool expect = ansLine>=0;
  bool actual = buf.findString(line, col, text, flags);

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


void entry()
{
  Buffer buf;
  buf.insertText("foo\nbar\n");
    // result: foo\n
    //         bar\n
  xassert(buf.line() == 2);
  xassert(buf.col() == 0);
  xassert(buf.numLines() == 3);    // so final 'line' is valid

  testGetRange(buf, 0,0, 2,0, "foo\nbar\n");
  testGetRange(buf, 0,1, 2,0, "oo\nbar\n");
  testGetRange(buf, 0,1, 1,3, "oo\nbar");
  testGetRange(buf, 0,3, 1,3, "\nbar");
  testGetRange(buf, 1,0, 1,3, "bar");
  testGetRange(buf, 1,2, 1,3, "r");
  testGetRange(buf, 1,3, 1,3, "");

  buf.moveAbsCursor(0, 1);
  buf.insertText("arf\ngak");
    // result: farf\n
    //         gakoo\n
    //         bar\n
  xassert(buf.line() == 1);
  xassert(buf.col() == 3);
  xassert(buf.numLines() == 4);
  testGetRange(buf, 0,0, 3,0, "farf\ngakoo\nbar\n");

  buf.insertNewline();
    // result: farf\n
    //         gak\n
    //         oo\n
    //         bar\n
  xassert(buf.line() == 2);
  xassert(buf.col() == 0);
  xassert(buf.numLines() == 5);
  testGetRange(buf, 0,0, 4,0, "farf\ngak\noo\nbar\n");

  // some ranges that go beyond the defined area
  testGetRange(buf, 0,0, 5,0, "farf\ngak\noo\nbar\n\n");
  testGetRange(buf, 0,0, 6,0, "farf\ngak\noo\nbar\n\n\n");
  testGetRange(buf, 0,0, 6,2, "farf\ngak\noo\nbar\n\n\n  ");

  testGetRange(buf, 0,0, 2,5, "farf\ngak\noo   ");
  testGetRange(buf, 0,5, 2,5, "\ngak\noo   ");
  testGetRange(buf, 2,5, 2,10, "     ");
  testGetRange(buf, 2,10, 2,10, "");
  testGetRange(buf, 12,5, 12,10, "     ");
  testGetRange(buf, 12,5, 14,5, "\n\n     ");

  buf.deleteTextRange(1,1, 1,2);
    // result: farf\n
    //         gk\n
    //         oo\n
    //         bar\n
  testGetRange(buf, 0,0, 4,0, "farf\ngk\noo\nbar\n");
  xassert(buf.numLines() == 5);

  buf.deleteTextRange(0,3, 1,1);
    // result: fark\n
    //         oo\n
    //         bar\n
  testGetRange(buf, 0,0, 3,0, "fark\noo\nbar\n");
  xassert(buf.numLines() == 4);

  buf.deleteTextRange(1,3, 1,5);   // nop
    // result: fark\n
    //         oo\n
    //         bar\n
  testGetRange(buf, 0,0, 3,0, "fark\noo\nbar\n");
  xassert(buf.numLines() == 4);

  buf.deleteTextRange(2,2, 6,4);
    // result: fark\n
    //         oo\n
    //         ba
  testGetRange(buf, 0,0, 2,2, "fark\noo\nba");
  xassert(buf.numLines() == 3);

  buf.deleteTextRange(1,2, 2,2);
    // result: fark\n
    //         oo
  testGetRange(buf, 0,0, 1,2, "fark\noo");
  xassert(buf.numLines() == 2);

  buf.deleteTextRange(1,0, 1,2);
    // result: fark\n
  testGetRange(buf, 0,0, 1,0, "fark\n");
  xassert(buf.numLines() == 2);

  buf.deleteTextRange(0,0, 1,0);
    // result: <empty>
  testGetRange(buf, 0,0, 0,0, "");
  xassert(buf.numLines() == 1);
  xassert(buf.lineLength(0) == 0);

  
  Buffer::FindStringFlags
    none = Buffer::FS_NONE,
    insens = Buffer::FS_CASE_INSENSITIVE,
    back = Buffer::FS_BACKWARDS,
    advance = Buffer::FS_ADVANCE_ONCE;

  buf.moveAbsCursor(0,0);
  buf.insertText("foofoofbar\n"
                 "ooFoo arg\n");
  testFind(buf, 0,0, "foo", 0,0, none);
  testFind(buf, 0,1, "foo", 0,3, none);
  testFind(buf, 0,3, "foof", 0,3, none);
  testFind(buf, 0,4, "foof", -1,-1, none);
  testFind(buf, 0,0, "foofgraf", -1,-1, none);

  testFind(buf, 0,7, "foo", -1,-1, none);
  testFind(buf, 0,7, "foo", 1,2, insens);
  testFind(buf, 0,0, "foo", 0,3, advance);
  testFind(buf, 0,2, "foo", 0,0, back);
  testFind(buf, 0,3, "foo", 0,0, back|advance);
  testFind(buf, 0,4, "foo", 0,3, back|advance);
  testFind(buf, 1,3, "foo", 0,3, back);
  testFind(buf, 1,3, "foo", 1,2, back|insens);
  testFind(buf, 1,2, "foo", 0,3, back|insens|advance);
  testFind(buf, 1,3, "goo", -1,-1, back|insens|advance);
  testFind(buf, 1,3, "goo", -1,-1, back|insens);
  testFind(buf, 1,3, "goo", -1,-1, back);
  testFind(buf, 1,3, "goo", -1,-1, none);

  cout << "\nbuffer is ok\n";
}

USUAL_MAIN


#endif // TEST_BUFFER
