// comment.h
// external interface to highlighter provided by comment.lex
// implementation in comment.lex's third section

#ifndef COMMENT_H
#define COMMENT_H

#include "inclexer.h"      // IncLexer
#include "lex_hilite.h"    // LexHighlighter

// lexer context class defined in comment.yy.cc
class CommentFlexLexer;


// incremental lexer for comment.lex
class CommentLexer : public IncLexer {
private:     // data
  CommentFlexLexer *lexer;       // (owner)

public:      // funcs
  CommentLexer();
  ~CommentLexer();

  // IncLexer funcs
  virtual void beginScan(BufferCore const *buffer, int line, LexerState state);
  virtual int getNextToken(Style &len);
  virtual LexerState getState() const;
};


// highlighter based on CommentLexer
class CommentHighlighter : public LexHighlighter {
private:     // data
  CommentLexer incLexer;

public:      // funcs
  CommentHighlighter(BufferCore const &buf)
    : LexHighlighter(buf, incLexer) {}
};


#endif // COMMENT_H
