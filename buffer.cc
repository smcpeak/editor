// buffer.cc
// code for buffer.h

#include "buffer.h"        // this module
#include "textline.h"      // TextLine
#include "position.h"      // Position

#include "strutil.h"       // encodeWithEscapes
#include "syserr.h"        // xsyserror
#include "ckheap.h"        // checkHeap
#include "test.h"          // USUAL_MAIN, PVAL

#include <sys/types.h>     // ?
#include <sys/stat.h>      // O_RDONLY, etc.
#include <fcntl.h>         // open, close
#include <unistd.h>        // read, write
#include <stdio.h>         // FILE, etc.
#include <stdlib.h>        // system
#include <string.h>        // memmove


void Buffer::init()
{
  lines = NULL;
  numLines = 0;
  linesAllocated = 0;
  changed = false;
}

Buffer::~Buffer()
{
  for (int i=0; i<numLines; i++) {
    lines[i].dealloc();
  }
  delete[] lines;
}


Buffer::Buffer(char const *initText, int initLen)
{
  init();

  // insert with a 0,0 position
  Position position(this);
  insertText(position, initText, initLen);
}


bool Buffer::operator == (Buffer const &obj) const
{
  if (numLines != obj.numLines) {
    return false;
  }

  for (int i=0; i<numLines; i++) {
    if (lines[i] != obj.lines[i]) {
      return false;
    }
  }

  return true;
}


// adjustable parameters (don't parenthesize)
#define LINES_SHRINK_RATIO 1 / 4
#define LINES_GROW_RATIO 3 / 2
#define LINES_GROW_STEP 20


// always uses a margin calculation
void Buffer::setNumLines(int newLines)
{
  if (newLines == numLines) { return; }
  changed = true;

  // dealloc any lines now not part of the buffer
  while (numLines > newLines) {
    numLines--;
    lines[numLines].dealloc();
  }

  if (linesAllocated < newLines  ||
      newLines < linesAllocated * LINES_SHRINK_RATIO - LINES_GROW_STEP) {
    int newAllocated = newLines * LINES_GROW_RATIO + LINES_GROW_STEP;

    // realloc and copy
    TextLine *newArray = new TextLine[newAllocated];

    // copy common prefix ('numLines' lines)
    memcpy(newArray, lines, numLines * sizeof(TextLine));

    // dealloc old array
    if (lines) {
      delete[] lines;    // does *not* call dtors!  essential!
    }

    // reassign to new
    lines = newArray;
    linesAllocated = newAllocated;
  }

  // init any lines that are now part of the buffer
  while (numLines < newLines) {
    lines[numLines].init();
    numLines++;
  }
}


void Buffer::readFile(char const *fname)
{
  // use raw 'read' calls and my own buffer to avoid unnecessary
  // overhead from FILE functions; a large buffer will minimize
  // the IPC (interprocess communication)

  #ifdef TEST_BUFFER
    enum { BUFSIZE=16 };       // small so I can easily test boundary issues
  #else
    enum { BUFSIZE=8192 };
  #endif
  char buf[BUFSIZE];

  int fd = open(fname, O_RDONLY);
  if (fd < 0) {
    throw_XOpen(fname);
  }

  // make a position to keep track of where to insert
  Position position(this);       // inits to 0,0

  for (;;) {
    int len = read(fd, buf, BUFSIZE);
    if (len == 0) {
      break;    // EOF
    }
    if (len < 0) {
      xsyserror("read");
    }

    // this updates the position
    insertText(position, buf, len);
  }

  if (close(fd) < 0) {
    xsyserror("close");
  }

  changed = false;
}


