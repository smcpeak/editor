// lex_hilite.h
// highlighter based on an incremental lexer

#ifndef LEX_HILITE_H
#define LEX_HILITE_H

// editor
#include "hilite.h"                    // Highlighter interface
#include "inclexer.h"                  // IncLexer, LexerState

// smbase
#include "smbase/refct-serf.h"         // RCSerf
#include "smbase/sm-noexcept.h"        // NOEXCEPT
#include "smbase/sm-override.h"        // OVERRIDE


// the highlighter
class LexHighlighter : public Highlighter {
private:     // data
  // Buffer we're observing.  Not NULL.
  RCSerf<TextDocumentCore const> buffer;

  // the lexer
  IncLexer &lexer;

  // map from line number to saved state at the end of that line
  typedef char LineState;
  GapArray<LineState> savedState;

  // range of lines whose contents have changed since the last time
  // their 'savedState' was computed; changedEnd is the first line
  // after the changed region that is *not* changed, so if
  // changedBegin==changedEnd then nothing has changed
  int changedBegin, changedEnd;

  // in addition the the changed region above, we maintain a line
  // for which no highlighting has been done at or below it (the
  // "water" metaphor is meant to suggest that we can't see below it);
  // invariant: if !changedIsEmpty() then waterline >= changedEnd
  int waterline;

private:     // funcs
  // check local invariants, fail assertion if they don't hold
  void checkInvar() const;

  // true if changed region is empty
  bool changedIsEmpty() const { return changedBegin == changedEnd; }

  // expand changed region to include at least 'line'
  void addToChanged(int line);

  // get saved state for the end of a line, returning 0 for negative lines
  LexerState getSavedState(int line);

  // set the saved state for 'line' to 'state', adjusting the changed
  // regions to exclude 'line'; the expectation is we're doing this
  // to one of the lines at the top edge of a contiguous changed region
  void saveLineState(int line, LexerState state);

public:      // funcs
  LexHighlighter(TextDocumentCore const &buf, IncLexer &lex);
  virtual ~LexHighlighter();

  // TextDocumentObserver funcs
  virtual void observeInsertLine(TextDocumentCore const &buf, int line) NOEXCEPT OVERRIDE;
  virtual void observeDeleteLine(TextDocumentCore const &buf, int line) NOEXCEPT OVERRIDE;
  virtual void observeInsertText(TextDocumentCore const &buf, TextMCoord tc, char const *text, int length) NOEXCEPT OVERRIDE;
  virtual void observeDeleteText(TextDocumentCore const &buf, TextMCoord tc, int length) NOEXCEPT OVERRIDE;
  virtual void observeTotalChange(TextDocumentCore const &doc) NOEXCEPT OVERRIDE;

  // Highlighter funcs
  virtual void highlight(TextDocumentCore const &buf, int line,
                         LineCategories &categories) OVERRIDE;
};


// make a highlighter
typedef LexHighlighter * /*owner*/ (*MakeHighlighterFunc)(TextDocumentCore const &buf);

// exercise a highlighter class, given a factory that makes them
void exerciseHighlighter(MakeHighlighterFunc func);

// For test/debug purpose, highlight 'line' in 'tdc' and print
// information about that to stdout.
void printHighlightedLine(TextDocumentCore const &tdc,
                          LexHighlighter &hi, int line);

// For test/debug, print highlight info about all lines in 'tdc'.
void printHighlightedLines(TextDocumentCore const &tdc,
                           LexHighlighter &hi);

// Test that 'hi', when run on 'inputFname', produces output matching a
// file whose name is 'inputFname' + ".hi".  Each output line is what
// LineCategories::asUnaryString() produces.  Throw an exception on test
// failure.
//
// 'tde' is a container for the document that 'hi' is already associated
// with.  It will be modified by the test.  (The presence of this
// parameter is due to the way highlighters bind themselves to
// documents.)
void testHighlighter(LexHighlighter &hi, TextDocumentAndEditor &tde,
                     string inputFname);


#endif // LEX_HILITE_H
