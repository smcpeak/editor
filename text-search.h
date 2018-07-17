// text-search.h
// TextSearch class.

#ifndef TEXT_SEARCH_H
#define TEXT_SEARCH_H

// editor
#include "ogap.h"                      // OGapArray
#include "td-core.h"                   // TextDocumentCore

// smbase
#include "array.h"                     // ArrayStack
#include "macros.h"                    // NO_OBJECT_COPIES
#include "refct-serf.h"                // RCSerf
#include "sm-noexcept.h"               // NOEXCEPT
#include "sm-override.h"               // OVERRIDE
#include "str.h"                       // string


// This class computes a set of search hits within a TextDocumentCore,
// both from scratch and for incremental update.
class TextSearch : public TextDocumentObserver {
  NO_OBJECT_COPIES(TextSearch);

public:      // types
  // Flags that control the interpretation of 'm_searchString'.
  enum SearchStringFlags {
    SS_NONE                  = 0x00,   // Text is literal.

    SS_CASE_INSENSITIVE      = 0x01,   // Letter case ignored.

    SS_ALL                   = 0x01
  };

  // Describes a match within a given line of text.
  //
  // TODO UTF-8: Review and revise.
  class MatchExtent {
  public:
    // Byte offset of the start of the match.
    int m_start;

    // Length of the match, in bytes.
    int m_length;

  public:
    MatchExtent();
    MatchExtent(int start, int length);
    MatchExtent(MatchExtent const &obj);
    MatchExtent& operator= (MatchExtent const &obj);

    bool operator== (MatchExtent const &obj) const;
    NOTEQUAL_OPERATOR(MatchExtent);
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

  // Get the matches on a single line.  This can only be called if there
  // is at least one match on the line.  The returned reference is
  // invalidated by any subsequent TextSearch method invocation, even if
  // it is marked 'const'.
  //
  // Extents are ordered first by 'm_start', then 'm_length'.  There are
  // no duplicates.
  ArrayStack<MatchExtent> const &getLineMatches(int line) const;

  // Starting from 'tc', search forward through the matches until we
  // find one, including one starting at 'tc' itself.  If one is found,
  // return true, set 'match', and set 'tc' to the start of the match.
  // Otherwise return false.
  bool firstMatchOnOrAfter(
    MatchExtent &match /*OUT*/, TextCoord &tc /*INOUT*/) const;

  // Similar, but do *not* report a match on 'tc' itself, and if
  // 'reverse', go backwards instead of forwards.
  bool firstMatchBeforeOrAfter(
    bool reverse, MatchExtent &match /*OUT*/, TextCoord &tc /*INOUT*/) const;

  // Generalization of the above two.  If 'matchAtTC', return a match at
  // 'tc' itself.
  bool firstMatchBeforeOnOrAfter(
    bool reverse, bool matchAtTC,
    MatchExtent &match /*OUT*/, TextCoord &tc /*INOUT*/) const;

  // Return true if there is a match starting at 'a' and going up to
  // but not including 'b'; or one from 'b' to 'a' in the same manner.
  bool rangeIsMatch(TextCoord const &a, TextCoord const &b) const;

  // TextDocumentObserver methods.
  virtual void observeInsertLine(TextDocumentCore const &buf, int line) NOEXCEPT OVERRIDE;
  virtual void observeDeleteLine(TextDocumentCore const &buf, int line) NOEXCEPT OVERRIDE;
  virtual void observeInsertText(TextDocumentCore const &buf, TextCoord tc, char const *text, int length) NOEXCEPT OVERRIDE;
  virtual void observeDeleteText(TextDocumentCore const &buf, TextCoord tc, int length) NOEXCEPT OVERRIDE;
  virtual void observeTotalChange(TextDocumentCore const &doc) NOEXCEPT OVERRIDE;
};


#endif // TEXT_SEARCH_H
