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


void TextDocumentCore::clear()
{
  while (this->numLines() > 1) {
    this->deleteText(TextCoord(0, 0), this->lineLength(0));
    this->deleteLine(0);
  }

  // delete contents of last remaining line
  this->deleteText(TextCoord(0, 0), this->lineLength(0));
}


void TextDocumentCore::swapWith(TextDocumentCore &other) noexcept
{
  if (this != &other) {
    swap(this->lines, other.lines);
    swap(this->recent, other.recent);
    swap(this->recentLine, other.recentLine);
    swap(this->longestLengthSoFar, other.longestLengthSoFar);
  }

  SFOREACH_OBJLIST_NC(TextDocumentObserver, observers, iter) {
    iter.data()->observeTotalChange(*this);
  }
}


// For testing purposes, this can be set to a non-zero value, and after
// reading this many bytes, an error will be injected.
static int injectedErrorCountdown = 0;

void TextDocumentCore::nonAtomicReadFile(char const *fname)
{
  AutoFILE fp(fname, "rb");

  // clear only once the file has been successfully opened
  this->clear();

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

    if (injectedErrorCountdown > 0) {
      if (len < injectedErrorCountdown) {
        injectedErrorCountdown -= len;
      }
      else {
        injectedErrorCountdown = 0;
        xfailure("throwing injected error");
      }
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
      this->insertText(tc, p, nl-p);
      tc.column += nl-p;

      if (nl < end) {
        // skip newline
        nl++;
        tc.line++;
        this->insertLine(tc.line);
        tc.column=0;
      }
      p = nl;
    }
    xassert(p == end);
  }
}


void TextDocumentCore::readFile(char const *fname)
{
  TextDocumentCore tmp;

  // This will throw on error.
  tmp.nonAtomicReadFile(fname);

  // At this point success is guaranteed.
  this->swapWith(tmp);
}


void TextDocumentCore::writeFile(char const *fname) const
{
  AutoFILE fp(fname, "wb");

  // Buffer into which we will copy each line before writing it out.
  GrowArray<char> buffer(256 /*initial size*/);

  for (int line=0; line < this->numLines(); line++) {
    int len = this->lineLength(line);
    buffer.ensureIndexDoubler(len);       // text + possible newline

    this->getLine(TextCoord(line, 0), buffer.getArrayNC(), len);
    if (line < this->numLines()-1) {        // last gets no newline
      buffer[len] = '\n';
      len++;
    }

    if ((int)fwrite(buffer.getArray(), 1, len, fp) != len) {
      xsyserror("read", fname);
    }
  }
}


bool TextDocumentCore::getTextSpan(TextCoord tc, char *text, int textLen) const
{
  xassert(this->validCoord(tc));

  int offset = 0;
  while (offset < textLen) {
    // how many chars remain on this line?
    int thisLine = this->lineLength(tc.line) - tc.column;

    if (textLen-offset <= thisLine) {
      // finish off with text from this line
      this->getLine(tc, text+offset, textLen-offset);
      return true;
    }

    // get all of this line, plus a newline
    this->getLine(tc, text+offset, thisLine);
    offset += thisLine;
    text[offset++] = '\n';

    // move cursor to beginning of next line
    tc.line++;
    tc.column = 0;

    if (tc.line >= this->numLines()) {
      return false;     // text span goes beyond end of file
    }
  }

  return true;
}


void TextDocumentCore::addObserver(TextDocumentObserver *observer) const
{
  this->observers.appendUnique(observer);
}

void TextDocumentCore::removeObserver(TextDocumentObserver *observer) const
{
  this->observers.removeItem(observer);
}


void TextDocumentCore::notifyUnsavedChangesChange(TextDocument const *doc) const
{
  SFOREACH_OBJLIST_NC(TextDocumentObserver, this->observers, iter) {
    iter.data()->observeUnsavedChangesChange(doc);
  }
}


// -------------------- TextDocumentObserver ------------------
void TextDocumentObserver::observeInsertLine(TextDocumentCore const &, int) noexcept
{}

void TextDocumentObserver::observeDeleteLine(TextDocumentCore const &, int) noexcept
{}

void TextDocumentObserver::observeInsertText(TextDocumentCore const &, TextCoord, char const *, int) noexcept
{}

void TextDocumentObserver::observeDeleteText(TextDocumentCore const &, TextCoord, int) noexcept
{}

void TextDocumentObserver::observeTotalChange(TextDocumentCore const &doc) noexcept
{}

void TextDocumentObserver::observeUnsavedChangesChange(TextDocument const *doc) noexcept
{}


// --------------------- test code -----------------------
#ifdef TEST_TD_CORE

#include "ckheap.h"        // malloc_stats
#include <stdlib.h>        // system


static void testAtomicRead()
{
  // Write a file that spans several blocks.
  {
    AutoFILE fp("td-core.tmp", "wb");
    for (int i=0; i < 1000; i++) {
      fputs("                                       \n", fp);  // 40 bytes
    }
  }

  // Read it.
  TextDocumentCore core;
  core.readFile("td-core.tmp");
  xassert(core.numLines() == 1001);

  // Read it again with an injected error.
  try {
    injectedErrorCountdown = 10000;
    cout << "This should throw:" << endl;
    core.readFile("td-core.tmp");
    assert(!"should not get here");
  }
  catch (xBase &x) {
    // As expected.
  }

  // Should have consumed this.
  xassert(injectedErrorCountdown == 0);

  // Confirm that the original contents are still there.
  xassert(core.numLines() == 1001);

  remove("td-core.tmp");
}


static void entry()
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
      doc.readFile("td-core.tmp");

      // dump its repr
      //doc.dumpRepresentation();

      // write it out again
      doc.writeFile("td-core.tmp2");

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
    doc.readFile("td-core.cc");
    doc.printMemStats();
  }

  testAtomicRead();

  printf("stats after:\n");
  malloc_stats();

  printf("\ntd-core is ok\n");
}

USUAL_MAIN


#endif // TEST_TD_CORE
