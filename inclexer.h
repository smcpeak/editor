// inclexer.h
// lexer interface with explicit access to state for incrementality

#ifndef INCLEXER_H
#define INCLEXER_H

class BufferCore;        // buffer.h

class IncLexer {
protected:
  virtual ~IncLexer() {}         // silence warning

public:
  // begin scanning a buffer line; must call this before any calls
  // to 'getNextToken'; 'state' is the result of a prior getState()
  // call, or 0 for the beginning-of-file state
  virtual void beginScan(BufferCore const *buffer, int line, int state)=0;

  // get next token in line; returns 0 at end of line, or a code that
  // signifies what text style to use for the token; 'len' says how
  // long that token is
  virtual int getNextToken(int &len)=0;

  // get the lexing state now; usually called at end-of-line to
  // remember the start state for the next line; used for incremetal
  // lexing
  virtual int getState() const=0;
};

#endif // INCLEXER_H
