// comment.h
// external interface to highlighter provided by comment.lex
// implementation in comment.lex's third section

#ifndef EDITOR_COMMENT_H
#define EDITOR_COMMENT_H

#include "inclexer.h"                  // IncLexer
#include "lex_hilite.h"                // LexHighlighter

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
  virtual void beginScan(TextDocumentCore const *buffer, LineIndex line, LexerState state);
  virtual int getNextToken(TextCategoryAOA &len);
  virtual LexerState getState() const;
};


// highlighter based on CommentLexer
class CommentHighlighter : public LexHighlighter {
private:     // data
  CommentLexer incLexer;

public:      // funcs
  CommentHighlighter(TextDocumentCore const &buf)
    : LexHighlighter(buf, incLexer) {}

  // Highlighter funcs
  virtual string highlighterName() const OVERRIDE;
};


#endif // EDITOR_COMMENT_H
