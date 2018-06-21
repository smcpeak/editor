// inclexer.h
// lexer interface with explicit access to state for incrementality

#ifndef INCLEXER_H
#define INCLEXER_H

#include "style.h"       // TextStyle

class BufferCore;        // buffer.h

// saved-state value; using enum type so it can't get confused
// with other ordinary integers
enum LexerState { LS_INITIAL=0 };

class IncLexer {
protected:
  virtual ~IncLexer() {}         // silence warning

public:
  // begin scanning a buffer line; must call this before any calls
  // to 'getNextToken'; 'state' is the result of a prior getState()
  // call, or LS_INITIAL for the beginning-of-file state
  virtual void beginScan(BufferCore const *buffer, int line, LexerState state)=0;

  // get next token in line, returning its length; 'code' signifies
  // what text style to use for the token, including the code for
  // the trailing segment (when it returns 0)a
  virtual int getNextToken(TextCategory &code)=0;

  // get the lexing state now; usually called at end-of-line to
  // remember the start state for the next line; used for incremetal
  // lexing
  virtual LexerState getState() const=0;
};

#endif // INCLEXER_H
