// lex_hilite.h
// highlighter based on an incremental lexer

#ifndef LEX_HILITE_H
#define LEX_HILITE_H

#include "hilite.h"        // Highlighter interface

class IncLexer;            // inclexer.h


// the highlighter
class LexHighlighter : public Highlighter {
private:     // data
  // buffer we're observing
  BufferCore const &buffer;

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
  LineState getSavedState(int line);

  // set the saved state for 'line' to 'state', adjusting the changed
  // regions to exclude 'line'; the expectation is we're doing this
  // to one of the lines at the top edge of a contiguous changed region
  void saveLineState(int line, LineState state);

public:      // funcs
  LexHighlighter(BufferCore const &buf, IncLexer &lex);
  ~LexHighlighter();
  
  // BufferObserver funcs
  virtual void insertLine(BufferCore const &buf, int line);
  virtual void deleteLine(BufferCore const &buf, int line);
  virtual void insertText(BufferCore const &buf, int line, int col, char const *text, int length);
  virtual void deleteText(BufferCore const &buf, int line, int col, int length);

  // Highlighter funcs
  virtual void highlight(BufferCore const &buf, int line, LineStyle &style);
};

#endif // LEX_HILITE_H
