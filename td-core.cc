// td-core.cc
// code for td-core.h

#include "td-core.h"                   // this module

#include "gap-gdvalue.h"               // toGDValue(GapArray)
#include "history.h"                   // HE_text

// smbase
#include "smbase/array.h"              // Array
#include "smbase/gdvalue.h"            // gdv::GDValue
#include "smbase/objcount.h"           // CheckObjectCount
#include "smbase/overflow.h"           // preIncrementWithOverflowCheck, safeToInt
#include "smbase/sm-test.h"            // USUAL_MAIN, PVAL
#include "smbase/string-util.h"        // vectorOfUCharToString, stringToVectorOfUChar
#include "smbase/strutil.h"            // encodeWithEscapes
#include "smbase/syserr.h"             // xsyserror

// libc
#include <assert.h>                    // assert
#include <ctype.h>                     // isalnum, isspace
#include <string.h>                    // strncasecmp

using namespace gdv;


// ---------------------- TextDocumentCore --------------------------
TextDocumentCore::TextDocumentCore()
  : m_lines(),               // empty sequence of lines
    m_recentIndex(-1),
    m_longestLengthSoFar(0),
    m_recentLine(),
    m_versionNumber(0),
    m_observers(),
    m_iteratorCount(0)
{
  // There is always at least one line.
  m_lines.insert(0 /*line*/, TextDocumentLine() /*value*/);
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

  assert(m_iteratorCount == 0);

  // deallocate the non-NULL lines
  for (int i=0; i < m_lines.length(); i++) {
    TextDocumentLine const &tdl = m_lines.get(i);
    if (tdl.m_bytes) {
      delete[] tdl.m_bytes;
    }
  }
}


void TextDocumentCore::selfCheck() const
{
  xassert(m_recentIndex >= -1);
  if (m_recentIndex >= 0) {
    xassert(m_lines.get(m_recentIndex).isEmpty());
  }
  else {
    xassert(m_recentLine.length() == 0);
  }

  for (int i=0; i < m_lines.length(); i++) {
    TextDocumentLine const &tdl = m_lines.get(i);
    xassert(tdl.isEmpty() || tdl.at(0) != '\n');
  }

  xassert(m_longestLengthSoFar >= 0);
}


TextDocumentCore::operator gdv::GDValue() const
{
  GDValue m(GDVK_TAGGED_ORDERED_MAP, "TextDocumentCore"_sym);

  m.mapSetValueAtSym("version", getVersionNumber());
  m.mapSetValueAtSym("lines", getAllLines());

  return m;
}


gdv::GDValue TextDocumentCore::getAllLines() const
{
  GDValue seq(GDVK_SEQUENCE);

  for (int i=0; i < numLines(); i++) {
    seq.sequenceAppend(GDValue(getWholeLineString(i)));
  }

  return seq;
}


gdv::GDValue TextDocumentCore::dumpInternals() const
{
  GDValue m(GDVK_TAGGED_ORDERED_MAP, "TextDocumentCoreInternals"_sym);

  GDV_WRITE_MEMBER_SYM(m_lines);
  GDV_WRITE_MEMBER_SYM(m_recentIndex);
  GDV_WRITE_MEMBER_SYM(m_longestLengthSoFar);
  GDV_WRITE_MEMBER_SYM(m_recentLine);
  GDV_WRITE_MEMBER_SYM(m_versionNumber);

  // Serf pointers generally don't get serialized, but a count could be
  // informative.
  m.mapSetValueAtSym("numObservers", m_observers.count());

  GDV_WRITE_MEMBER_SYM(m_iteratorCount);

  return m;
}


void TextDocumentCore::detachRecent()
{
  if (m_recentIndex == -1) { return; }

  // The slot in 'm_lines' should currently be empty because the content
  // is in 'm_recentLine'.
  xassert(m_lines.get(m_recentIndex).isEmpty());

  // copy 'recentLine' into lines[recent]
  int len = m_recentLine.length();
  if (len) {
    char *p = new char[len+1];
    p[len] = '\n';
    m_recentLine.writeIntoArray(p, len);
    xassert(p[len] == '\n');   // may as well check for overrun..

    m_lines.set(m_recentIndex, TextDocumentLine(len+1, p));

    m_recentLine.clear();
  }
  else {
    // lines[recent] is already empty, nothing needs to be done.
  }

  m_recentIndex = -1;
}


