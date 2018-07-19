// td-core.cc
// code for td-core.h

#include "td-core.h"                   // this module

// smbase
#include "array.h"                     // Array
#include "autofile.h"                  // AutoFILE
#include "objcount.h"                  // CheckObjectCount
#include "strutil.h"                   // encodeWithEscapes
#include "syserr.h"                    // xsyserror
#include "test.h"                      // USUAL_MAIN, PVAL

// libc
#include <assert.h>                    // assert
#include <ctype.h>                     // isalnum, isspace
#include <string.h>                    // strncasecmp


// ---------------------- TextDocumentCore --------------------------
int TextDocumentCore::s_injectedErrorCountdown = 0;

TextDocumentCore::TextDocumentCore()
  : m_lines(),               // empty sequence of lines
    m_recent(-1),
    m_recentLine(),
    m_longestLengthSoFar(0),
    m_observers()
{
  // always at least one line; see comments at end of td-core.h
  m_lines.insert(0 /*line*/, NULL /*value*/);
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
  assert(this->m_observers.isEmpty());

  // deallocate the non-NULL lines
  for (int i=0; i < m_lines.length(); i++) {
    char *p = m_lines.get(i);
    if (p) {
      delete[] p;
    }
  }
}


void TextDocumentCore::detachRecent()
{
  if (m_recent == -1) { return; }

  xassert(m_lines.get(m_recent) == NULL);

  // copy 'recentLine' into lines[recent]
  int len = m_recentLine.length();
  if (len) {
    char *p = new char[len+1];
    p[len] = '\n';
    m_recentLine.writeIntoArray(p, len);
    xassert(p[len] == '\n');   // may as well check for overrun..

    m_lines.set(m_recent, p);

    m_recentLine.clear();
  }
  else {
    // it's already NULL, nothing needs to be done
  }

  m_recent = -1;
}


void TextDocumentCore::attachRecent(TextCoord tc, int insLength)
{
  if (m_recent == tc.line) { return; }
  detachRecent();

  char const *p = m_lines.get(tc.line);
  int len = bufStrlen(p);
  if (len) {
    // copy contents into 'recentLine'
    m_recentLine.fillFromArray(p, len, tc.column, insLength);

    // deallocate the source
    delete[] p;
    m_lines.set(tc.line, NULL);
  }
  else {
    xassert(m_recentLine.length() == 0);
  }

  m_recent = tc.line;
}


