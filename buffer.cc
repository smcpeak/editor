// buffer.cc
// code for buffer.h

#include "buffer.h"        // this module

#include "strutil.h"       // encodeWithEscapes
#include "syserr.h"        // xsyserror
#include "test.h"          // USUAL_MAIN, PVAL
#include "autofile.h"      // AutoFILE


Buffer::Buffer()
  : lines(),               // empty sequence of lines
    recent(-1),
    recentLine()
{}

Buffer::~Buffer()
{
  // deallocate the non-NULL lines
  for (int i=0; i < lines.length(); i++) {
    char *p = lines.get(i);
    if (p) {
      delete[] p;
    }
  }
}


void Buffer::detachRecent()
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


void Buffer::attachRecent(int line, int insCol, int insLength)
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


int Buffer::lineLength(int line) const
{
  bc(line);

  if (line == recent) {
    return recentLine.length();
  }
  else {
    return bufStrlen(lines.get(line));
  }
}

STATICDEF int Buffer::bufStrlen(char const *p)
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


void Buffer::getLine(int line, int col, char *dest, int destLen) const
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


void Buffer::insertLine(int line)
{
  // insert a blank line
  lines.insert(line, NULL /*value*/);

  // adjust which line is 'recent'
  if (recent >= line) {
    recent++;
  }
}


void Buffer::deleteLine(int line)
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


void Buffer::insertText(int line, int col, char const *text, int length)
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
  }
  else {
    // use recent
    attachRecent(line, col, length);
    recentLine.insertMany(col, text, length);
  }
}


void Buffer::deleteText(int line, int col, int length)
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


void Buffer::dumpRepresentation() const
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


void Buffer::printMemStats() const
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


void Buffer::readFile(char const *fname)
{
  AutoFILE fp(fname, "r");

  // assume the lines aren't very big, and don't have embedded NULs
  enum { BUFSIZE=256 };
  char buffer[BUFSIZE];

  int line = 0;
  while (fgets(buffer, BUFSIZE, fp)) {
    insertLine(line);

    int len = strlen(buffer);
    if (len && buffer[len-1]=='\n') {
      buffer[--len] = 0;
    }

    if (len) {
      insertText(line, 0 /*col*/, buffer, len);
    }
    line++;
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
    fprintf(fp, "%.*s\n", len, buffer);
  }
}


// --------------------- test code -----------------------
#ifdef TEST_BUFFER

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
}

USUAL_MAIN


#endif // TEST_BUFFER
