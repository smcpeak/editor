// buffer.cc
// code for buffer.h

#include "buffer.h"        // this module

//#include "strutil.h"       // encodeWithEscapes
//#include "syserr.h"        // xsyserror
//#include "test.h"          // USUAL_MAIN, PVAL
//#include "autofile.h"      // AutoFILE
//#include "array.h"         // Array

#include <string.h>        // strncasecmp
#include <ctype.h>         // isalnum, isspace


// ---------------------- Buffer -------------------------
Buffer::Buffer()
  : HistoryBuffer()
{
  //insertLine(0);     // still need this hack?
}


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
  
  // NOTE: we do *not* allow line==numLines(); rather, it is expected
  // that the editor will make sure there's an extra line at the end
  // if it wants to let the user put the cursor on the "last" line
}


void Buffer::insertTextRange(char const *text)
{
  insertLR(false /*left*/, text, strlen(text));
}



class Grouper {
  Buffer &buf;
  
public:
  Grouper(Buffer &b) : buf(b)
    { buf.beginGroup(); }
  ~Grouper()
    { buf.endGroup(); }
};

#define GROUP_THESE Grouper grouper(*this)


void Buffer::insertNewline()
{
  // this pair of actions raises two points:
  //   - they should be a single action in the undo/redo history,
  //     so I need to implement grouping thing
  //   - maybe combining fill and insert/delete was a mistake,
  //     since it's certainly the uncommon case and could have
  //     been handled by grouping ...

  GROUP_THESE;

  int overEdge = col() - lineLengthLoose(line());
  if (overEdge > 0) {
    // move back to the end of this line
    moveCursor(true /*relLine*/, 0 /*line*/,
               true /*relCol*/, -overEdge /*col*/);
  }

  if (core().locationInDefined(line(), col())) {
    insertTextRange("\n");
  }
}


void Buffer::indentLine(int line, int ind)
{
  if (!( line < numLines() )) {
    return;
  }

  GROUP_THESE;

  while (ind > 0) {
    // insert lots of spaces at once, maybe to make the undo log
    // look nicer?
    static char const spaces[] = "                          ";
    int amt = min((int)sizeof(spaces), ind);
    xassert(amt > 0);
    insertText(line, 0, spaces, amt);
    ind -= amt;
  }

  if (ind < 0) {
    ind = -ind;
    int lineInd = getIndentation(line);
    if (lineInd >= 0) {                      
      // remove up to 'ind' chars of indentation
      deleteText(line, 0, min(ind, lineInd));
    }
    else {
      // no non-ws chars; remove up to 'ind' of them
      deleteText(line, 0, min(ind, lineLength(line)));
    }
  }
}


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