void Buffer::writeFile(char const *fname) const
{
  // use FILE ops for writing so we don't do an IPC for
  // each line; in this case automatic buffering is good

  FILE *fp = fopen(fname, "w");
  if (!fp) {
    throw_XOpen(fname);
  }

  for (int line=0; line<numLines; line++) {
    TextLine const &tl = lines[line];
    if (tl.getLength()) {
      if (1 != fwrite(tl.getText(), tl.getLength(), 1, fp)) {
        xsyserror("write");
      }
    }

    if (line+1 < numLines) {
      // newline separator
      if (1 != fwrite("\n", 1, 1, fp)) {
        xsyserror("write");
      }
    }
  }

  if (0 != fclose(fp)) {
    xsyserror("close");
  }

  // caller must do this, because this method is 'const', and I
  // don't want to make it non-const, nor make 'changed' mutable
  //changed = false;
}


void Buffer::dumpRepresentation() const
{
  printf("-- buffer -- (changed: %s)\n",
         changed? "yes" : "no");

  for (int i=0; i<numLines; i++) {
    printf("line %d: [%d/%d] \"%s\"\n",
           i, lines[i].getLength(), lines[i]._please_getAllocated(),
           encodeWithEscapes(lines[i].getText(),
                             lines[i].getLength()).pchar());
  }
}


TextLine const *Buffer::getLineC(int n) const
{
  xassert(n < numLines);

  return lines + n;
}


void Buffer::ensureLineExists(int n)
{
  if (n >= numLines) {
    setNumLines(n+1);
  }
}


void Buffer::insertLinesAt(int n, int howmany)
{
  changed = true;

  if (n >= numLines) {
    // easy case: just expand the array
    setNumLines(n+howmany);
  }
  else {
    // harder case: must move existing lines down

    // first, make room
    setNumLines(numLines + howmany);

    // then shift the original lines below the insertion
    // point downward
    memmove(lines+n+howmany,                // dest
            lines+n,                        // src
            howmany * sizeof(TextLine));    // size to move

    // the lines in the gap now are defunct, though they
    // still point to some of the shifted lines; re-init
    // them so they are empty and so they don't deallocate
    // the pointed-at data
    for (int i=n; i < n+howmany; i++) {
      lines[i].init();
    }
  }
}


void Buffer::insertText(Position &c, char const *text, int length)
{
  // 'changed' automatically set to true by accessors

  int curLine = c.line();
  int curCol = c.col();

  ensureLineExists(curLine);

  // mark the start of the new text to assimilate
  char const *start = text;

  // loop over all line fragments in the buffer
  while (start < text+length) {
    // look for a newline
    char const *end = start;
    while (*end != '\n' &&
           end < text+length) {
      end++;
    }

    if (end < text+length) {
      // we found a newline

      if (curCol == 0) {
        // push subsequent lines down
        insertLineAt(curLine);

        // set a complete line, with no margin
        lines[curLine].setText(start, end-start);
      }

      else {
        int oldLen = lines[curLine].getLength();

        // push the rest of the current line down into the next line
        insertLineAt(curLine+1);
        lines[curLine+1].setText(lines[curLine].getText()+curCol,
                                 oldLen-curCol);

        // remove that text from the end of this line
        lines[curLine].remove(curCol, oldLen-curCol);

        // append to what's still in this line, and take
        // the hit of the margin
        lines[curLine].insert(curCol, start, end-start);
      }

      // move our insertion point into the newly-created line
      curLine++;
      curCol = 0;

      // move our read-from-next point
      start = end+1;                      // skip newline char
    }

    else {
      // we ran into the end of the buffer; add this text
      // but don't move to the next line
      lines[curLine].insert(curCol, start, end-start);

      // move insertion point w/in the line
      curCol += end-start;

      // and read-from-next point (will cause us to
      // break the inner loop)
      start = end;
    }
  }

  // update the position
  c.set(curLine, curCol);

  // POSSIBLE TODO: update other positions, based on the insertion
}


