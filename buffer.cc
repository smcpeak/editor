// buffer.cc
// code for buffer.h

#include "buffer.h"        // this module

#include "strutil.h"       // encodeWithEscapes
#include "syserr.h"        // xsyserror
#include "test.h"          // USUAL_MAIN, PVAL
#include "autofile.h"      // AutoFILE
#include "array.h"         // Array


// ---------------------- BufferCore --------------------------
BufferCore::BufferCore()
  : lines(),               // empty sequence of lines
    recent(-1),
    recentLine(),
    longestLengthSoFar(0)
{}

BufferCore::~BufferCore()
{
  // deallocate the non-NULL lines
  for (int i=0; i < lines.length(); i++) {
    char *p = lines.get(i);
    if (p) {
      delete[] p;
    }
  }
}


void BufferCore::detachRecent()
{
  if (recent == -1) { return; }

  xassert(lines.get(recent) == NULL);

  // copy 'recentLine' into lines[recent]
  int len = recentLine.length();
  if (len) {
    char *p = new char[len+1];
    p[len] = '\n';
    recentLine.writeIntoArray(p, len);
    xassert(p[len] == '\n');   // may as well check for overrun..

    lines.set(recent, p);

    recentLine.clear();
  }
  else {
    // it's already NULL, nothing needs to be done
  }

  recent = -1;
}


void BufferCore::attachRecent(int line, int insCol, int insLength)
{
  if (recent == line) { return; }
  detachRecent();

  char const *p = lines.get(line);
  int len = bufStrlen(p);
  if (len) {
    // copy contents into 'recentLine'
    recentLine.fillFromArray(p, len, insCol, insLength);

    // deallocate the source
    delete[] p;
    lines.set(line, NULL);
  }
  else {
    xassert(recentLine.length() == 0);
  }

  recent = line;
}


int BufferCore::lineLength(int line) const
{
  bc(line);

  if (line == recent) {
    return recentLine.length();
  }
  else {
    return bufStrlen(lines.get(line));
  }
}

STATICDEF int BufferCore::bufStrlen(char const *p)
{
  if (p) {
    int ret = 0;
    while (*p != '\n') {
      ret++;
      p++;
    }
    return ret;
  }
  else {
    return 0;
  }
}


void BufferCore::getLine(int line, int col, char *dest, int destLen) const
{
  bc(line);
  xassert(destLen >= 0);

  if (line == recent) {
    recentLine.writeIntoArray(dest, destLen, col);
  }
  else {
    char const *p = lines.get(line);
    int len = bufStrlen(p);
    xassert(0 <= col && col+destLen <= len);

    memcpy(dest, p+col, destLen);
  }
}


void BufferCore::insertLine(int line)
{
  // insert a blank line
  lines.insert(line, NULL /*value*/);

  // adjust which line is 'recent'
  if (recent >= line) {
    recent++;
  }
}


void BufferCore::deleteLine(int line)
{
  if (line == recent) {
    xassert(recentLine.length() == 0);
    detachRecent();
  }

  // make sure line is empty
  xassert(lines.get(line) == NULL);

  // remove the line
  lines.remove(line);

  // adjust which line is 'recent'
  if (recent > line) {
    recent--;
  }
}


void BufferCore::insertText(int line, int col, char const *text, int length)
{
  bc(line);

  #ifndef NDEBUG
    for (int i=0; i<length; i++) {
      xassert(text[i] != '\n');
    }
  #endif

  if (col==0 && lineLength(line)==0 && line!=recent) {
    // setting a new line, can leave 'recent' alone
    char *p = new char[length+1];
    memcpy(p, text, length);
    p[length] = '\n';
    lines.set(line, p);

    seenLineLength(length);
  }
  else {
    // use recent
    attachRecent(line, col, length);
    recentLine.insertMany(col, text, length);
    
    seenLineLength(recentLine.length());
  }
}


void BufferCore::deleteText(int line, int col, int length)
{
  bc(line);

  if (col==0 && length==lineLength(line) && line!=recent) {
    // removing entire line, no need to move 'recent'
    char *p = lines.get(line);
    if (p) {
      delete[] p;
    }
    lines.set(line, NULL);
  }
  else {
    // use recent
    attachRecent(line, col, 0);
    recentLine.removeMany(col, length);
  }
}


