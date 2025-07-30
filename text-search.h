// text-search.h
// TextSearch class.

#ifndef TEXT_SEARCH_H
#define TEXT_SEARCH_H

// editor
#include "ogap.h"                      // OGapArray
#include "td-core.h"                   // TextDocumentCore

// smbase
#include "smbase/array.h"              // ArrayStack
#include "smbase/sm-macros.h"          // NO_OBJECT_COPIES
#include "smbase/owner.h"              // Owner
#include "smbase/refct-serf.h"         // RCSerf
#include "smbase/sm-iostream.h"        // ostream
#include "smbase/sm-noexcept.h"        // NOEXCEPT
#include "smbase/sm-override.h"        // OVERRIDE
#include "smbase/str.h"                // string

class QRegularExpression;


// This class computes a set of search hits within a TextDocumentCore,
// both from scratch and for incremental update.
class TextSearch : public TextDocumentObserver {
  NO_OBJECT_COPIES(TextSearch);

public:      // types
  // Flags that control the interpretation of 'm_searchString'.
  enum SearchStringFlags {
    SS_NONE                  = 0x00,   // Text is literal.

    SS_CASE_INSENSITIVE      = 0x01,   // Letter case ignored.
    SS_REGEX                 = 0x02,   // Text is a Perl regular expression.

    SS_ALL                   = 0x03
  };

  // Describes a match within a given line of text.
  //
  // TODO UTF-8: Review and revise.
  class MatchExtent {
  public:
    // Byte offset of the start of the match.
    int m_startByte;

    // Length of the match, in bytes.
    int m_lengthBytes;

  public:
    MatchExtent();
    MatchExtent(int start, int length);
    MatchExtent(MatchExtent const &obj);
    MatchExtent& operator= (MatchExtent const &obj);

    bool operator== (MatchExtent const &obj) const;
    NOTEQUAL_OPERATOR(MatchExtent);

    // Write as "(s=<start>,l=<length>)".
    void insertOstream(ostream &os) const;
  };

public:      // class data
  static int s_objectCount;

private:     // instance data
  // The document in which we are searching.  We also act as an
  // observer of this document in order to maintain the matches
  // incrementally.  Never NULL.
  RCSerf<TextDocumentCore const> const m_document;

  // Description of what to look for.  The syntax is dependent on
  // 'm_searchStringFlags'.  The empty string means to not search for
  // anything.  Initially empty.
  string m_searchString;

  // How to interpret 'm_searchString'.  Initially SS_NONE.
  SearchStringFlags m_searchStringFlags;

  // Maximum number of hits to find in a single invocation of
  // 'recomputeLineRange'.  If this is exceeded, 'm_incompleteMatches'
  // is set to true and recomputation stops, except that old matches
  // are still always cleared.  Initially 1000.
  //
  // Note that this is not a precise limit.  The point at which it is
  // hit can vary depending on how matches are arranged on lines and
  // the way recomputation gets triggered.
  int m_matchCountLimit;

  // True if we hit the match limit and hence the set of matches is
  // incmplete.
  bool m_incompleteMatches;

  // Regular expression object for SS_REGEX.  May be NULL.
  Owner<QRegularExpression> m_regex;

  // Map from line number to matches for that line.  If there are no
  // matches on the line, stores NULL.
  //
  // The matches for a line are sorted by 'm_start', then by 'm_length'.
  // There are no duplicates.
  //
  // This is kept up to date as we observe changes to 'm_document', or
  // the values of 'm_searchString' or 'm_searchStringFlags' are set by
  // the client.
  OGapArray<ArrayStack<MatchExtent> > m_lineToMatches;

private:     // funcs
  // Recompute the set of matches from scratch.
  void recomputeMatches();

  // Recompute a given range.  The document sizes have to be right.
  void recomputeLineRange(int startLine, int endLinePlusOne);

  // Recompute a single line.
  void recomputeLine(int line) { recomputeLineRange(line, line+1); }

  // Compute 'm_regex' from the search string and flags.
  void computeRegex();

public:      // funcs
  TextSearch(TextDocumentCore const *document);
  ~TextSearch();

  // Verify invariants *other* than that all the matches are correct.
  // These invariants are true after document change notifications have
  // been processed.
  void selfCheck() const;

  // Get the document.
  TextDocumentCore const *document() const { return m_document; }

