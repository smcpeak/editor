// comment.h
// lexer class for comment.lex

#ifndef COMMENT_H
#define COMMENT_H

#include "flexlexer.h"   // RawFlexLexer, IncFlexLexer

// raw comment flex lexer
class CommentFlexLexer : public RawFlexLexer {
public:      // funcs
  virtual int yylex();
};


// lexer for comment.lex
class CommentLexer : public IncFlexLexer {
public:      // funcs
  CommentLexer() : IncFlexLexer(new CommentFlexLexer) {}
};


#endif // COMMENT_H
