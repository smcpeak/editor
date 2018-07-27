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
  // There is always at least one line.
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


void TextDocumentCore::selfCheck() const
{
  xassert(m_recent >= -1);
  if (m_recent >= 0) {
    xassert(m_lines.get(m_recent) == NULL);
  }
  else {
    xassert(m_recentLine.length() == 0);
  }

  for (int i=0; i < m_lines.length(); i++) {
    char *p = m_lines.get(i);
    xassert(p == NULL || *p != '\n');
  }

  xassert(m_longestLengthSoFar >= 0);
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


void TextDocumentCore::attachRecent(TextMCoord tc, int insLength)
{
  if (m_recent == tc.m_line) { return; }
  detachRecent();

  char const *p = m_lines.get(tc.m_line);
  int len = bufStrlen(p);
  if (len) {
    // copy contents into 'recentLine'
    m_recentLine.fillFromArray(p, len, tc.m_byteIndex, insLength);

    // deallocate the source
    delete[] p;
    m_lines.set(tc.m_line, NULL);
  }
  else {
    xassert(m_recentLine.length() == 0);
  }

  m_recent = tc.m_line;
}


bool TextDocumentCore::isEmptyLine(int line) const
{
  bc(line);

  if (line == m_recent) {
    return m_recentLine.length() == 0;
  }
  else {
    return m_lines.get(line) == NULL;
  }
}


int TextDocumentCore::lineLengthBytes(int line) const
{
  bc(line);

  if (line == m_recent) {
    return m_recentLine.length();
  }
  else {
    char *p = m_lines.get(line);
    if (p) {
      // An empty line should be represented with a NULL pointer.
      // Otherwise, 'isEmptyLine' will get the wrong answer.
      xassert(*p != '\n');
      return bufStrlen(p);
    }
    else {
      return 0;
    }
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


void TextDocumentCore::getLine(TextMCoord tc, char *dest, int destLen) const
{
  bc(tc.m_line);
  xassert(destLen >= 0);

  if (tc.m_line == m_recent) {
    m_recentLine.writeIntoArray(dest, destLen, tc.m_byteIndex);
  }
  else {
    char const *p = m_lines.get(tc.m_line);
    int len = bufStrlen(p);
    xassert(0 <= tc.m_byteIndex && tc.m_byteIndex + destLen <= len);

    memcpy(dest, p + tc.m_byteIndex, destLen);
  }
}


bool TextDocumentCore::validCoord(TextMCoord tc) const
{
  // TODO UTF-8: Check that the byteIndex is not in the middle of a
  // multibyte sequence.

  return 0 <= tc.m_line &&
              tc.m_line < numLines() &&
         0 <= tc.m_byteIndex &&
              tc.m_byteIndex <= lineLengthBytes(tc.m_line); // EOL is ok
}


bool TextDocumentCore::validRange(TextMCoordRange const &range) const
{
  return this->validCoord(range.m_start) &&
         this->validCoord(range.m_end) &&
         range.isRectified();
}


TextMCoord TextDocumentCore::endCoord() const
{
  int line = this->numLines()-1;
  return TextMCoord(line, this->lineLengthBytes(line));
}


TextMCoord TextDocumentCore::lineBeginCoord(int line) const
{
  bc(line);
  return TextMCoord(line, 0);
}


TextMCoord TextDocumentCore::lineEndCoord(int line) const
{
  return TextMCoord(line, this->lineLengthBytes(line));
}


// Interestingly, this is *not* what "wc -l" returns.  Instead, wc -l
// returns a count of the newline characters.  But that seems like a bug
// in 'wc' to me.
int TextDocumentCore::numLinesExceptFinalEmpty() const
{
  int lastLine = this->numLines()-1;
  if (this->isEmptyLine(lastLine)) {
    return lastLine;
  }
  else {
    return lastLine+1;
  }
}


bool TextDocumentCore::walkCoordBytes(TextMCoord &tc, int len) const
{
  xassert(this->validCoord(tc));

  for (; len > 0; len--) {
    if (tc.m_byteIndex == this->lineLengthBytes(tc.m_line)) {
      // cycle to next line
      tc.m_line++;
      if (tc.m_line >= this->numLines()) {
        return false;      // beyond EOF
      }
      tc.m_byteIndex=0;
    }
    else {
      tc.m_byteIndex++;
    }
  }

  for (; len < 0; len++) {
    if (tc.m_byteIndex == 0) {
      // cycle up to end of preceding line
      tc.m_line--;
      if (tc.m_line < 0) {
        return false;      // before BOF
      }
      tc.m_byteIndex = this->lineLengthBytes(tc.m_line);
    }
    else {
      tc.m_byteIndex--;
    }
  }

  return true;
}


// 'line' is marked 'const' to ensure its value is not changed before
// being passed to the observers.  The same thing is done in the other
// three mutator functions.
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


void TextDocumentCore::insertText(TextMCoord const tc,
                                  char const * const text,
                                  int const length)
{
  bctc(tc);

  xassert(length >= 0);
  if (length == 0) {
    // This prevents needlessly allocating an array for an empty line.
    return;
  }

  #ifndef NDEBUG
    for (int i=0; i<length; i++) {
      xassert(text[i] != '\n');
    }
  #endif

  if (tc.m_byteIndex==0 && isEmptyLine(tc.m_line) && tc.m_line!=m_recent) {
    // setting a new line, can leave 'recent' alone
    char *p = new char[length+1];
    memcpy(p, text, length);
    p[length] = '\n';
    m_lines.set(tc.m_line, p);

    seenLineLength(length);
  }
  else {
    // use recent
    attachRecent(tc, length);
    m_recentLine.insertMany(tc.m_byteIndex, text, length);

    seenLineLength(m_recentLine.length());
  }

  FOREACH_RCSERFLIST_NC(TextDocumentObserver, m_observers, iter) {
    iter.data()->observeInsertText(*this, tc, text, length);
  }
}


void TextDocumentCore::deleteText(TextMCoord const tc, int const length)
{
  bctc(tc);

  if (tc.m_byteIndex==0 && length==lineLengthBytes(tc.m_line) && tc.m_line!=m_recent) {
    // removing entire line, no need to move 'recent'
    char *p = m_lines.get(tc.m_line);
    if (p) {
      delete[] p;
    }
    m_lines.set(tc.m_line, NULL);
  }
  else {
    // use recent
    attachRecent(tc, 0);
    m_recentLine.removeMany(tc.m_byteIndex, length);
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
    int len = lineLengthBytes(i);
    char *p = NULL;
    if (len) {
      p = new char[len];
      getLine(TextMCoord(i, 0), p, len);
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
    this->deleteText(TextMCoord(0, 0), this->lineLengthBytes(0));
    this->deleteLine(0);
  }

  // delete contents of last remaining line
  this->deleteText(TextMCoord(0, 0), this->lineLengthBytes(0));
}


void TextDocumentCore::swapWith(TextDocumentCore &other) NOEXCEPT
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
  TextMCoord tc(0,0);

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
      tc.m_byteIndex += nl-p;

      if (nl < end) {
        // skip newline
        nl++;
        tc.m_line++;
        this->insertLine(tc.m_line);
        tc.m_byteIndex=0;
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
    int len = this->lineLengthBytes(line);
    buffer.ensureIndexDoubler(len);       // text + possible newline

    this->getLine(TextMCoord(line, 0), buffer.getArrayNC(), len);
    if (line < this->numLines()-1) {        // last gets no newline
      buffer[len] = '\n';
      len++;
    }

    if ((int)fwrite(buffer.getArray(), 1, len, fp) != len) {
      xsyserror("write", fname);
    }
  }
}


bool TextDocumentCore::getTextSpan(TextMCoord tc, char *text, int textLenBytes) const
{
  xassert(this->validCoord(tc));
  xassert(textLenBytes >= 0);

  TextMCoord end(tc);
  if (!this->walkCoordBytes(end, textLenBytes)) {
    return false;
  }

  TextMCoordRange range(tc, end);
  string str(this->getTextRange(range));

  xassert(str.length() == textLenBytes);
  memcpy(text, str.c_str(), textLenBytes);
  return true;
}


int TextDocumentCore::bytesInRange(TextMCoordRange const &range) const
{
  // Very inefficient!
  string s(this->getTextRange(range));
  return s.length();
}


string TextDocumentCore::getTextRange(TextMCoordRange const &range) const
{
  xassert(this->validRange(range));

  // This function uses the 'withinOneLine' case as a base case of a two
  // level recursion; it's not terribly efficient.

  if (range.withinOneLine()) {
    // extracting text from a single line
    int lenBytes = range.m_end.m_byteIndex - range.m_start.m_byteIndex;

    // It is not very efficient to allocate two buffers, one here and
    // one inside the string object, but the std::string API doesn't
    // offer a way to do it directly, so I need to refactor my APIs if
    // I want to avoid the extra allocation.
    Array<char> buf(lenBytes+1);

    buf[lenBytes] = 0;              // NUL terminator
    this->getLine(range.m_start, buf, lenBytes);
    string ret(buf.ptrC());    // Explicitly calling 'ptrC' is only needed for Eclipse...

    return ret;
  }

  // build up returned string
  stringBuilder sb;

  // Right half of range start line.
  sb = this->getTextRange(TextMCoordRange(
         range.m_start, this->lineEndCoord(range.m_start.m_line)));

  // Full lines between start and end.
  for (int i = range.m_start.m_line + 1; i < range.m_end.m_line; i++) {
    sb << "\n";
    sb << this->getWholeLine(i);
  }

  // Left half of end line.
  sb << "\n";
  sb << this->getTextRange(TextMCoordRange(
          this->lineBeginCoord(range.m_end.m_line), range.m_end));

  return sb;
}


string TextDocumentCore::getWholeLine(int line) const
{
  bc(line);
  return getTextRange(TextMCoordRange(
    this->lineBeginCoord(line), this->lineEndCoord(line)
  ));
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

void TextDocumentObserver::observeInsertLine(TextDocumentCore const &, int) NOEXCEPT
{}

void TextDocumentObserver::observeDeleteLine(TextDocumentCore const &, int) NOEXCEPT
{}

void TextDocumentObserver::observeInsertText(TextDocumentCore const &, TextMCoord, char const *, int) NOEXCEPT
{}

void TextDocumentObserver::observeDeleteText(TextDocumentCore const &, TextMCoord, int) NOEXCEPT
{}

void TextDocumentObserver::observeTotalChange(TextDocumentCore const &doc) NOEXCEPT
{}

void TextDocumentObserver::observeUnsavedChangesChange(TextDocument const *doc) NOEXCEPT
{}


// EOF