void Buffer::deleteTextRange(int line1, int col1, int line2, int col2)
{          
  pos(line1, col1);
  pos(line2, col2);

  if (line1 == line2) {
    // delete within one line
    xassert(col1 <= col2);
    if (line1 < numLines()) {
      int len = lineLength(line1);
      col1 = min(col1, len);
      col2 = min(col2, len);
      deleteText(line1, col1, col2-col1);
    }
    return;
  }

  xassert(line1 < line2);

  if (line1 >= numLines()) {
    return;
  }

  // delete tail of first line
  int len = lineLength(line1);
  col1 = min(col1, len);
  deleteText(line1, col1, len-col1);

  // line we're working on now
  int line = line1+1;

  // intervening complete lines
  for (int i=line; i < line2 && line < numLines(); i++) {
    // because we keep deleting lines, the next one is always
    // called 'line'
    deleteText(line, 0, lineLength(line));
    deleteLine(line);
  }

  // what was called 'line2' before is now called 'line'

  // delete beginning of last line
  if (line < numLines()) {
    deleteText(line, 0, min(col2, lineLength(line)));
  }

  // splice 'line' onto 'line1'
  spliceNextLine(line1);
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


void Buffer::advanceWithWrap(int &line, int &col, bool backwards) const
{
  if (!backwards) {
    if (0 <= line &&
        line < numLines() &&
        col < lineLength(line)) {
      col++;
    }
    else {
      line++;
      col = 0;
    }
  }

  else {
    if (0 <= line &&
        line < numLines() &&
        col >= 0) {
      col--;
    }
    else if (line > 0) {
      line--;
      col = lineLength(line);
    }
    else {
      line--;
      col = 0;
    }
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

  if (flags & FS_ADVANCE_ONCE) {
    advanceWithWrap(line, col, !!(flags & FS_BACKWARDS));
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


// -------------------- BufferObserver ------------------
void BufferObserver::insertLine(BufferCore const &, int)
{}

void BufferObserver::deleteLine(BufferCore const &, int)
{}

void BufferObserver::insertText(BufferCore const &, int, int, char const *, int)
{}

void BufferObserver::deleteText(BufferCore const &, int, int, int)
{}


// --------------------- test code -----------------------
#ifdef TEST_BUFFER

#include "ckheap.h"        // malloc_stats
#include <stdlib.h>        // system

// test Buffer::getTextRange
void testGetRange(Buffer &buf, int line1, int col1, int line2, int col2,
                               char const *expect)
{
  string actual = buf.getTextRange(line1, col1, line2, col2);
  if (!actual.equals(expect)) {
    buf.dumpRepresentation();
    cout << "getTextRange(" << line1 << "," << col1 << ", "
                            << line2 << "," << col2 << "):\n";
    cout << "  actual: " << quoted(actual) << "\n";
    cout << "  expect: " << quoted(expect) << "\n";
    exit(2);
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
    exit(2);
  }

  if (actual && (line!=ansLine || col!=ansCol)) {
    cout << "find(\"" << text << "\"): expected " << ansLine << ":" << ansCol
         << ", got " << line << ":" << col << endl;
    exit(2);
  }
}


void entry()
{
  // ---------- test BufferCore storage,read,write -------------
  printf("testing core functionality...\n");

  for (int looper=0; looper<2; looper++) {
    printf("stats before:\n");
    malloc_stats();

    // build a text file
    {
      AutoFILE fp("buffer.tmp", "w");

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
      // read it as a buffer
      Buffer buf;
      buf.readFile("buffer.tmp");

      // dump its repr
      //buf.dumpRepresentation();

      // write it out again
      buf.writeFile("buffer.tmp2");

      printf("stats before dealloc:\n");
      malloc_stats();

      printf("\nbuffer mem usage stats:\n");
      buf.printMemStats();
    }

    // make sure they're the same
    if (system("diff buffer.tmp buffer.tmp2 >/dev/null") != 0) {
      xbase("the files were different!\n");
    }

    // ok
    system("ls -l buffer.tmp");
    remove("buffer.tmp");
    remove("buffer.tmp2");

    printf("stats after:\n");
    malloc_stats();
  }

  {
    printf("reading buffer.cc ...\n");
    Buffer buf;
    buf.readFile("buffer.cc");
    buf.printMemStats();
  }

  printf("stats after:\n");
  malloc_stats();
  

  // ---------------- test Buffer convenience functions ----------------
  printf("\ntesting convenience functions...\n");

  Buffer buf;
  int line=0, col=0;
  buf.insertTextRange(line,col, "foo\nbar\n");
    // result: foo\n
    //         bar\n
  xassert(line == 2);
  xassert(col == 0);
  xassert(buf.numLines() == 3);    // so final 'line' is valid

  testGetRange(buf, 0,0, 2,0, "foo\nbar\n");
  testGetRange(buf, 0,1, 2,0, "oo\nbar\n");
  testGetRange(buf, 0,1, 1,3, "oo\nbar");
  testGetRange(buf, 0,3, 1,3, "\nbar");
  testGetRange(buf, 1,0, 1,3, "bar");
  testGetRange(buf, 1,2, 1,3, "r");
  testGetRange(buf, 1,3, 1,3, "");

  line=0; col=1;
  buf.insertTextRange(line, col, "arf\ngak");
    // result: farf\n
    //         gakoo\n
    //         bar\n
  xassert(line == 1);
  xassert(col == 3);
  xassert(buf.numLines() == 4);
  testGetRange(buf, 0,0, 3,0, "farf\ngakoo\nbar\n");

  buf.insertNewline(line, col);
    // result: farf\n
    //         gak\n
    //         oo\n
    //         bar\n
  xassert(line == 2);
  xassert(col == 0);
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

  line=0; col=0;
  buf.insertTextRange(line, col, "foofoofbar\n"
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

  printf("buffer is ok\n");
}

USUAL_MAIN


#endif // TEST_BUFFER
