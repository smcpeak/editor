// c_hilite.h
// C/C++ highlighter

#ifndef C_HILITE_H
#define C_HILITE_H

#include "lex_hilite.h"    // LexHighlighter
#include "comment.h"       // CommentLexer

class C_Highlighter : public LexHighlighter {
private:     // data
  // for now, we always use this lexer
  CommentLexer theLexer;

public:      // funcs
  C_Highlighter(BufferCore const &buf);
  ~C_Highlighter();
};

#endif // C_HILITE_H
