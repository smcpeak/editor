// flexlexer.h
// implementation of an incremental lexer based on a flex lexer

#ifndef FLEXLEXER_H
#define FLEXLEXER_H

// This included file is part of the Flex distribution.  It defines
// the base class yyFlexLexer.
#include <FlexLexer.h>

#include "inclexer.h"    // IncLexer

class BufferCore;        // buffer.h


// called when flex lexer needs more data for its buffer
typedef int (*LexerInputFunc)(void *ths, char *buf, int max_size);


// flex lexer that fills its buffer via function pointer
class RawFlexLexer : public yyFlexLexer {
public:     // data
  // for implementation of LexerInput; these are set directly by client
  LexerInputFunc bufferFunc;
  void *ths;

protected:   // funcs
  virtual int LexerInput(char* buf, int max_size)
    { return bufferFunc(ths, buf, max_size); }

public:      // funcs
  RawFlexLexer() : bufferFunc(NULL), ths(NULL) {}

  // access current start state
  void setState(int state);
  int getState() const;
};


// incremental lexer built on a raw flex lexer
class IncFlexLexer : public IncLexer {
private:     // data
  // underlying raw lexer
  RawFlexLexer *rawLexer;            // (owner)

  // source of text to lex
  BufferCore const *buffer;          // (serf)

  // which line we're working on
  int bufferLine;

  // length of that line
  int lineLength;

  // column (0-based) for next slurp into yyFlexLexer's internal buffer
  int nextSlurpCol;

private:     // funcs
  int innerFillBuffer(char *buf, int max_size);
  static int fillBuffer(void *ths, char *buf, int max_size);

public:      // funcs
  IncFlexLexer(RawFlexLexer * /*owner*/ rawLexer);
  ~IncFlexLexer();

  // IncLexer functions
  virtual void beginScan(BufferCore const *buffer, int line, int state);
  virtual bool getNextToken(int &len, int &code);
  virtual int getState() const;
};


#endif // FLEXLEXER_H