void TextDocumentCore::attachRecent(TextMCoord tc, int insLength)
{
  if (m_recentIndex == tc.m_line) { return; }
  detachRecent();

  TextDocumentLine const &tdl = m_lines.get(tc.m_line);
  int len = tdl.lengthWithoutNL();
  if (len) {
    // copy contents into 'recentLine'
    m_recentLine.fillFromArray(tdl.m_bytes, len, tc.m_byteIndex, insLength);

    // deallocate the source
    delete[] tdl.m_bytes;
    m_lines.set(tc.m_line, TextDocumentLine());
  }
  else {
    xassert(m_recentLine.length() == 0);
  }

  m_recentIndex = tc.m_line;
}


bool TextDocumentCore::isEmptyLine(int line) const
{
  bc(line);

  if (line == m_recentIndex) {
    return m_recentLine.length() == 0;
  }
  else {
    return m_lines.get(line).isEmpty();
  }
}


int TextDocumentCore::lineLengthBytes(int line) const
{
  bc(line);

  if (line == m_recentIndex) {
    return m_recentLine.length();
  }
  else {
    TextDocumentLine const &tdl = m_lines.get(line);
    if (!tdl.isEmpty()) {
      // An empty line should be represented with a NULL pointer.
      // Otherwise, 'isEmptyLine' will get the wrong answer.
      xassert(tdl.m_bytes[0] != '\n');
      return tdl.lengthWithoutNL();
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


void TextDocumentCore::getPartialLine(TextMCoord tc,
  ArrayStack<char> /*INOUT*/ &dest, int numBytes) const
{
  bc(tc.m_line);
  xassert(numBytes >= 0);

  char *destPtr = dest.ptrToPushedMultipleAlt(numBytes);

  if (tc.m_line == m_recentIndex) {
    m_recentLine.writeIntoArray(destPtr, numBytes, tc.m_byteIndex);
  }
  else {
    TextDocumentLine const &tdl = m_lines.get(tc.m_line);
    int len = tdl.lengthWithoutNL();
    xassert(0 <= tc.m_byteIndex && tc.m_byteIndex + numBytes <= len);

    memcpy(destPtr, tdl.m_bytes + tc.m_byteIndex, numBytes);
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


bool TextDocumentCore::walkCoordBytes(TextMCoord &tc /*INOUT*/, int len) const
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
  bumpVersionNumber();

  // insert a blank line
  m_lines.insert(line, TextDocumentLine() /*value*/);

  // adjust which line is 'recent'
  if (m_recentIndex >= line) {
    m_recentIndex++;
  }

  FOREACH_RCSERFLIST_NC(TextDocumentObserver, m_observers, iter) {
    iter.data()->observeInsertLine(*this, line);
  }
}


void TextDocumentCore::deleteLine(int const line)
{
  bumpVersionNumber();

  if (line == m_recentIndex) {
    xassert(m_recentLine.length() == 0);
    detachRecent();
  }

  // make sure line is empty
  xassert(m_lines.get(line).isEmpty());

  // make sure we're not deleting the last line
  xassert(numLines() > 1);

  // remove the line
  m_lines.remove(line);

  // adjust which line is 'recent'
  if (m_recentIndex > line) {
    m_recentIndex--;
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
  bumpVersionNumber();

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

  if (tc.m_byteIndex == 0 &&
      isEmptyLine(tc.m_line) &&
      tc.m_line != m_recentIndex) {
    // setting a new line, can leave 'recent' alone
    char *p = new char[length+1];
    memcpy(p, text, length);
    p[length] = '\n';
    m_lines.set(tc.m_line, TextDocumentLine(length+1, p));

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


void TextDocumentCore::deleteTextBytes(TextMCoord const tc, int const length)
{
  bctc(tc);
  bumpVersionNumber();

  if (tc.m_byteIndex == 0 &&
      length == lineLengthBytes(tc.m_line) &&
      tc.m_line != m_recentIndex) {
    // removing entire line, no need to move 'recent'
    TextDocumentLine const &tdl = m_lines.get(tc.m_line);
    if (!tdl.isEmpty()) {
      delete[] tdl.m_bytes;
    }
    m_lines.set(tc.m_line, TextDocumentLine());
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


void TextDocumentCore::replaceMultilineRange(
  TextMCoordRange const &range, std::string const &text)
{
  xassertPrecondition(validRange(range));

  if (int deletionByteCount = countBytesInRange(range)) {
    // This is what `TextDocument::deleteAt` does, in essence.
    HE_text elt(range.m_start,
                false /*insertion*/,
                NULL /*text*/, 0 /*textLen*/);

    // This step is a bit wasteful because we make a copy of the text we
    // are about to delete (since `HE_text` is part of the undo/redo
    // mechanism), when all we really need is the count.  (But we cannot
    // skip this call because the `apply` call gets its byte count from
    // the copy made here.)
    elt.computeText(*this, deletionByteCount);

    elt.apply(*this, false /*reverse*/);
  }

  if (int insertionByteCount = safeToInt(text.size())) {
    // This is what `TextDocument::insertAt` does.
    HE_text elt(range.m_start,
                true /*insertion*/,
                text.data(), insertionByteCount);
    elt.apply(*this, false /*reverse*/);
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
  printf("  recent=%d: L=%d G=%d R=%d, L+R=%d\n", m_recentIndex, L,G,R, L+R);

  // line contents
  for (int i=0; i<numLines(); i++) {
    ArrayStack<char> text;
    this->getWholeLine(i, text);

    printf("  line %d: \"%s\"\n", i,
           toCStr(encodeWithEscapes(text.getArray(), text.length())));
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
    TextDocumentLine const &tdl = m_lines.get(i);

    textBytes += tdl.lengthWithoutNL();

    int alloc = 0;
    if (!tdl.isEmpty()) {
      alloc = tdl.lengthWithoutNL()+1;    // for '\n'
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
    this->deleteTextBytes(TextMCoord(0, 0), this->lineLengthBytes(0));
    this->deleteLine(0);
  }

  // delete contents of last remaining line
  this->deleteTextBytes(TextMCoord(0, 0), this->lineLengthBytes(0));
}


void TextDocumentCore::notifyTotalChange()
{
  FOREACH_RCSERFLIST_NC(TextDocumentObserver, m_observers, iter) {
    iter.data()->observeTotalChange(*this);
  }
}


void TextDocumentCore::bumpVersionNumber()
{
  // Since we are about to make a change, ensure there are no
  // outstanding iterators.
  xassert(m_iteratorCount == 0);

  preIncrementWithOverflowCheck(m_versionNumber);
}


void TextDocumentCore::replaceWholeFile(
  std::vector<unsigned char> const &bytes)
{
  this->clear();

  // Location of the end of the document as it is being built.
  TextMCoord tc(0,0);

  char const *p = (char const *)bytes.data();
  char const *end = p + bytes.size();
  while (p < end) {
    // Find next newline, or end of the input.
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

  this->notifyTotalChange();
}


std::vector<unsigned char> TextDocumentCore::getWholeFile() const
{
  // Destination sequence.
  std::vector<unsigned char> fileBytes;

  // Buffer into which we will copy each line before copying it to
  // 'fileBytes'.  (This is necessitated by my use of ArrayStack instead
  // of vector in the document interface.)
  ArrayStack<char> buffer;

  for (int line=0; line < this->numLines(); line++) {
    buffer.clear();

    this->getWholeLine(line, buffer);
    if (line < this->numLines()-1) {        // last gets no newline
      buffer.push('\n');
    }

    unsigned char const *d = (unsigned char const *)buffer.getArray();
    fileBytes.insert(fileBytes.end(), d, d + buffer.length());
  }

  return fileBytes;
}


std::string TextDocumentCore::getWholeFileString() const
{
  return vectorOfUCharToString(getWholeFile());
}


void TextDocumentCore::replaceWholeFileString(std::string const &str)
{
  replaceWholeFile(stringToVectorOfUChar(str));
}


bool TextDocumentCore::getTextSpanningLines(
  TextMCoord tc, ArrayStack<char> /*INOUT*/ &dest, int numBytes) const
{
  xassert(this->validCoord(tc));
  xassert(numBytes >= 0);

  TextMCoord end(tc);
  if (!this->walkCoordBytes(end, numBytes)) {
    return false;
  }

  int origDestLength = dest.length();
  TextMCoordRange range(tc, end);
  this->getTextForRange(range, dest);
  xassert(dest.length() - origDestLength == numBytes);

  return true;
}


int TextDocumentCore::countBytesInRange(TextMCoordRange const &range) const
{
  // Inefficient!
  ArrayStack<char> arr;
  this->getTextForRange(range, arr);
  return arr.length();
}


bool TextDocumentCore::adjustMCoord(TextMCoord /*INOUT*/ &tc) const
{
  if (tc.m_line < 0) {
    tc = beginCoord();
    return true;
  }

  if (tc.m_line >= numLines()) {
    tc = endCoord();
    return true;
  }

  if (tc.m_byteIndex < 0) {
    tc.m_byteIndex = 0;
    return true;
  }

  auto len = lineLengthBytes(tc.m_line);
  if (tc.m_byteIndex > len) {
    tc.m_byteIndex = len;
    return true;
  }

  // TODO UTF-8: Check for being inside a multibyte sequence.

  xassert(validCoord(tc));

  return false;
}


bool TextDocumentCore::adjustMCoordRange(
  TextMCoordRange /*INOUT*/ &range) const
{
  bool adjusted = adjustMCoord(range.m_start);
  adjusted |= adjustMCoord(range.m_end);

  if (range.m_end < range.m_start) {
    range.m_end = range.m_start;
    adjusted = true;
  }

  return adjusted;
}


void TextDocumentCore::getTextForRange(TextMCoordRange const &range,
  ArrayStack<char> /*INOUT*/ &dest) const
{
  xassert(this->validRange(range));

  // This function uses the 'withinOneLine' case as a base case of a two
  // level recursion.

  if (range.withinOneLine()) {
    // extracting text from a single line
    int lenBytes = range.m_end.m_byteIndex - range.m_start.m_byteIndex;

    this->getPartialLine(range.m_start, dest, lenBytes);
    return;
  }

  // Right half of range start line.
  this->getTextForRange(
    TextMCoordRange(range.m_start, this->lineEndCoord(range.m_start.m_line)),
    dest);

  // Full lines between start and end.
  for (int i = range.m_start.m_line + 1; i < range.m_end.m_line; i++) {
    dest.push('\n');
    this->getWholeLine(i, dest);
  }

  // Left half of end line.
  dest.push('\n');
  this->getTextForRange(
    TextMCoordRange(this->lineBeginCoord(range.m_end.m_line), range.m_end),
    dest);
}


void TextDocumentCore::getWholeLine(int line,
  ArrayStack<char> /*INOUT*/ &dest) const
{
  bc(line);
  this->getTextForRange(
    TextMCoordRange(this->lineBeginCoord(line), this->lineEndCoord(line)),
    dest);
}


string TextDocumentCore::getWholeLineString(int line) const
{
  ArrayStack<char> text;
  this->getWholeLine(line, text);
  return toString(text);
}


int TextDocumentCore::countLeadingSpacesTabs(int line) const
{
  int ret = 0;
  for (LineIterator it(*this, line); it.has(); it.advByte()) {
    if (isSpaceOrTab(it.byteAt())) {
      ret++;
    }
    else {
      break;
    }
  }
  return ret;
}


int TextDocumentCore::countTrailingSpacesTabs(int line) const
{
  int ret = 0;
  for (LineIterator it(*this, line); it.has(); it.advByte()) {
    if (isSpaceOrTab(it.byteAt())) {
      ret++;
    }
    else {
      ret = 0;
    }
  }
  return ret;
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


void TextDocumentCore::notifyMetadataChange() const
{
  FOREACH_RCSERFLIST_NC(TextDocumentObserver, m_observers, iter) {
    iter.data()->observeMetadataChange(*this);
  }
}



// --------------- TextDocumentCore::LineIterator ----------------
TextDocumentCore::LineIterator::LineIterator(
  TextDocumentCore const &tdc, int line)
:
  m_tdc(&tdc),
  m_isRecentLine(false),
  m_nonRecentLine(NULL),
  m_byteOffset(0)
{
  if (line == m_tdc->m_recentIndex) {
    m_isRecentLine = true;
  }
  else if (0 <= line && line < m_tdc->numLines()) {
    m_nonRecentLine = m_tdc->m_lines.get(line).m_bytes; // Might be NULL.
  }
  else {
    // Treat an invalid line like an empty line.
    m_nonRecentLine = NULL;
  }
  m_tdc->m_iteratorCount++;
}


TextDocumentCore::LineIterator::~LineIterator()
{
  m_tdc->m_iteratorCount--;
}


bool TextDocumentCore::LineIterator::has() const
{
  if (m_nonRecentLine) {
    return m_nonRecentLine[m_byteOffset] != '\n';
  }
  else if (m_isRecentLine) {
    return m_byteOffset < m_tdc->m_recentLine.length();
  }
  else {
    return false;
  }
}


int TextDocumentCore::LineIterator::byteAt() const
{
  int ret = 0;
  if (m_nonRecentLine) {
    ret = (unsigned char)(m_nonRecentLine[m_byteOffset]);
  }
  else if (m_isRecentLine) {
    ret = (unsigned char)(m_tdc->m_recentLine.get(m_byteOffset));
  }
  else {
    xfailure("byteAt: line is empty");
  }
  xassert(ret != '\n');
  return ret;
}


void TextDocumentCore::LineIterator::advByte()
{
  xassert(this->has());
  m_byteOffset++;
}


// -------------------- TextDocumentObserver ---------------------
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

  verifyZeroRefCount();
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

void TextDocumentObserver::observeMetadataChange(TextDocumentCore const &) NOEXCEPT
{}


// EOF
