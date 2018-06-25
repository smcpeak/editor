// lex_hilite.h
// highlighter based on an incremental lexer

#ifndef LEX_HILITE_H
#define LEX_HILITE_H

#include "hilite.h"        // Highlighter interface
#include "inclexer.h"      // IncLexer, LexerState


// the highlighter
class LexHighlighter : public Highlighter {
private:     // data
  // buffer we're observing
  TextDocumentCore const &buffer;

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
  virtual void observeInsertLine(TextDocumentCore const &buf, int line) override;
  virtual void observeDeleteLine(TextDocumentCore const &buf, int line) override;
  virtual void observeInsertText(TextDocumentCore const &buf, TextCoord tc, char const *text, int length) override;
  virtual void observeDeleteText(TextDocumentCore const &buf, TextCoord tc, int length) override;

  // Highlighter funcs
  virtual void highlight(TextDocumentCore const &buf, int line,
                         LineCategories &categories) override;
};


// make a highlighter
typedef LexHighlighter * /*owner*/ (*MakeHighlighterFunc)(TextDocumentCore const &buf);

// exercise a highlighter class, given a factory that makes them
void exerciseHighlighter(MakeHighlighterFunc func);


#endif // LEX_HILITE_H
