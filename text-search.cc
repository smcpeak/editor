// text-search.cc
// code for text-search.h

#include "text-search.h"               // this module

// smqtutil
#include "qtutil.h"                    // toQString(QString)

// smbase
#include "exc.h"                       // GENERIC_CATCH_BEGIN/END
#include "objcount.h"                  // CHECK_OBJECT_COUNT(className)
#include "sm-swap.h"                   // swap

// Qt
#include <QRegularExpression>
#include <QString>

// libc
#include <string.h>                    // strncmp
#include <strings.h>                   // strncasecmp (POSIX)


// ------------------------- MatchExtent ---------------------------
TextSearch::MatchExtent::MatchExtent()
  : m_start(0),
    m_length(0)
{}


TextSearch::MatchExtent::MatchExtent(int start, int length)
  : m_start(start),
    m_length(length)
{}


TextSearch::MatchExtent::MatchExtent(MatchExtent const &obj)
  : DMEMB(m_start),
    DMEMB(m_length)
{}


TextSearch::MatchExtent& TextSearch::MatchExtent::operator= (MatchExtent const &obj)
{
  CMEMB(m_start);
  CMEMB(m_length);
  return *this;
}


bool TextSearch::MatchExtent::operator== (MatchExtent const &obj) const
{
  return EMEMB(m_start) &&
         EMEMB(m_length);
}


// -------------------------- TextSearch ---------------------------
int TextSearch::s_objectCount = 0;

CHECK_OBJECT_COUNT(TextSearch);


// This is used by the naive string matcher while doing a batch scan of
// the document.
static bool hasMatchAt(
  TextSearch::SearchStringFlags flags,
  char const *candidate,
  char const *searchString,
  int searchStringLength)
{
  if (flags & TextSearch::SS_CASE_INSENSITIVE) {
    return 0==strncasecmp(candidate, searchString, searchStringLength);
  }
  else {
    return 0==strncmp(candidate, searchString, searchStringLength);
  }
}


void TextSearch::recomputeMatches()
{
  // Adjust sizes to match.
  while (m_lineToMatches.length() < m_document->numLines()) {
    m_lineToMatches.insert(m_lineToMatches.length(), NULL);
  }
  while (m_lineToMatches.length() > m_document->numLines()) {
    m_lineToMatches.deleteElt(m_lineToMatches.length()-1);
  }

  this->recomputeLineRange(0, m_document->numLines());
}


void TextSearch::recomputeLineRange(int startLine, int endLinePlusOne)
{
  this->selfCheck();
  xassert(0 <= startLine &&
               startLine <= endLinePlusOne &&
                            endLinePlusOne <= m_document->numLines());

  // Get search string info into locals.
  char const *searchString = m_searchString.c_str();
  int searchStringLength = m_searchString.length();

  // Treat an invalid regex like an empty search string.
  if (m_regex.get() && !m_regex->isValid()) {
    searchStringLength = 0;
  }

  // Temporary array into which we copy line contents.
  GrowArray<char> contents(10);

  // Temporary array of extents for search hits.  This is rebuilt for
  // each line, but only rarely reallocated.
  ArrayStack<MatchExtent> lineMatches;

  for (int line=startLine; line < endLinePlusOne; line++) {
    // Discard matches from prior lines.
    lineMatches.clear();

    // Get the line of text.
    int lineLength = m_document->lineLength(line);
    contents.ensureIndexDoubler(lineLength);
    char *buffer = contents.getArrayNC();
    m_document->getLine(TextCoord(line, 0), buffer, lineLength);

    // Scan the line for matches.
    if (searchStringLength == 0) {
      // Empty string never matches anything.
    }
    else if (m_regex.get()) {
      // TODO: Converting every line from UTF-8 to UTF-16 is
      // inefficient!  It seems I can't even avoid the memory
      // allocation; there is almost a fast-path with
      // QTextDecoder::toUnicode(Qtring*), but it does not quite go all
      // the way.
      //
      // I probably need to find a different regex engine, ideally
      // something that can operate on UTF-8 directly.
      QString lineQString(QString::fromUtf8(buffer, lineLength));

      QRegularExpressionMatchIterator iter(m_regex->globalMatch(lineQString));
      while (iter.hasNext()) {
        QRegularExpressionMatch match(iter.next());

        // TODO UTF-8: This incorrectly equates UTF-16 code units with
        // UTF-8 octets.
        lineMatches.push(MatchExtent(
          match.capturedStart(),
          match.capturedLength()));
      }
    }
    else {
      // Naive, slow algorithm of repeated strncmps.
      int offset = 0;
      while (offset+searchStringLength <= lineLength) {
        if (hasMatchAt(m_searchStringFlags, buffer+offset,
                       searchString, searchStringLength)) {
          lineMatches.push(MatchExtent(offset, searchStringLength));

          // Move one past the match so that subsequent matches are not
          // adjacent, since the UI would show adjacent matches as if they
          // were one long match.
          //
          // Note: With the regex engine, I can get both adjacent and
          // zero-width matches.  The handling in EditorWidget isn't
          // great, but it is not catastrophic.
          offset += searchStringLength+1;
        }
        else {
          // This is, of course, very inefficient.
          offset++;
        }
      }
    }

    // Replace the corresponding element of 'm_lineToMatches' if it is
    // different from what we just computed.  This algorithm tries to
    // minimize allocator traffic in the common case that the matches
    // from a previous run are similar or identical to those now.
    ArrayStack<MatchExtent> *existing = m_lineToMatches.get(line);
    if (lineMatches.length() == 0) {
      if (existing == NULL) {
        // Both empty.
      }
      else {
        // Remove and deallocate.
        delete m_lineToMatches.replace(line, NULL);
      }
    }
    else {
      if (existing == NULL) {
        // Install copy of new array.
        m_lineToMatches.replace(line,
          new ArrayStack<MatchExtent>(lineMatches));
      }
      else if (lineMatches == *existing) {
        // Same contents.
      }
      else {
        // Copy contents.
        *existing = lineMatches;
      }
    }
  } // for(line)
}


