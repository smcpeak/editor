// td-core.cc
// code for td-core.h

#include "td-core.h"                   // this module

#include "array.h"                     // Array
#include "autofile.h"                  // AutoFILE
#include "strutil.h"                   // encodeWithEscapes
#include "syserr.h"                    // xsyserror
#include "test.h"                      // USUAL_MAIN, PVAL

#include <assert.h>                    // assert
#include <ctype.h>                     // isalnum, isspace
#include <string.h>                    // strncasecmp


// ---------------------- TextDocumentCore --------------------------
TextDocumentCore::TextDocumentCore()
  : lines(),               // empty sequence of lines
    recent(-1),
    recentLine(),
    longestLengthSoFar(0),
    observers()
{
  // always at least one line; see comments at end of td-core.h
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


void TextDocumentCore::attachRecent(TextCoord tc, int insLength)
{
  if (recent == tc.line) { return; }
  detachRecent();

  char const *p = lines.get(tc.line);
  int len = bufStrlen(p);
  if (len) {
    // copy contents into 'recentLine'
    recentLine.fillFromArray(p, len, tc.column, insLength);

    // deallocate the source
    delete[] p;
    lines.set(tc.line, NULL);
  }
  else {
    xassert(recentLine.length() == 0);
  }

  recent = tc.line;
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


void TextDocumentCore::getLine(TextCoord tc, char *dest, int destLen) const
{
  bc(tc.line);
  xassert(destLen >= 0);

  if (tc.line == recent) {
    recentLine.writeIntoArray(dest, destLen, tc.column);
  }
  else {
    char const *p = lines.get(tc.line);
    int len = bufStrlen(p);
    xassert(0 <= tc.column && tc.column + destLen <= len);

    memcpy(dest, p + tc.column, destLen);
  }
}


bool TextDocumentCore::validCoord(TextCoord tc) const
{
  return 0 <= tc.line && tc.line < numLines() &&
         0 <= tc.column  && tc.column <= lineLength(tc.line); // at EOL is ok
}


TextCoord TextDocumentCore::endCoord() const
{
  int line = this->numLines()-1;
  return TextCoord(line, this->lineLength(line));
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


void TextDocumentCore::insertText(TextCoord const tc,
                                  char const * const text,
                                  int const length)
{
  bctc(tc);

  #ifndef NDEBUG
    for (int i=0; i<length; i++) {
      xassert(text[i] != '\n');
    }
  #endif

  if (tc.column==0 && lineLength(tc.line)==0 && tc.line!=recent) {
    // setting a new line, can leave 'recent' alone
    char *p = new char[length+1];
    memcpy(p, text, length);
    p[length] = '\n';
    lines.set(tc.line, p);

    seenLineLength(length);
  }
  else {
    // use recent
    attachRecent(tc, length);
    recentLine.insertMany(tc.column, text, length);

    seenLineLength(recentLine.length());
  }

  SFOREACH_OBJLIST_NC(TextDocumentObserver, observers, iter) {
    iter.data()->observeInsertText(*this, tc, text, length);
  }
}


void TextDocumentCore::deleteText(TextCoord const tc, int const length)
{
  bctc(tc);

  if (tc.column==0 && length==lineLength(tc.line) && tc.line!=recent) {
    // removing entire line, no need to move 'recent'
    char *p = lines.get(tc.line);
    if (p) {
      delete[] p;
    }
    lines.set(tc.line, NULL);
  }
  else {
    // use recent
    attachRecent(tc, 0);
    recentLine.removeMany(tc.column, length);
  }

  SFOREACH_OBJLIST_NC(TextDocumentObserver, observers, iter) {
    iter.data()->observeDeleteText(*this, tc, length);
  }
}


void TextDocumentCore::dumpRepresentation() const
{
  printf("-- td-core --\n");

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
      getLine(TextCoord(i, 0), p, len);
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
      alloc = textBytes+1;                // for '\n'
      overheadBytes += sizeof(size_t);    // malloc's internal 'size' field
    }
    int sst = sizeof(size_t);
    intFragBytes = (alloc%sst)? (sst - alloc%sst) : 0;    // bytes to round up
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
void clear(TextDocumentCore &doc)
{
  while (doc.numLines() > 1) {
    doc.deleteText(TextCoord(0, 0), doc.lineLength(0));
    doc.deleteLine(0);
  }

  // delete contents of last remaining line
  doc.deleteText(TextCoord(0, 0), doc.lineLength(0));
}


void readFile(TextDocumentCore &doc, char const *fname)
{
  AutoFILE fp(fname, "rb");

  // clear only once the file has been successfully opened
  clear(doc);

  enum { BUFSIZE=0x2000 };     // 8k
  char buffer[BUFSIZE];

  // Location of the end of the document as it is being built.
  TextCoord tc(0,0);

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
      doc.insertText(tc, p, nl-p);
      tc.column += nl-p;

      if (nl < end) {
        // skip newline
        nl++;
        tc.line++;
        doc.insertLine(tc.line);
        tc.column=0;
      }
      p = nl;
    }
    xassert(p == end);
  }
}


void writeFile(TextDocumentCore const &doc, char const *fname)
{
  AutoFILE fp(fname, "wb");

  // Buffer into which we will copy each line before writing it out.
  GrowArray<char> buffer(256 /*initial size*/);

  for (int line=0; line < doc.numLines(); line++) {
    int len = doc.lineLength(line);
    buffer.ensureIndexDoubler(len);       // text + possible newline

    doc.getLine(TextCoord(line, 0), buffer.getArrayNC(), len);
    if (line < doc.numLines()-1) {        // last gets no newline
      buffer[len] = '\n';
      len++;
    }

    if ((int)fwrite(buffer.getArray(), 1, len, fp) != len) {
      xsyserror("read", fname);
    }
  }
}


bool getTextSpan(TextDocumentCore const &doc, TextCoord tc,
                 char *text, int textLen)
{
  xassert(doc.validCoord(tc));

  int offset = 0;
  while (offset < textLen) {
    // how many chars remain on this line?
    int thisLine = doc.lineLength(tc.line) - tc.column;

    if (textLen-offset <= thisLine) {
      // finish off with text from this line
      doc.getLine(tc, text+offset, textLen-offset);
      return true;
    }

    // get all of this line, plus a newline
    doc.getLine(tc, text+offset, thisLine);
    offset += thisLine;
    text[offset++] = '\n';

    // move cursor to beginning of next line
    tc.line++;
    tc.column = 0;

    if (tc.line >= doc.numLines()) {
      return false;     // text span goes beyond end of file
    }
  }

  return true;
}


int computeSpanLength(TextDocumentCore const &doc, TextCoord tc1,
                      TextCoord tc2)
{
  xassert(tc1 <= tc2);

  if (tc1.line == tc2.line) {
    return tc2.column-tc1.column;
  }

  // tail of first line
  int length = doc.lineLength(tc1.line) - tc1.column +1;

  // line we're working on now
  tc1.line++;

  // intervening complete lines
  for (; tc1.line < tc2.line; tc1.line++) {
    // because we keep deleting lines, the next one is always
    // called 'line'
    length += doc.lineLength(tc1.line)+1;
  }

  // beginning of last line
  length += tc2.column;

  return length;
}


// -------------------- TextDocumentObserver ------------------
void TextDocumentObserver::observeInsertLine(TextDocumentCore const &, int)
{}

void TextDocumentObserver::observeDeleteLine(TextDocumentCore const &, int)
{}

void TextDocumentObserver::observeInsertText(TextDocumentCore const &, TextCoord, char const *, int)
{}

void TextDocumentObserver::observeDeleteText(TextDocumentCore const &, TextCoord, int)
{}


// --------------------- test code -----------------------
#ifdef TEST_TD_CORE

#include "ckheap.h"        // malloc_stats
#include <stdlib.h>        // system

void entry()
{
  for (int looper=0; looper<2; looper++) {
    printf("stats before:\n");
    malloc_stats();

    // build a text file
    {
      AutoFILE fp("td-core.tmp", "w");

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
      // Read it as a text document.
      TextDocumentCore doc;
      readFile(doc, "td-core.tmp");

      // dump its repr
      //doc.dumpRepresentation();

      // write it out again
      writeFile(doc, "td-core.tmp2");

      printf("stats before dealloc:\n");
      malloc_stats();

      printf("\nbuffer mem usage stats:\n");
      doc.printMemStats();
    }

    // make sure they're the same
    if (system("diff td-core.tmp td-core.tmp2") != 0) {
      xbase("the files were different!\n");
    }

    // ok
    system("ls -l td-core.tmp");
    remove("td-core.tmp");
    remove("td-core.tmp2");

    printf("stats after:\n");
    malloc_stats();
  }

  {
    printf("reading td-core.cc ...\n");
    TextDocumentCore doc;
    readFile(doc, "td-core.cc");
    doc.printMemStats();
  }

  printf("stats after:\n");
  malloc_stats();

  printf("\ntd-core is ok\n");
}

USUAL_MAIN


#endif // TEST_TD_CORE
