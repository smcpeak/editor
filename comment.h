// comment.h
// lexer class for comment.lex

#ifndef COMMENT_H
#define COMMENT_H

// This included file is part of the Flex distribution.  It defines
// the base class yyFlexLexer.
#include <FlexLexer.h>

#include "inclexer.h"    // IncLexer

class BufferCore;        // buffer.h


// lexer object
class CommentLexer : public IncLexer, private yyFlexLexer {
private:     // data
  // source of text to lex
  BufferCore const *buffer;

  // which line we're working on
  int bufferLine;

  // length of that line
  int lineLength;

  // column (0-based) for next slurp into yyFlexLexer's internal buffer
  int nextSlurpCol;

private:     // funcs
  virtual int LexerInput(char* buf, int max_size);
  virtual int yylex();

public:      // funcs
  CommentLexer();
  ~CommentLexer();

  // IncLexer functions
  virtual void beginScan(BufferCore const *buffer, int line, int state);
  virtual bool getNextToken(int &len, int &code);
  virtual int getState() const;
};


#endif // COMMENT_H