void TextSearch::observeInsertLine(TextDocumentCore const &doc, int line) NOEXCEPT
{
  GENERIC_CATCH_BEGIN
  xassert(&doc == m_document);
  m_lineToMatches.insert(line, NULL);
  this->selfCheck();
  GENERIC_CATCH_END
}


void TextSearch::observeDeleteLine(TextDocumentCore const &doc, int line) NOEXCEPT
{
  GENERIC_CATCH_BEGIN
  xassert(&doc == m_document);
  m_lineToMatches.deleteElt(line);
  this->selfCheck();
  GENERIC_CATCH_END
}


void TextSearch::observeInsertText(TextDocumentCore const &doc, TextCoord tc, char const *text, int length) NOEXCEPT
{
  GENERIC_CATCH_BEGIN
  xassert(&doc == m_document);
  this->recomputeLine(tc.line);
  GENERIC_CATCH_END
}


void TextSearch::observeDeleteText(TextDocumentCore const &doc, TextCoord tc, int length) NOEXCEPT
{
  GENERIC_CATCH_BEGIN
  xassert(&doc == m_document);
  this->recomputeLine(tc.line);
  GENERIC_CATCH_END
}


void TextSearch::observeTotalChange(TextDocumentCore const &doc) NOEXCEPT
{
  GENERIC_CATCH_BEGIN
  xassert(&doc == m_document);
  this->recomputeMatches();
  GENERIC_CATCH_END
}


TextSearch::TextSearch(TextDocumentCore const *document)
  : TextDocumentObserver(),
    m_document(document),
    m_searchString(""),
    m_searchStringFlags(SS_NONE),
    m_regex(NULL),
    m_lineToMatches()
{
  this->recomputeMatches();

  m_document->addObserver(this);

  s_objectCount++;
}


TextSearch::~TextSearch()
{
  s_objectCount--;

  m_document->removeObserver(this);
}


void TextSearch::selfCheck() const
{
  xassert(m_lineToMatches.length() == m_document->numLines());
}


void TextSearch::setSearchString(string const &searchString)
{
  m_searchString = searchString;
  this->computeRegex();
  this->recomputeMatches();
}


void TextSearch::setSearchStringFlags(SearchStringFlags flags)
{
  m_searchStringFlags = flags;
  this->computeRegex();
  this->recomputeMatches();
}


void TextSearch::setSearchStringAndFlags(string const &s, SearchStringFlags f)
{
  m_searchString = s;
  m_searchStringFlags = f;
  this->computeRegex();
  this->recomputeMatches();
}


void TextSearch::computeRegex()
{
  if (m_searchStringFlags & SS_REGEX) {
    m_regex = new QRegularExpression(toQString(m_searchString),
      (m_searchStringFlags & SS_CASE_INSENSITIVE)?
        QRegularExpression::CaseInsensitiveOption :
        QRegularExpression::NoPatternOption);
  }
  else {
    m_regex.del();
  }
}


bool TextSearch::searchStringIsValid() const
{
  if (m_regex) {
    return m_regex->isValid();
  }
  else {
    return true;
  }
}


string TextSearch::searchStringSyntaxError() const
{
  if (m_regex && !m_regex->isValid()) {
    return toString(m_regex->errorString());
  }
  else {
    return "";
  }
}


