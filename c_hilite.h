// c_hilite.h
// C/C++ highlighter

#ifndef C_HILITE_H
#define C_HILITE_H

#include "hilite.h"        // Highlighter interface
#include "comment.h"       // CommentLexer

class C_Highlighter : public Highlighter {
private:     // data
  // for now, we always use this lexer
  CommentLexer lexer;

public:      // funcs
  C_Highlighter();
  ~C_Highlighter();

  // Highlighter funcs
  virtual void highlight(BufferCore const &buf, int line, LineStyle &style);
};

#endif // C_HILITE_H
