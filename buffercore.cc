// buffercore.cc
// code for buffercore.h

#include "buffercore.h"    // this module

#include "strutil.h"       // encodeWithEscapes
#include "syserr.h"        // xsyserror
#include "test.h"          // USUAL_MAIN, PVAL
#include "autofile.h"      // AutoFILE
#include "array.h"         // Array

#include <string.h>        // strncasecmp
#include <ctype.h>         // isalnum, isspace


// ---------------------- BufferCore --------------------------
BufferCore::BufferCore()
  : lines(),               // empty sequence of lines
    recent(-1),
    recentLine(),
    longestLengthSoFar(0),
    observers()
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


bool BufferCore::locationInDefined(int line, int col) const
{
  return 0 <= line && line < numLines() &&
         0 <= col  && col <= lineLength(line);     // at EOL is ok
}


// 'line' is marked 'const' to ensure its value is not changed before
// being passed to the observers; the same thing is done in the other
// three mutator functions; the C++ standard explicitly allows 'const'
// to be added here despite not having it in the .h file
void BufferCore::insertLine(int const line)
{
  // insert a blank line
  lines.insert(line, NULL /*value*/);

  // adjust which line is 'recent'
  if (recent >= line) {
    recent++;
  }

  SFOREACH_OBJLIST_NC(BufferObserver, observers, iter) {
    iter.data()->insertLine(*this, line);
  }
}


void BufferCore::deleteLine(int const line)
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

  SFOREACH_OBJLIST_NC(BufferObserver, observers, iter) {
    iter.data()->deleteLine(*this, line);
  }
}


void BufferCore::insertText(int const line, int const col,
                            char const * const text, int const length)
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

  SFOREACH_OBJLIST_NC(BufferObserver, observers, iter) {
    iter.data()->insertText(*this, line, col, text, length);
  }
}


void BufferCore::deleteText(int const line, int const col, int const length)
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

  SFOREACH_OBJLIST_NC(BufferObserver, observers, iter) {
    iter.data()->deleteText(*this, line, col, length);
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


// ------------------- BufferCore utilities --------------------
void clear(BufferCore &buf)
{
  while (buf.numLines() > 0) {
    buf.deleteText(0, 0, buf.lineLength(0));
    buf.deleteLine(0);
  }
}


void readFile(BufferCore &buf, char const *fname)
{
  AutoFILE fp(fname, "r");

  // clear only once the file has been successfully opened
  clear(buf);

  enum { BUFSIZE=0x2000 };     // 8k
  char buffer[BUFSIZE];

  int line = 0;
  int col = 0;
  
  for (;;) {
    int len = fread(buffer, 1, BUFSIZE, fp);
    if (len == 0) {
      break;
    }
    if (len < 0) {
      xsyserror("read", fname);
    }

    char const *p = buffer;
    char const *end = buffer+len;
    while (p < end) {
      // find next newline, or end of this buffer segment
      char const *nl = p;
      while (nl < end && *nl!='\n') {
        nl++;
      }

      // insert this line fragment
      buf.insertText(line, col, p, nl-p);
      col += nl-p;

      if (nl < end) {
        // skip newline
        nl++;
        buf.insertLine(line);
        line++;
        col=0;
      }
      p = nl;
    }
    xassert(p == end);
  }
}


void writeFile(BufferCore const &buf, char const *fname)
{
  AutoFILE fp(fname, "w");

  GrowArray<char> buffer(256 /*initial size*/);

  for (int line=0; line < buf.numLines(); line++) {
    int len = buf.lineLength(line);
    buffer.ensureIndexDoubler(len);       // text + possible newline

    buf.getLine(line, 0, buffer.getArrayNC(), len);
    if (line < buf.numLines()-1) {        // last gets no newline
      buffer[len] = '\n';
      len++;
    }

    if ((int)fwrite(buffer.getArray(), 1, len, fp) != len) {
      xsyserror("read", fname);
    }
  }
}


bool walkBackwards(BufferCore const &buf, int &line, int &col, int textLen)
{
  xassert(buf.locationInDefined(line, col));

  for (; textLen > 0; textLen--) {
    if (col == 0) {
      // cycle up to end of preceding line
      line--;
      if (line < 0) {
        return false;
      }
      col = buf.lineLength(line);
    }
    else {
      // move left
      col--;
    }
  }

  return true;
}


bool getTextSpan(BufferCore const &buf, int line, int col,
                 char *text, int textLen)
{
  xassert(buf.locationInDefined(line, col));

  int offset = 0;
  while (offset < textLen) {
    // how many chars remain on this line?
    int thisLine = buf.lineLength(line) - col;

    if (textLen-offset <= thisLine) {
      // finish off with text from this line
      buf.getLine(line, col, text+offset, textLen-offset);
      return true;
    }

    // get all of this line, plus a newline
    buf.getLine(line, col, text+offset, thisLine);
    offset += thisLine;
    text[offset++] = '\n';

    // move cursor to beginning of next line
    line++;
    col = 0;

    if (line >= buf.numLines()) {
      return false;     // text span goes beyond end of file
    }
  }

  return true;
}


void computeSpaceFill(CursorBuffer &buf, int line, int col,
                      int &rowfill, int &colfill)
{
  if (line < buf.numLines()) {
    // case 1: only need to add spaces to the end of some line
    int diff = col - buf.lineLength(line);
    if (diff < 0) {
      diff = 0;
    }
    rowfill = 0;
    colfill = diff;
  }

  else {
    // case 2: need to add lines, then possibly add spaces
    rowfill = (line - buf.numLines() + 1);    // # of lines to add
    colfill = col;                            // # of cols to add
  }

  xassert(rowfill >= 0);
  xassert(colfill >= 0);
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
#ifdef TEST_BUFFERCORE

#include "ckheap.h"        // malloc_stats
#include <stdlib.h>        // system

void entry()
{
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
      BufferCore buf;
      readFile(buf, "buffer.tmp");

      // dump its repr
      //buf.dumpRepresentation();

      // write it out again
      writeFile(buf, "buffer.tmp2");

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
    readFile(buf, "buffer.cc");
    buf.printMemStats();
  }

  printf("stats after:\n");
  malloc_stats();
}

USUAL_MAIN


#endif // TEST_BUFFERCORE