int TextDocumentCore::lineLength(int line) const
{
  bc(line);

  if (line == m_recent) {
    return m_recentLine.length();
  }
  else {
    return bufStrlen(m_lines.get(line));
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

  if (tc.line == m_recent) {
    m_recentLine.writeIntoArray(dest, destLen, tc.column);
  }
  else {
    char const *p = m_lines.get(tc.line);
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


// Interestingly, this is *not* what "wc -l" returns.  Instead, wc -l
// returns a count of the newline characters.  But that seems like a bug
// in 'wc' to me.
int TextDocumentCore::numLinesExceptFinalEmpty() const
{
  int lastLine = this->numLines()-1;
  if (this->lineLength(lastLine) == 0) {
    return lastLine;
  }
  else {
    return lastLine+1;
  }
}


// This is tested in 'test-td-editor'.  I think of it primarily as a
// service provided by TextDocumentEditor, but occasionally I need to
// do this with a TextDocumentCore, so it is implemented here.
bool TextDocumentCore::walkCoord(TextCoord &tc, int len) const
{
  xassert(this->validCoord(tc));

  for (; len > 0; len--) {
    if (tc.column == this->lineLength(tc.line)) {
      // cycle to next line
      tc.line++;
      if (tc.line >= this->numLines()) {
        return false;      // beyond EOF
      }
      tc.column=0;
    }
    else {
      tc.column++;
    }
  }

  for (; len < 0; len++) {
    if (tc.column == 0) {
      // cycle up to end of preceding line
      tc.line--;
      if (tc.line < 0) {
        return false;      // before BOF
      }
      tc.column = this->lineLength(tc.line);
    }
    else {
      tc.column--;
    }
  }

  return true;
}


// 'line' is marked 'const' to ensure its value is not changed before
// being passed to the observers; the same thing is done in the other
// three mutator functions; the C++ standard explicitly allows 'const'
// to be added here despite not having it in the .h file
void TextDocumentCore::insertLine(int const line)
{
  // insert a blank line
  m_lines.insert(line, NULL /*value*/);

  // adjust which line is 'recent'
  if (m_recent >= line) {
    m_recent++;
  }

  FOREACH_RCSERFLIST_NC(TextDocumentObserver, m_observers, iter) {
    iter.data()->observeInsertLine(*this, line);
  }
}


void TextDocumentCore::deleteLine(int const line)
{
  if (line == m_recent) {
    xassert(m_recentLine.length() == 0);
    detachRecent();
  }

  // make sure line is empty
  xassert(m_lines.get(line) == NULL);

  // make sure we're not deleting the last line
  xassert(numLines() > 1);

  // remove the line
  m_lines.remove(line);

  // adjust which line is 'recent'
  if (m_recent > line) {
    m_recent--;
  }

  FOREACH_RCSERFLIST_NC(TextDocumentObserver, m_observers, iter) {
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

  if (tc.column==0 && lineLength(tc.line)==0 && tc.line!=m_recent) {
    // setting a new line, can leave 'recent' alone
    char *p = new char[length+1];
    memcpy(p, text, length);
    p[length] = '\n';
    m_lines.set(tc.line, p);

    seenLineLength(length);
  }
  else {
    // use recent
    attachRecent(tc, length);
    m_recentLine.insertMany(tc.column, text, length);

    seenLineLength(m_recentLine.length());
  }

  FOREACH_RCSERFLIST_NC(TextDocumentObserver, m_observers, iter) {
    iter.data()->observeInsertText(*this, tc, text, length);
  }
}


void TextDocumentCore::deleteText(TextCoord const tc, int const length)
{
  bctc(tc);

  if (tc.column==0 && length==lineLength(tc.line) && tc.line!=m_recent) {
    // removing entire line, no need to move 'recent'
    char *p = m_lines.get(tc.line);
    if (p) {
      delete[] p;
    }
    m_lines.set(tc.line, NULL);
  }
  else {
    // use recent
    attachRecent(tc, 0);
    m_recentLine.removeMany(tc.column, length);
  }

  FOREACH_RCSERFLIST_NC(TextDocumentObserver, m_observers, iter) {
    iter.data()->observeDeleteText(*this, tc, length);
  }
}


void TextDocumentCore::dumpRepresentation() const
{
  printf("-- td-core --\n");

  // lines
  int L, G, R;
  m_lines.getInternals(L, G, R);
  printf("  lines: L=%d G=%d R=%d, num=%d\n", L,G,R, numLines());

  // recent
  m_recentLine.getInternals(L, G, R);
  printf("  recent=%d: L=%d G=%d R=%d, L+R=%d\n", m_recent, L,G,R, L+R);

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
  m_lines.getInternals(L, G, R);
  int linesBytes = (L+G+R) * sizeof(char*);
  printf("  lines: L=%d G=%d R=%d, L+R=%d, bytes=%d\n", L,G,R, L+R, linesBytes);

  // recent
  m_recentLine.getInternals(L, G, R);
  int recentBytes = (L+G+R) * sizeof(char);
  printf("  recentLine: L=%d G=%d R=%d, bytes=%d\n", L,G,R, recentBytes);

  // line contents
  int textBytes = 0;
  int intFragBytes = 0;
  int overheadBytes = 0;

  for (int i=0; i<numLines(); i++) {
    char const *p = m_lines.get(i);

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
  if (len > m_longestLengthSoFar) {
    m_longestLengthSoFar = len;
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
    swap(this->m_lines, other.m_lines);
    swap(this->m_recent, other.m_recent);
    swap(this->m_recentLine, other.m_recentLine);
    swap(this->m_longestLengthSoFar, other.m_longestLengthSoFar);
  }

  FOREACH_RCSERFLIST_NC(TextDocumentObserver, m_observers, iter) {
    iter.data()->observeTotalChange(*this);
  }
}


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

    if (s_injectedErrorCountdown > 0) {
      if (len < s_injectedErrorCountdown) {
        s_injectedErrorCountdown -= len;
      }
      else {
        s_injectedErrorCountdown = 0;
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


inline bool isSpaceOrTab(char c)
{
  return c == ' ' || c == '\t';
}


int TextDocumentCore::countLeadingSpacesTabs(int line) const
{
  bc(line);

  if (line == m_recent) {
    int i=0;
    while (i < m_recentLine.length() &&
           isSpaceOrTab(m_recentLine.get(i))) {
      i++;
    }
    return i;
  }
  else {
    char const *begin = m_lines.get(line);
    if (!begin) {
      return 0;
    }
    char const *p = begin;
    while (*p != '\n' && isSpaceOrTab(*p)) {
      p++;
    }
    return p - begin;
  }
}


int TextDocumentCore::countTrailingSpacesTabs(int line) const
{
  bc(line);

  if (line == m_recent) {
    int i = m_recentLine.length();
    while (i > 0 &&
           isSpaceOrTab(m_recentLine.get(i-1))) {
      i--;
    }
    return m_recentLine.length() - i;
  }
  else {
    char const *begin = m_lines.get(line);
    if (!begin) {
      return 0;
    }
    char const *end = begin + bufStrlen(begin);
    char const *p = end;
    while (p > begin && isSpaceOrTab(p[-1])) {
      p--;
    }
    return end - p;
  }
}


void TextDocumentCore::addObserver(TextDocumentObserver *observer) const
{
  this->m_observers.appendNewItem(observer);
}

void TextDocumentCore::removeObserver(TextDocumentObserver *observer) const
{
  this->m_observers.removeItem(observer);
}

bool TextDocumentCore::hasObserver(TextDocumentObserver const *observer) const
{
  return this->m_observers.contains(observer);
}


void TextDocumentCore::notifyUnsavedChangesChange(TextDocument const *doc) const
{
  FOREACH_RCSERFLIST_NC(TextDocumentObserver, m_observers, iter) {
    iter.data()->observeUnsavedChangesChange(doc);
  }
}


// -------------------- TextDocumentObserver ------------------
int TextDocumentObserver::s_objectCount = 0;

CHECK_OBJECT_COUNT(TextDocumentObserver);


TextDocumentObserver::TextDocumentObserver()
{
  s_objectCount++;
}

TextDocumentObserver::TextDocumentObserver(TextDocumentObserver const &obj)
{
  s_objectCount++;
}

TextDocumentObserver::~TextDocumentObserver()
{
  s_objectCount--;
}

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


// EOF
