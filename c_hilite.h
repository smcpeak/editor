// c_hilite.h
// C/C++ highlighter

#ifndef C_HILITE_H
#define C_HILITE_H

#include "hilite.h"        // Highlighter interface
#include "comment.h"       // CommentLexer

class C_Highlighter : public Highlighter {
private:     // data
  // buffer we're observing
  BufferCore const &buffer;

  // for now, we always use this lexer
  CommentLexer lexer;

  // map from line number to saved state at the end of that line
  typedef char LineState;
  GapArray<LineState> savedState;

  // range of lines whose contents have changed since the last time
  // their 'savedState' was computed; changedEnd is the first line
  // after the changed region that is *not* changed, so if
  // changedBegin==changedEnd then nothing has changed
  int changedBegin, changedEnd;

private:     // funcs
  // true if changed region is empty
  bool changedIsEmpty() const { return changedBegin == changedEnd; }

  // expand changed region to include at least 'line'
  void addToChanged(int line);
  
  // get saved state for the end of a line, returning 0 for negative lines
  LineState getSavedState(int line);

  void saveLineState(int line, LineState state);

public:      // funcs
  C_Highlighter(BufferCore const &buf);
  ~C_Highlighter();
  
  // BufferObserver funcs
  virtual void insertLine(BufferCore const &buf, int line);
  virtual void deleteLine(BufferCore const &buf, int line);
  virtual void insertText(BufferCore const &buf, int line, int col, char const *text, int length);
  virtual void deleteText(BufferCore const &buf, int line, int col, int length);

  // Highlighter funcs
  virtual void highlight(BufferCore const &buf, int line, LineStyle &style);
};

#endif // C_HILITE_H