void BufferCore::dumpRepresentation() const
{
  printf("-- buffer --\n");

  // lines
  int L, G, R;
  lines.getInternals(L, G, R);
  printf("  lines: L=%d G=%d R=%d, num=%d\n", L,G,R, numLines());

  // recent
  recentLine.getInternals(L, G, R);
  printf("  recent=%d: L=%d G=%d R=%d, L+R=%d\n", recent, L,G,R, L+R);

  // line contents
  for (int i=0; i<numLines(); i++) {
    int len = lineLength(i);
    char *p = NULL;
    if (len) {
      p = new char[len];
      getLine(i, 0, p, len);
    }

    printf("  line %d: \"%s\"\n", i,
           encodeWithEscapes(p, len).pchar());

    if (len) {
      delete[] p;
    }
  }
}


void BufferCore::printMemStats() const
{
  // lines
  int L, G, R;
  lines.getInternals(L, G, R);
  int linesBytes = (L+G+R) * sizeof(char*);
  printf("  lines: L=%d G=%d R=%d, L+R=%d, bytes=%d\n", L,G,R, L+R, linesBytes);

  // recent
  recentLine.getInternals(L, G, R);
  int recentBytes = (L+G+R) * sizeof(char);
  printf("  recentLine: L=%d G=%d R=%d, bytes=%d\n", L,G,R, recentBytes);

  // line contents
  int textBytes = 0;
  int intFragBytes = 0;
  int overheadBytes = 0;

  for (int i=0; i<numLines(); i++) {
    char const *p = lines.get(i);

    textBytes += bufStrlen(p);

    int alloc = 0;
    if (p) {
      alloc = textBytes+1;   // for '\n'
      overheadBytes += 4;    // malloc's internal 'size' field
    }
    intFragBytes = (alloc%4)? (4 - alloc%4) : 0;    // bytes to round up to 4
  }

  PVAL(textBytes);
  PVAL(intFragBytes);
  PVAL(overheadBytes);

  printf("total: %d\n",
         linesBytes + recentBytes +
         textBytes + intFragBytes + overheadBytes);
}


void BufferCore::seenLineLength(int len)
{
  if (len > longestLengthSoFar) {
    longestLengthSoFar = len;
  }
}


// ---------------------- Buffer -------------------------
Buffer::Buffer()
  : BufferCore()
{
  insertLine(0);
}


void Buffer::readFile(char const *fname)
{
  AutoFILE fp(fname, "r");

  // assume the lines don't have embedded NULs
  enum { BUFSIZE=256 };
  char buffer[BUFSIZE];

  int line = 0;
  int col = 0;
  while (fgets(buffer, BUFSIZE, fp)) {
    insertTextRange(line, col, buffer);
  }
}


void Buffer::writeFile(char const *fname) const
{
  AutoFILE fp(fname, "w");

  enum { BUFSIZE=256 };
  char buffer[BUFSIZE];

  for (int line=0; line < numLines(); line++) {
    int len = min((int)BUFSIZE, lineLength(line));

    getLine(line, 0, buffer, len);
    fprintf(fp, "%.*s", len, buffer);
    if (line < numLines()-1) {    // last gets no newline
      fprintf(fp, "\n");
    }
  }
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


void Buffer::insertTextRange(int &line, int &col, char const *text)
{
  bc(line, col);

  while (*text) {
    // find next newline
    char const *nl = strchr(text, '\n');

    // length of this segment
    int len;
    if (nl) {
      len = nl-text;
    }
    else {
      len = strlen(text);    // no newline, take it all
    }

    // insert this text at line/col
    insertText(line, col, text, len);
    col += len;

    // insert newline
    if (nl) {
      insertNewline(line, col);
      len++;
    }

    // skip consumed text
    text += len;
  }
}


void Buffer::insertNewline(int &line, int &col)
{
  bc(line, col);

  if (col >= lineLength(line)) {
    // add a blank line after this one
    line++;
    col = 0;
    insertLine(line);
  }
  else if (col == 0) {
    // insert blank line and go to the next
    insertLine(line);
    line++;
  }
  else {
    // steal text after the cursor
    int len = lineLength(line) - col;
    Array<char> temp(len);
    getLine(line, col, temp, len);
    deleteText(line, col, len);

    // insert a line and move down
    line++;
    col = 0;
    insertLine(line);

    // insert the stolen text
    insertText(line, 0, temp, len);
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

  printf("buffer is ok\n");
}

USUAL_MAIN


#endif // TEST_BUFFER