int TextSearch::searchStringErrorOffset() const
{
  if (m_regex && !m_regex->isValid()) {
    return m_regex->patternErrorOffset();
  }
  else {
    return -1;
  }
}


int TextSearch::countRangeMatches(int startLine, int endPlusOneLine) const
{
  int ret = 0;

  for (int line=startLine; line < endPlusOneLine; line++) {
    if (0 <= line && line < m_lineToMatches.length()) {
      ArrayStack<MatchExtent> const *matches = m_lineToMatches.getC(line);
      if (matches) {
        ret += matches->length();
      }
    }
  }

  return ret;
}


ArrayStack<TextSearch::MatchExtent> const &TextSearch::getLineMatches(int line) const
{
  xassert(0 <= line && line < m_lineToMatches.length());
  ArrayStack<MatchExtent> const *matches = m_lineToMatches.getC(line);
  xassert(matches);
  return *matches;
}


bool TextSearch::firstMatchOnOrAfter(
  MatchExtent &match /*OUT*/, TextCoord &tc /*INOUT*/) const
{
  return this->firstMatchBeforeOnOrAfter(
    false /*reverse*/, true /*matchAtTC*/,
    match /*OUT*/, tc /*INOUT*/);
}

bool TextSearch::firstMatchOnOrBefore(
  MatchExtent &match /*OUT*/, TextCoord &tc /*INOUT*/) const
{
  return this->firstMatchBeforeOnOrAfter(
    true /*reverse*/, true /*matchAtTC*/,
    match /*OUT*/, tc /*INOUT*/);
}

bool TextSearch::firstMatchBeforeOrAfter(
  bool reverse, MatchExtent &match /*OUT*/, TextCoord &tc /*INOUT*/) const
{
  return this->firstMatchBeforeOnOrAfter(
    reverse, false /*matchAtTC*/,
    match /*OUT*/, tc /*INOUT*/);
}


static bool reversibleLT(bool reverse, int a, int b)
{
  return (reverse? b<a : a<b);
}

static bool reversibleLTE(bool reverse, int a, int b)
{
  return (reverse? b<=a : a<=b);
}

bool TextSearch::firstMatchBeforeOnOrAfter(
  bool reverse, bool matchAtTC,
  MatchExtent &match /*OUT*/, TextCoord &tc /*INOUT*/) const
{
  // Direction of travel.
  int inc = (reverse? -1 : +1);

  // Consider line with 'tc'.
  if (countLineMatches(tc.line)) {
    ArrayStack<MatchExtent> const &matches =
      this->getLineMatches(tc.line);

    // Nominally walk forward.
    int begin = 0;
    int end = matches.length()-1;

    // But maybe reverse.
    if (reverse) {
      swap(begin, end);
    }

    // Walk the matches in 'inc' direction.
    for (int i=begin; reversibleLTE(reverse, i, end); i += inc) {
      MatchExtent const &m = matches[i];
      if (reversibleLT(reverse, m.m_start, tc.column)) {
        // This match occurs $before 'tc'.
        continue;
      }
      if (m.m_start == tc.column && !matchAtTC) {
        // This match is at 'tc' but we don't want that.
        continue;
      }

      // Found first match $after 'tc' on its line.
      match = m;
      tc.column = m.m_start;
      return true;
    }
  }

  // Consider other lines.
  tc.line += inc;
  while (reverse? 0 <= tc.line : tc.line < this->documentLines()) {
    if (countLineMatches(tc.line)) {
      // Grab the extreme match on this line.
      ArrayStack<MatchExtent> const &matches =
        this->getLineMatches(tc.line);
      int i = (reverse? matches.length()-1 : 0);
      MatchExtent const &m = matches[i];

      // Found first match $after 'tc' on another line.
      match = m;
      tc.column = m.m_start;
      return true;
    }
    tc.line += inc;
  }

  return false;
}


bool TextSearch::rangeIsMatch(TextCoord const &a0, TextCoord const &b0) const
{
  TextCoord a(a0);
  TextCoord b(b0);
  if (a > b) {
    swap(a,b);
  }

  if (a.line != b.line) {
    // Currently we never match across line boundaries.
    return false;
  }

  if (countLineMatches(a.line)) {
    ArrayStack<MatchExtent> const &matches =
      this->getLineMatches(a.line);

    for (int i=0; i < matches.length(); i++) {
      MatchExtent const &m = matches[i];
      if (m.m_start == a.column &&
          m.m_length == (b.column - a.column)) {
        return true;
      }
    }
  }

  return false;
}


// EOF
