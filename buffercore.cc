// buffercore.cc
// code for buffercore.h

#include "buffercore.h"    // this module

#include "strutil.h"       // encodeWithEscapes
#include "syserr.h"        // xsyserror
#include "test.h"          // USUAL_MAIN, PVAL
#include "autofile.h"      // AutoFILE
#include "array.h"         // Array

#include <assert.h>        // assert
#include <ctype.h>         // isalnum, isspace
#include <string.h>        // strncasecmp


// ---------------------- TextDocumentCore --------------------------
TextDocumentCore::TextDocumentCore()
  : lines(),               // empty sequence of lines
    recent(-1),
    recentLine(),
    longestLengthSoFar(0),
    observers()
{
  // always at least one line; see comments at end of buffercore.h
  lines.insert(0 /*line*/, NULL /*value*/);
}

TextDocumentCore::~TextDocumentCore()
{
  // We require that the client code arrange to empty the observer list
  // before this object is destroyed.  If it is not empty here, the
  // observer will be left with a dangling pointer, which will cause
  // problems later of course.
  //
  // This uses ordinary 'assert' rather than 'xassert' to avoid
  // throwing an exception from within a destructor.
  assert(this->observers.isEmpty());

  // deallocate the non-NULL lines
  for (int i=0; i < lines.length(); i++) {
    char *p = lines.get(i);
    if (p) {
      delete[] p;
    }
  }
}


void TextDocumentCore::detachRecent()
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


void TextDocumentCore::attachRecent(int line, int insCol, int insLength)
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


int TextDocumentCore::lineLength(int line) const
{
  bc(line);

  if (line == recent) {
    return recentLine.length();
  }
  else {
    return bufStrlen(lines.get(line));
  }
}

STATICDEF int TextDocumentCore::bufStrlen(char const *p)
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


void TextDocumentCore::getLine(int line, int col, char *dest, int destLen) const
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


bool TextDocumentCore::locationInDefined(int line, int col) const
{
  return 0 <= line && line < numLines() &&
         0 <= col  && col <= lineLength(line);     // at EOL is ok
}


bool TextDocumentCore::locationAtEnd(int line, int col) const
{
  return line == numLines()-1 && col == lineLength(line);
}


// 'line' is marked 'const' to ensure its value is not changed before
// being passed to the observers; the same thing is done in the other
// three mutator functions; the C++ standard explicitly allows 'const'
// to be added here despite not having it in the .h file
void TextDocumentCore::insertLine(int const line)
{
  // insert a blank line
  lines.insert(line, NULL /*value*/);

  // adjust which line is 'recent'
  if (recent >= line) {
    recent++;
  }

  SFOREACH_OBJLIST_NC(TextDocumentObserver, observers, iter) {
    iter.data()->observeInsertLine(*this, line);
  }
}


void TextDocumentCore::deleteLine(int const line)
{
  if (line == recent) {
    xassert(recentLine.length() == 0);
    detachRecent();
  }

  // make sure line is empty
  xassert(lines.get(line) == NULL);

  // make sure we're not deleting the last line
  xassert(numLines() > 1);

  // remove the line
  lines.remove(line);

  // adjust which line is 'recent'
  if (recent > line) {
    recent--;
  }

  SFOREACH_OBJLIST_NC(TextDocumentObserver, observers, iter) {
    iter.data()->observeDeleteLine(*this, line);
  }
}


void TextDocumentCore::insertText(int const line, int const col,
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

  SFOREACH_OBJLIST_NC(TextDocumentObserver, observers, iter) {
    iter.data()->observeInsertText(*this, line, col, text, length);
  }
}


void TextDocumentCore::deleteText(int const line, int const col, int const length)
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

  SFOREACH_OBJLIST_NC(TextDocumentObserver, observers, iter) {
    iter.data()->observeDeleteText(*this, line, col, length);
  }
}


void TextDocumentCore::dumpRepresentation() const
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
           toCStr(encodeWithEscapes(p, len)));

    if (len) {
      delete[] p;
    }
  }

  fflush(stdout);
}


void TextDocumentCore::printMemStats() const
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


void TextDocumentCore::seenLineLength(int len)
{
  if (len > longestLengthSoFar) {
    longestLengthSoFar = len;
  }
}


// ------------------- TextDocumentCore utilities --------------------
void clear(TextDocumentCore &buf)
{
  while (buf.numLines() > 1) {
    buf.deleteText(0, 0, buf.lineLength(0));
    buf.deleteLine(0);
  }

  // delete contents of last remaining line
  buf.deleteText(0, 0, buf.lineLength(0));
}


void readFile(TextDocumentCore &buf, char const *fname)
{
  AutoFILE fp(fname, "rb");

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
        line++;
        buf.insertLine(line);
        col=0;
      }
      p = nl;
    }
    xassert(p == end);
  }
}


void writeFile(TextDocumentCore const &buf, char const *fname)
{
  AutoFILE fp(fname, "wb");

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


bool walkCursor(TextDocumentCore const &buf, int &line, int &col, int len)
{
  xassert(buf.locationInDefined(line, col));

  for (; len > 0; len--) {
    if (col == buf.lineLength(line)) {
      // cycle to next line
      line++;
      if (line >= buf.numLines()) {
        return false;      // beyond EOF
      }
      col=0;
    }
    else {
      col++;
    }
  }

  for (; len < 0; len++) {
    if (col == 0) {
      // cycle up to end of preceding line
      line--;
      if (line < 0) {
        return false;      // before BOF
      }
      col = buf.lineLength(line);
    }
    else {
      col--;
    }
  }

  return true;
}


void truncateCursor(TextDocumentCore const &buf, int &line, int &col)
{
  line = max(0, line);
  col = max(0, col);

  line = min(line, buf.numLines()-1);      // numLines>=1 always
  col = min(col, buf.lineLength(line));
}


bool getTextSpan(TextDocumentCore const &buf, int line, int col,
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


void computeSpaceFill(TextDocumentCore const &buf, int line, int col,
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


int computeSpanLength(TextDocumentCore const &buf, int line1, int col1,
                      int line2, int col2)
{
  xassert(line1 < line2 ||
          (line1==line2 && col1<=col2));

  if (line1 == line2) {
    return col2-col1;
  }

  // tail of first line
  int length = buf.lineLength(line1) - col1 +1;

  // line we're working on now
  line1++;

  // intervening complete lines
  for (; line1 < line2; line1++) {
    // because we keep deleting lines, the next one is always
    // called 'line'
    length += buf.lineLength(line1)+1;
  }

  // beginning of last line
  length += col2;

  return length;
}


// -------------------- TextDocumentObserver ------------------
void TextDocumentObserver::observeInsertLine(TextDocumentCore const &, int)
{}

void TextDocumentObserver::observeDeleteLine(TextDocumentCore const &, int)
{}

void TextDocumentObserver::observeInsertText(TextDocumentCore const &, int, int, char const *, int)
{}

void TextDocumentObserver::observeDeleteText(TextDocumentCore const &, int, int, int)
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
      TextDocumentCore buf;
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
    if (system("diff buffer.tmp buffer.tmp2") != 0) {
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
    TextDocumentCore buf;
    readFile(buf, "buffer.cc");
    buf.printMemStats();
  }

  printf("stats after:\n");
  malloc_stats();

  printf("\nbuffercore is ok\n");
}

USUAL_MAIN


#endif // TEST_BUFFERCORE