void Buffer::deleteText(Position const &c1, Position &c2)
{
  // 'changed' automatically set to true by accessors

  xassert(c1 <= c2);

  if (c1.beyondEnd()) {
    // nothing out here
    return;
  }

  if (c1.beyondLineEnd()) {
    // insert spaces to the left of c1
    c1.getBufLine()->setLengthNoMargin(c1.col());
  }

  c2.clampToText();

  xassert(c1.inText() &&
          c2.inText() &&
          c1 <= c2);

  if (c1.line() == c2.line()) {
    // easy case: intra-line deletion
    lines[c1.line()].remove(c1.col(), c2.col() - c1.col());
  }

  else {
    // at least one line will be deleted

    // first, delete the right half of c1's line
    lines[c1.line()].remove(
      c1.col(),                                   // start
      lines[c1.line()].getLength() - c1.col());  // len to del

    // splice the 2nd half of c2's line onto the 1st half of c1
    lines[c1.line()].insert(
      c1.col(),                                   // ins point
      lines[c2.line()].getText() + c2.col(),     // text
      lines[c2.line()].getLength() - c2.col());  // text length

    // remove all of the lines between c1 and c2, excluding
    // c1's line but including c2's line
    removeLines(c1.line() + 1, c2.line() - c1.line());
  }

  // update c2 (kind of degenerate..)
  c2 = c1;
}


void Buffer::removeLines(int startLine, int linesToRemove)
{
  xassert(linesToRemove >= 0);

  ensureLineExists(startLine);
  if (startLine + linesToRemove >= numLines) {
    // trim to what's there
    linesToRemove = numLines - (startLine+1);
  }

  if (linesToRemove == 0) {
    return;
  }
  changed = true;

  // deallocate the lines being removed
  for (int i=0; i<linesToRemove; i++) {
    lines[startLine+i].dealloc();
  }

  // move the 2nd half lines up to cover the gap
  memmove(lines+startLine,                    // dest
          lines+startLine+linesToRemove,      // src
          (numLines - linesToRemove) * sizeof(TextLine));  // bytes to move

  // we'll simply adjust the # of lines; this should cause
  // us to ignore the (valid-looking) lines beyond the end
  numLines -= linesToRemove;
}


void Buffer::clear()
{
  removeLines(0, numLines);
}


void Buffer::printMemStats() const
{
  PVAL(numLines);
  PVAL(linesAllocated);
  PVAL(linesAllocated * sizeof(TextLine));

  int textBytes = 0;
  int allocBytes = 0;
  int intFragBytes = 0;
  int overheadBytes = 0;

  for (int i=0; i<numLines; i++) {
    TextLine const &tl = lines[i];

    textBytes += tl.getLength();

    int alloc = tl._please_getAllocated();
    allocBytes += alloc;
    intFragBytes = (alloc%4)? (4 - alloc%4) : 0;    // bytes to round up to 4
    overheadBytes += 4;
  }

  PVAL(textBytes);
  PVAL(allocBytes);
  PVAL(intFragBytes);
  PVAL(overheadBytes);
  
  printf("total: %d\n",
         linesAllocated * sizeof(TextLine) +
         allocBytes + intFragBytes + overheadBytes);
}


// --------------------- test code -----------------------
#ifdef TEST_BUFFER

#include "malloc.h"    // malloc_stats

void entry()
{
  for (int looper=0; looper<2; looper++) {
    printf("stats before:\n");
    malloc_stats();

    // build a text file
    FILE *fp = fopen("buffer.tmp", "w");
    if (!fp) {
      xsyserror("open");
    }

    for (int i=0; i<2; i++) {
      for (int j=0; j<53; j++) {
        for (int k=0; k<j; k++) {
          fputc('0' + (k%10), fp);
        }
        fputc('\n', fp);
      }
    }
    fprintf(fp, "last line no newline");

    fclose(fp);

    {
      // read it as a buffer
      Buffer buf;
      buf.readFile("buffer.tmp");

      // dump its repr
      buf.dumpRepresentation();

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


    // make a buffer for more testing..
    //#define STR(str) str, strlen(str)
    //Buffer buf(STR("hi\nthere\nfoo\nbar\n"));












    printf("stats after:\n");
    malloc_stats();
  }
  
  {                     
    printf("reading buffer.cc ...\n");
    Buffer buf;
    buf.readFile("buffer.cc");
    buf.printMemStats();              
  }
}

USUAL_MAIN


#endif // TEST_BUFFER
