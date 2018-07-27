// text-search.cc
// code for text-search.h

#include "text-search.h"               // this module

// editor
#include "debug-values.h"              // DEBUG_VALUES
#include "fasttime.h"                  // fastTimeMilliseconds

// smqtutil
#include "qtutil.h"                    // toQString(QString), operator<<(QString)

// smbase
#include "dev-warning.h"               // DEV_WARNING
#include "exc.h"                       // GENERIC_CATCH_BEGIN/END
#include "nonport.h"                   // getMilliseconds
#include "objcount.h"                  // CHECK_OBJECT_COUNT(className)
#include "sm-swap.h"                   // swap
#include "trace.h"                     // TRACE

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


void TextSearch::MatchExtent::insertSB(stringBuilder &sb) const
{
  sb << "(s=" << m_start << ",l=" << m_length << ')';
}


void TextSearch::MatchExtent::insertOstream(ostream &os) const
{
  os << "(s=" << m_start << ",l=" << m_length << ')';
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
  // Since we are going to recompute everything, reset this flag.
  m_incompleteMatches = false;

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

  // Performance measurement.
  unsigned startTime = fastTimeMilliseconds;
  unsigned totalDeletionTime = 0;

  // Count of matches during this recomputation.
  int matchesFound = 0;

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

    // Scan the line for matches.
    if (searchStringLength == 0) {
      // Empty string never matches anything.
    }
    else {
      // Get the line of text.
      int lineLength = m_document->lineLengthBytes(line);
      contents.ensureIndexDoubler(lineLength);
      char *buffer = contents.getArrayNC();
      m_document->getPartialLine(TextMCoord(line, 0), buffer, lineLength);

      if (m_regex.get()) {
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

      // Check the match limit.
      matchesFound += lineMatches.length();
      if (matchesFound > m_matchCountLimit) {
        m_incompleteMatches = true;
        TRACE("TextSearch", "hit match limit of " << m_matchCountLimit);

        // Skip all remaining matches, but do continue iterating through
        // the file in order to discard previous matches.
        searchStringLength = 0;
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
        FastTimeAccumulator acc(totalDeletionTime);
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

  unsigned elapsed = fastTimeMilliseconds - startTime;
  if (elapsed > 100) {
    TRACE("TextSearch", "recomputeLineRange took " << elapsed <<
      " ms; totalDeletionTime=" << totalDeletionTime);
  }
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


void TextSearch::observeInsertText(TextDocumentCore const &doc, TextMCoord tc, char const *text, int length) NOEXCEPT
{
  GENERIC_CATCH_BEGIN
  xassert(&doc == m_document);
  this->recomputeLine(tc.m_line);
  GENERIC_CATCH_END
}


void TextSearch::observeDeleteText(TextDocumentCore const &doc, TextMCoord tc, int length) NOEXCEPT
{
  GENERIC_CATCH_BEGIN
  xassert(&doc == m_document);
  this->recomputeLine(tc.m_line);
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
    m_matchCountLimit(1000),
    m_incompleteMatches(false),
    m_regex(NULL),
    m_lineToMatches()
{
  this->recomputeMatches();

  m_document->addObserver(this);

  fastTimeInitialize();       // This is idempotent.

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


template <class T>
static bool reversibleLT(bool reverse, T const &a, T const &b)
{
  return (reverse? b<a : a<b);
}

template <class T>
static bool reversibleLTE(bool reverse, T const &a, T const &b)
{
  return (reverse? b<=a : a<=b);
}

bool TextSearch::nextMatch(bool reverse, TextMCoordRange &range /*INOUT*/) const
{
  // Normalize coordinates.
  range.rectify();

  // Direction of travel.
  int inc = (reverse? -1 : +1);

  // Consider line with range start.
  if (countLineMatches(range.m_start.m_line)) {
    ArrayStack<MatchExtent> const &matches =
      this->getLineMatches(range.m_start.m_line);

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
      if (reversibleLT(reverse, m.m_start, range.m_start.m_byteIndex)) {
        // This match occurs before 'range.start'.
        continue;
      }

      // Where does this match end?
      TextMCoord matchEnd(range.m_start.m_line, m.m_start);
      m_document->walkCoordBytes(matchEnd, +m.m_length);

      // Now, how does its start compare to 'range.start'?
      if (m.m_start == range.m_start.m_byteIndex) {
        // How does the end compare to 'range.end'?
        if (reversibleLTE(reverse, matchEnd, range.m_end)) {
          // The match end is less than or equal to my current mark.  I
          // regard that as being the match we are on or already past,
          // so keep going.
          continue;
        }
        else {
          // The match end is further than mark.  We will return this
          // one, effectively extending the selection to get it.  (If
          // 'reverse', we are actually shrinking the match.)
          range.m_end = matchEnd;
          return true;
        }
      }
      else {
        // This is the first match after the range start.
        range.m_start.m_byteIndex = m.m_start;
        range.m_end = matchEnd;
        return true;
      }
    }
  }

  // Consider other lines.
  range.m_start.m_line += inc;
  while (reverse? 0 <= range.m_start.m_line :
                  range.m_start.m_line < this->documentLines()) {
    if (countLineMatches(range.m_start.m_line)) {
      // Grab the extreme match on this line.
      ArrayStack<MatchExtent> const &matches =
        this->getLineMatches(range.m_start.m_line);
      int i = (reverse? matches.length()-1 : 0);
      MatchExtent const &m = matches[i];

      // Found first match after 'range.start' on another line.
      range.m_start.m_byteIndex = m.m_start;
      range.m_end = range.m_start;
      m_document->walkCoordBytes(range.m_end, +m.m_length);
      return true;
    }

    // Keep looking on subsequent lines.
    range.m_start.m_line += inc;
  }

  return false;
}


bool TextSearch::rangeIsMatch(TextMCoord const &a0, TextMCoord const &b0) const
{
  TextMCoord a(a0);
  TextMCoord b(b0);
  if (a > b) {
    swap(a,b);
  }

  if (a.m_line != b.m_line) {
    // Currently we never match across line boundaries.
    return false;
  }

  if (countLineMatches(a.m_line)) {
    ArrayStack<MatchExtent> const &matches =
      this->getLineMatches(a.m_line);

    for (int i=0; i < matches.length(); i++) {
      MatchExtent const &m = matches[i];
      if (m.m_start == a.m_byteIndex &&
          m.m_length == (b.m_byteIndex - a.m_byteIndex)) {
        return true;
      }
    }
  }

  return false;
}


string TextSearch::getReplacementText(string const &existing,
                                      string const &replaceSpec) const
{
  if (m_regex) {
    // Match the regular expression so we can get the capture groups.
    QRegularExpressionMatch match = m_regex->match(toQString(existing));
    if (!match.hasMatch()) {
      DEV_WARNING("TextSearch::getReplacementText failed to match regex; " <<
        DEBUG_VALUES4(m_searchString, m_regex->pattern(), existing, replaceSpec));
      return existing;
    }

    // Build up the replacement string by interpreting the backslash
    // escape sequences in 'replaceSpec'.
    stringBuilder sb;

    for (char const *p = replaceSpec.c_str(); *p; p++) {
      if (*p == '\\') {
        p++;

        if (*p == 0) {
          // Backslash at the end of the replacement string.  Just
          // interpret it literally and bail.
          sb << '\\';
          break;
        }
        else if ('0' <= *p && *p <= '9') {
          // Numbered capture group.
          int digit = (*p - '0');
          QString capture = match.captured(digit);
          sb << toString(capture);
        }
        else if (*p == 'n') {
          sb << '\n';
        }
        else if (*p == 't') {
          sb << '\t';
        }
        else if (*p == 'r') {
          sb << '\r';
        }
        else {
          // Everything else will be treated literally.
          sb << *p;
        }
      }
      else {
        sb << *p;
      }
    }

    return sb;
  }

  else {
    // Non-regex, replacement is treated literally.
    //
    // An idea here is to do something like Emacs does in "case fold"
    // mode, where it will do certain kinds of automatic capitalization.
    return replaceSpec;
  }
}


// EOF