  // Number of lines in the document, which establishes the range from
  // which 'countRangeMatches' can yield useful data.
  int documentLines() const { return m_lineToMatches.length(); }

  // True if we have an active search string.
  bool hasSearchString() const { return !m_searchString.empty(); }

  // Get current search string.
  string const &searchString() const { return m_searchString; }

  // Set the search string and recompute matches.
  void setSearchString(string const &searchString);

  // Get current search flags.
  SearchStringFlags searchStringFlags() const { return m_searchStringFlags; }

  // Set flags and recompute matches.
  void setSearchStringFlags(SearchStringFlags flags);

  // Set both string and flags, saving a recomputation.
  void setSearchStringAndFlags(string const &s, SearchStringFlags f);

  // Return false if the search string has a syntax error.  Depends on
  // the search string flags.  When there is a syntax error, there are
  // no matches.  The empty search string is always considered valid but
  // matches nothing.
  bool searchStringIsValid() const;

  // When '!searchStringIsValid()', a description of what is wrong with
  // it.  Otherwise, "".
  string searchStringSyntaxError() const;

  // When '!searchStringIsValid()', the character offset of the first
  // offending error in the string.  Otherwise -1.
  int searchStringErrorOffset() const;

  // True if we are searching by regex, and the regex ends with "$".
  bool searchStringEndsWithEOL() const;

  // Set/get limit on matches.
  int matchCountLimit() const { return m_matchCountLimit; }
  void setMatchCountLimit(int limit) { m_matchCountLimit = limit; }

  // True if the set of matches is incomplete.
  bool hasIncompleteMatches() const { return m_incompleteMatches; }

  // Count the matches within a given range of lines.  Lines beyond
  // the document's current contents silently yield a 0 count.
  int countRangeMatches(int startLine, int endPlusOneLine) const;

  // Count the matches above a line.
  int countMatchesAbove(int line) const
    { return countRangeMatches(0, line); }

  // Count the matches on a single line.
  int countLineMatches(int line) const
    { return countRangeMatches(line, line+1); }

  // Count the matches below a line.
  int countMatchesBelow(int line) const
    { return countRangeMatches(line+1, this->documentLines()); }

  // Count all matches.
  int countAllMatches() const
    { return countRangeMatches(0, this->documentLines()); }

  // Get the matches on a single line.  This can only be called if there
  // is at least one match on the line.  The returned reference is
  // invalidated by any subsequent TextSearch method invocation, even if
  // it is marked 'const'.
  //
  // Extents are ordered first by 'm_start', then 'm_length'.  There are
  // no duplicates.
  ArrayStack<MatchExtent> const &getLineMatches(int line) const;

  // Given the current 'range', locate the next match (previous match if
  // 'reverse') and set 'range' to it.  If there is no next match,
  // return false.
  //
  // This function begins by rectifying 'range'.
  //
  // If the return value is false, the output value of 'range' is
  // unspecified.
  bool nextMatch(bool reverse, TextMCoordRange &range /*INOUT*/) const;

  // Return true if there is a match starting at 'a' and going up to
  // but not including 'b'; or one from 'b' to 'a' in the same manner.
  bool rangeIsMatch(TextMCoord const &a, TextMCoord const &b) const;

  // Compute the replacement text for 'existing', given 'replaceSpec',
  // which is non-trivial when using regex search.
  string getReplacementText(string const &existing,
                            string const &replaceSpec) const;

  // TextDocumentObserver methods.
  virtual void observeInsertLine(TextDocumentCore const &buf, int line) NOEXCEPT OVERRIDE;
  virtual void observeDeleteLine(TextDocumentCore const &buf, int line) NOEXCEPT OVERRIDE;
  virtual void observeInsertText(TextDocumentCore const &buf, TextMCoord tc, char const *text, int length) NOEXCEPT OVERRIDE;
  virtual void observeDeleteText(TextDocumentCore const &buf, TextMCoord tc, int length) NOEXCEPT OVERRIDE;
  virtual void observeTotalChange(TextDocumentCore const &doc) NOEXCEPT OVERRIDE;
};


ENUM_BITWISE_OPS(TextSearch::SearchStringFlags, TextSearch::SS_ALL)


inline ostream& operator<< (ostream &os,
                            TextSearch::MatchExtent const &match)
{
  match.insertOstream(os);
  return os;
}


#endif // TEXT_SEARCH_H
