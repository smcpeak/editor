// td-core.cc
// code for td-core.h

#include "td-core.h"                   // this module

#include "byte-count.h"                // sizeBC, memchrBC, memcpyBC
#include "byte-difference.h"           // ByteDifference
#include "gap-gdvalue.h"               // toGDValue(LineGapArray)
#include "history.h"                   // HE_text
#include "line-number.h"               // LineNumber
#include "wrapped-integer.h"           // WrappedInteger::getAs

// smbase
#include "smbase/array.h"              // Array
#include "smbase/codepoint.h"          // isSpaceOrTab
#include "smbase/gdvalue-optional.h"   // gdv::GDValue(std::optional)
#include "smbase/gdvalue.h"            // gdv::GDValue
#include "smbase/objcount.h"           // CheckObjectCount
#include "smbase/overflow.h"           // preIncrementWithOverflowCheck, safeToInt
#include "smbase/sm-test.h"            // USUAL_MAIN, PVAL
#include "smbase/sm-trace.h"           // INIT_TRACE, etc.
#include "smbase/string-util.h"        // vectorOfUCharToString, stringToVectorOfUChar
#include "smbase/stringb.h"            // stringb
#include "smbase/strutil.h"            // encodeWithEscapes
#include "smbase/syserr.h"             // xsyserror
#include "smbase/xassert.h"            // xassert

// libc
#include <assert.h>                    // assert
#include <ctype.h>                     // isalnum, isspace
#include <string.h>                    // strncasecmp

using namespace gdv;
using namespace smbase;


INIT_TRACE("td-core");


// ---------------------- TextDocumentCore --------------------------
TextDocumentCore::TextDocumentCore()
  : m_lines(),               // Momentarily empty sequence of lines.
    m_recentIndex(),
    m_longestLengthSoFar(0),
    m_recentLine(),
    m_versionNumber(1),
    m_observers(),
    m_iteratorCount(0)
{
  // There is always at least one line.
  m_lines.insert(LineIndex(0) /*line*/, TextDocumentLine() /*value*/);

  selfCheck();
}


TextDocumentCore::~TextDocumentCore()
{
  if (!m_observers.isEmpty()) {
    TextDocumentObserver const *obs = m_observers.nthRef(0);
    TRACE1("TDC at " << (void*)this <<
            " still has " << m_observers.count() <<
            " observers, including " << (void*)obs);
  }

  // We require that the client code arrange to empty the observer list
  // before this object is destroyed.  If it is not empty here, the
  // observer will be left with a dangling pointer, which will cause
  // problems later of course.
  //
  // This uses ordinary 'assert' rather than 'xassert' to avoid
  // throwing an exception from within a destructor.
  assert(this->m_observers.isEmpty());

  assert(m_iteratorCount == 0);

  // Deallocate the non-null lines.
  FOR_EACH_LINE_INDEX_IN(i, *this) {
    TextDocumentLine &tdl = m_lines.eltRef(i);
    if (tdl.m_bytes) {
      delete[] tdl.m_bytes;

      // Just for safety; should not be necessary.
      tdl.m_bytes = nullptr;
    }
  }
}


void TextDocumentCore::selfCheck() const
{
  if (m_recentIndex.has_value()) {
    xassert(m_lines.get(*m_recentIndex).isEmpty());
  }
  else {
    xassert(m_recentLine.length() == 0);
  }

  FOR_EACH_LINE_INDEX_IN(i, *this) {
    TextDocumentLine const &tdl = m_lines.get(i);
    tdl.selfCheck();
  }

  xassert(m_longestLengthSoFar >= 0);
}


// True if `ga` and `tdl` represent the same sequence of bytes.
static bool equal_BGA_TDL(
  ByteGapArray<char> const &ga,
  TextDocumentLine const &tdl)
{
  ByteCount len = ga.length();
  if (len != tdl.length()) {
    return false;
  }

  for (ByteIndex i(0); i < len; ++i) {
    if (ga.get(i) != tdl.at(i)) {
      return false;
    }
  }

  return true;
}


