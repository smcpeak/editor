// comment.h
// lexer class for comment.lex

#ifndef COMMENT_H
#define COMMENT_H

// This included file is part of the Flex distribution.  It defines
// the base class yyFlexLexer.
#include <FlexLexer.h>

class BufferCore;        // buffer.h


// lexer object
class CommentLexer : private yyFlexLexer {
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

  // begin scanning a buffer line; must call this before any calls
  // to 'getNextToken'; 'state' is the result of a prior getState()
  // call, where 0 is the beginning-of-file state
  void beginScan(BufferCore const *buffer, int line, int state);

  // get next token in line; returns false at end of line;
  // 'code' is numeric code returned by lexer action
  bool getNextToken(int &len, int &code);

  // get the lexing state now; usually called at end-of-line to
  // remember the start state for the next line
  int getState() const;
};


#endif // COMMENT_H