bool TextDocumentCore::equalLineAt(
  LineIndex i, TextDocumentCore const &obj) const
{
  bc(i);
  obj.bc(i);

  if (i == m_recentIndex) {
    if (i == obj.m_recentIndex) {
      return m_recentLine == obj.m_recentLine;
    }
    else {
      return equal_BGA_TDL(m_recentLine, obj.getMLine(i));
    }
  }
  else {
    if (i == obj.m_recentIndex) {
      return equal_BGA_TDL(obj.m_recentLine, getMLine(i));
    }
    else {
      return getMLine(i) == obj.getMLine(i);
    }
  }
}


bool TextDocumentCore::operator==(TextDocumentCore const &obj) const
{
  if (numLines() != obj.numLines()) {
    return false;
  }

  FOR_EACH_LINE_INDEX_IN(i, *this) {
    if (!equalLineAt(i, obj)) {
      return false;
    }
  }

  return true;
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

  FOR_EACH_LINE_INDEX_IN(i, *this) {
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
  if (!m_recentIndex.has_value()) { return; }

  // The slot in 'm_lines' should currently be empty because the content
  // is in 'm_recentLine'.
  xassert(getMLine(*m_recentIndex).isEmpty());

  // copy 'recentLine' into lines[recent]
  ByteCount len = m_recentLine.length();
  if (len) {
    char *p = new char[len.get()];
    m_recentLine.writeIntoArray(p, len);

    setMLine(*m_recentIndex, TextDocumentLine(p, len));

    m_recentLine.clear();
  }
  else {
    // lines[recent] is already empty, nothing needs to be done.
  }

  m_recentIndex.reset();
}


void TextDocumentCore::attachRecent(TextMCoord tc, ByteCount insLength)
{
  if (m_recentIndex == tc.m_line) { return; }
  detachRecent();

  TextDocumentLine const &tdl = getMLine(tc.m_line);
  ByteCount len = tdl.length();
  if (len) {
    // copy contents into 'recentLine'
    m_recentLine.fillFromArray(
      tdl.m_bytes, len, tc.m_byteIndex, insLength);

    // deallocate the source
    delete[] tdl.m_bytes;
    setMLine(tc.m_line, TextDocumentLine());
  }
  else {
    xassert(m_recentLine.length() == 0);
  }

  m_recentIndex = tc.m_line;
}


LineIndex TextDocumentCore::lastLineIndex() const
{
  return LineIndex(numLines().pred());
}


bool TextDocumentCore::isEmptyLine(LineIndex line) const
{
  bc(line);

  if (line == m_recentIndex) {
    return m_recentLine.length() == 0;
  }
  else {
    return getMLine(line).isEmpty();
  }
}


ByteCount TextDocumentCore::lineLengthBytes(LineIndex line) const
{
  bc(line);

  if (line == m_recentIndex) {
    return m_recentLine.length();
  }
  else {
    return getMLine(line).length();
  }
}


ByteIndex TextDocumentCore::lineLengthByteIndex(LineIndex line) const
{
  return ByteIndex(lineLengthBytes(line));
}


void TextDocumentCore::getPartialLine(TextMCoord tc,
  ArrayStack<char> /*INOUT*/ &dest, ByteCount numBytes) const
{
  bc(tc.m_line);

  char *destPtr = dest.ptrToPushedMultipleAlt(numBytes.get());

  if (tc.m_line == m_recentIndex) {
    m_recentLine.writeIntoArray(destPtr, numBytes, tc.m_byteIndex);
  }
  else {
    TextDocumentLine const &tdl = getMLine(tc.m_line);
    ByteCount len = tdl.length();
    xassert(tc.m_byteIndex + numBytes <= len);

    memcpyBC(destPtr, tdl.m_bytes + tc.m_byteIndex, numBytes);
  }
}


bool TextDocumentCore::validCoord(TextMCoord tc) const
{
  // TODO UTF-8: Check that the byteIndex is not in the middle of a
  // multibyte sequence.

  return validLine(tc.m_line) &&
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
  LineIndex line = lastLineIndex();
  return TextMCoord(line, this->lineLengthByteIndex(line));
}


TextMCoord TextDocumentCore::lineBeginCoord(LineIndex line) const
{
  bc(line);
  return TextMCoord(line, ByteIndex(0));
}


TextMCoord TextDocumentCore::lineEndCoord(LineIndex line) const
{
  return TextMCoord(line, this->lineLengthByteIndex(line));
}


// Interestingly, this is *not* what "wc -l" returns.  Instead, wc -l
// returns a count of the newline characters.  But that seems like a bug
// in 'wc' to me.
LineCount TextDocumentCore::numLinesExcludingFinalEmpty() const
{
  LineIndex lastIndex = lastLineIndex();
  if (this->isEmptyLine(lastIndex)) {
    return lastIndex;
  }
  else {
    return lastIndex.succ();
  }
}


bool TextDocumentCore::walkCoordBytes(
  TextMCoord &tc /*INOUT*/, ByteDifference len) const
{
  xassert(this->validCoord(tc));

  for (; len > 0; len--) {
    if (tc.m_byteIndex == this->lineLengthByteIndex(tc.m_line)) {
      // cycle to next line
      ++tc.m_line;
      if (!validLine(tc.m_line)) {
        return false;      // beyond EOF
      }
      tc.m_byteIndex.set(0);
    }
    else {
      ++tc.m_byteIndex;
    }
  }

  for (; len < 0; len++) {
    if (tc.m_byteIndex == 0) {
      // cycle up to end of preceding line
      if (tc.m_line.isZero()) {
        return false;      // before BOF
      }
      --tc.m_line;
      tc.m_byteIndex = this->lineLengthByteIndex(tc.m_line);
    }
    else {
      tc.m_byteIndex--;
    }
  }

  return true;
}


void TextDocumentCore::walkCoordBytesValid(
  TextMCoord &tc /*INOUT*/, ByteDifference distance) const
{
  bool valid = walkCoordBytes(tc /*INOUT*/, distance);
  xassert(valid);
}


// 'line' is marked 'const' to ensure its value is not changed before
// being passed to the observers.  The same thing is done in the other
// three mutator functions.
void TextDocumentCore::insertLine(LineIndex const line)
{
  bumpVersionNumber();

  // insert a blank line
  m_lines.insert(line, TextDocumentLine() /*value*/);

  // adjust which line is 'recent'
  if (m_recentIndex.has_value() && *m_recentIndex >= line) {
    ++(*m_recentIndex);
  }

  FOREACH_RCSERFLIST_NC(TextDocumentObserver, m_observers, iter) {
    iter.data()->observeInsertLine(*this, line);
  }
}


void TextDocumentCore::deleteLine(LineIndex const line)
{
  bumpVersionNumber();

  if (line == m_recentIndex) {
    xassert(m_recentLine.length() == 0);
    detachRecent();
  }

  // make sure line is empty
  xassert(getMLine(line).isEmpty());

  // make sure we're not deleting the last line
  xassert(numLines() > 1);

  // remove the line
  m_lines.remove(line);

  // adjust which line is 'recent'
  if (m_recentIndex.has_value() && *m_recentIndex > line) {
    --(*m_recentIndex);
  }

  FOREACH_RCSERFLIST_NC(TextDocumentObserver, m_observers, iter) {
    iter.data()->observeDeleteLine(*this, line);
  }
}


void TextDocumentCore::insertText(TextMCoord const tc,
                                  char const * const text,
                                  ByteCount const length)
{
  xassertPrecondition(memchrBC(text, '\n', length) == nullptr);

  bctc(tc);

  if (length == 0) {
    // Bail early if there is nothing to change.
    return;
  }

  bumpVersionNumber();

  if (tc.m_byteIndex == 0 &&
      isEmptyLine(tc.m_line) &&
      tc.m_line != m_recentIndex) {
    // Inserting an entirely new line, can leave recent alone.
    char *p = new char[length.get()];
    memcpyBC(p, text, length);
    setMLine(tc.m_line, TextDocumentLine(p, length));

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


void TextDocumentCore::insertString(
  TextMCoord tc, std::string const &str)
{
  insertText(tc, str.data(), sizeBC(str));
}


void TextDocumentCore::deleteTextBytes(
  TextMCoord const tc, ByteCount const length)
{
  bctc(tc);

  if (length == 0) {
    // Like with insertion, bail early if `length==0`.
    return;
  }

  bumpVersionNumber();

  if (tc.m_byteIndex == 0 &&
      length == lineLengthBytes(tc.m_line) &&
      tc.m_line != m_recentIndex) {
    // removing entire line, no need to move 'recent'
    TextDocumentLine const &tdl = getMLine(tc.m_line);
    if (!tdl.isEmpty()) {
      delete[] tdl.m_bytes;
    }
    setMLine(tc.m_line, TextDocumentLine());
  }
  else {
    // use recent
    attachRecent(tc, ByteCount(0));
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

  if (ByteCount deletionByteCount = countBytesInRange(range)) {
    // This is what `TextDocument::deleteAt` does, in essence.
    HE_text elt(range.m_start,
                false /*insertion*/,
                NULL /*text*/, ByteCount(0) /*textLen*/);

    // This step is a bit wasteful because we make a copy of the text we
    // are about to delete (since `HE_text` is part of the undo/redo
    // mechanism), when all we really need is the count.  (But we cannot
    // skip this call because the `apply` call gets its byte count from
    // the copy made here.)
    elt.computeText(*this, deletionByteCount);

    elt.apply(*this, false /*reverse*/);
  }

  if (ByteCount insertionByteCount = sizeBC(text)) {
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
  printf("  lines: L=%d G=%d R=%d, num=%d\n", L,G,R, numLines().get());

  // recent
  m_recentLine.getInternals(L, G, R);
  printf("  recent=%d: L=%d G=%d R=%d, L+R=%d\n",
    (m_recentIndex? m_recentIndex->get() : -1),
    L,G,R, L+R);

  // line contents
  FOR_EACH_LINE_INDEX_IN(i, *this) {
    ArrayStack<char> text;
    this->getWholeLine(i, text);

    printf("  line %d: \"%s\"\n", i.get(),
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

  FOR_EACH_LINE_INDEX_IN(i, *this) {
    TextDocumentLine const &tdl = getMLine(i);

    textBytes += tdl.length().get();

    int alloc = 0;
    if (!tdl.isEmpty()) {
      alloc = tdl.length().get() + 1;        // +1 for '\n'
      overheadBytes += sizeof(size_t);  // malloc's internal 'size' field
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


void TextDocumentCore::seenLineLength(ByteCount len)
{
  if (len > m_longestLengthSoFar) {
    m_longestLengthSoFar = len;
  }
}


void TextDocumentCore::clear()
{
  LineIndex const zeroLI(0);
  ByteIndex const zeroBI(0);

  while (this->numLines() > 1) {
    this->deleteTextBytes(
      TextMCoord(zeroLI, zeroBI), this->lineLengthBytes(zeroLI));
    this->deleteLine(zeroLI);
  }

  // delete contents of last remaining line
  this->deleteTextBytes(
    TextMCoord(zeroLI, zeroBI), this->lineLengthBytes(zeroLI));
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
  TextMCoord tc = beginCoord();

  char const *p = (char const *)bytes.data();
  char const *end = p + bytes.size();
  while (p < end) {
    // Find next newline, or end of the input.
    char const *nl = p;
    while (nl < end && *nl!='\n') {
      nl++;
    }

    // insert this line fragment
    ByteCount numBytes(nl - p);
    this->insertText(tc, p, numBytes);
    tc.m_byteIndex += numBytes;

    if (nl < end) {
      // skip newline
      ++nl;
      ++tc.m_line;
      this->insertLine(tc.m_line);
      tc.m_byteIndex.set(0);
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
  // of vector in the `getWholeLine` interface.)
  ArrayStack<char> buffer;

  FOR_EACH_LINE_INDEX_IN(line, *this) {
    buffer.clear();

    this->getWholeLine(line, buffer);
    if (line < lastLineIndex()) {        // last gets no newline
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
  TextMCoord tc, ArrayStack<char> /*INOUT*/ &dest, ByteCount numBytes) const
{
  xassert(this->validCoord(tc));
  xassert(numBytes >= 0);

  TextMCoord end(tc);
  if (!this->walkCoordBytes(end /*INOUT*/, numBytes)) {
    return false;
  }

  int origDestLength = dest.length();
  TextMCoordRange range(tc, end);
  this->getTextForRange(range, dest);
  xassert(dest.length() - origDestLength == numBytes);

  return true;
}


ByteCount TextDocumentCore::countBytesInRange(TextMCoordRange const &range) const
{
  // TODO: Inefficient!
  ArrayStack<char> arr;
  this->getTextForRange(range, arr);
  return ByteCount(arr.length());
}


bool TextDocumentCore::adjustMCoord(TextMCoord /*INOUT*/ &tc) const
{
  if (!validLine(tc.m_line)) {
    tc = endCoord();
    return true;
  }

  if (tc.m_byteIndex < 0) {
    tc.m_byteIndex.set(0);
    return true;
  }

  auto len = lineLengthByteIndex(tc.m_line);
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
    ByteDifference lenBytes = range.m_end.m_byteIndex - range.m_start.m_byteIndex;

    this->getPartialLine(range.m_start, dest, ByteCount(lenBytes));
    return;
  }

  // Right half of range start line.
  this->getTextForRange(
    TextMCoordRange(range.m_start, this->lineEndCoord(range.m_start.m_line)),
    dest);

  // Full lines between start and end.
  for (LineIndex i = range.m_start.m_line.succ(); i < range.m_end.m_line; ++i) {
    dest.push('\n');
    this->getWholeLine(i, dest);
  }

  // Left half of end line.
  dest.push('\n');
  this->getTextForRange(
    TextMCoordRange(this->lineBeginCoord(range.m_end.m_line), range.m_end),
    dest);
}


void TextDocumentCore::getWholeLine(LineIndex line,
  ArrayStack<char> /*INOUT*/ &dest) const
{
  bc(line);
  this->getTextForRange(
    TextMCoordRange(this->lineBeginCoord(line), this->lineEndCoord(line)),
    dest);
}


string TextDocumentCore::getWholeLineString(LineIndex line) const
{
  ArrayStack<char> text;
  this->getWholeLine(line, text);
  return toString(text);
}


std::string TextDocumentCore::getWholeLineStringOrRangeErrorMessage(
  LineIndex lineIndex,
  std::string const &fname) const
{
  if (validLine(lineIndex)) {
    return getWholeLineString(lineIndex);
  }
  else {
    return stringb(
      "<Line number " << lineIndex.toLineNumber() <<
      " is out of range for " << doubleQuote(fname) <<
      ", which has " << numLines() <<
      " lines.>");
  }
}


ByteCount TextDocumentCore::countLeadingSpacesTabs(LineIndex line) const
{
  ByteCount ret(0);
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


ByteCount TextDocumentCore::countTrailingSpacesTabs(LineIndex line) const
{
  ByteCount ret(0);
  for (LineIterator it(*this, line); it.has(); it.advByte()) {
    if (isSpaceOrTab(it.byteAt())) {
      ret++;
    }
    else {
      ret.set(0);
    }
  }
  return ret;
}


void TextDocumentCore::addObserver(TextDocumentObserver *observer) const
{
  TRACE1("TDC at " << (void*)this << " adding observer: " << (void*)observer);
  this->m_observers.appendNewItem(observer);
}

void TextDocumentCore::removeObserver(TextDocumentObserver *observer) const
{
  TRACE1("TDC at " << (void*)this << " removing observer: " << (void*)observer);
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
  TextDocumentCore const &tdc, LineIndex line)
:
  m_tdc(&tdc),
  m_isRecentLine(false),
  m_nonRecentLine(NULL),
  m_totalBytes(0),
  m_byteOffset(0)
{
  if (line == m_tdc->m_recentIndex) {
    m_isRecentLine = true;
    m_totalBytes = m_tdc->m_recentLine.length();
  }
  else if (line < m_tdc->numLines()) {
    TextDocumentLine const &tdl = m_tdc->getMLine(line);
    m_nonRecentLine = tdl.m_bytes;       // Might be NULL.
    m_totalBytes = tdl.length();
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
  return m_byteOffset < m_totalBytes;
}


int TextDocumentCore::LineIterator::byteAt() const
{
  xassertPrecondition(has());

  int ret = 0;
  if (m_nonRecentLine) {
    ret = (unsigned char)(m_nonRecentLine[m_byteOffset.get()]);
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
  xassertPrecondition(has());

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

void TextDocumentObserver::observeInsertLine(TextDocumentCore const &, LineIndex) NOEXCEPT
{}

void TextDocumentObserver::observeDeleteLine(TextDocumentCore const &, LineIndex) NOEXCEPT
{}

void TextDocumentObserver::observeInsertText(TextDocumentCore const &, TextMCoord, char const *, ByteCount) NOEXCEPT
{}

void TextDocumentObserver::observeDeleteText(TextDocumentCore const &, TextMCoord, ByteCount) NOEXCEPT
{}

void TextDocumentObserver::observeTotalChange(TextDocumentCore const &doc) NOEXCEPT
{}

void TextDocumentObserver::observeMetadataChange(TextDocumentCore const &) NOEXCEPT
{}


// EOF
