// flexlexer.cc
// code for flexlexer.h

#include "flexlexer.h"     // this module
#include "buffer.h"        // BufferCore


// ------- begin: copied from flex output ------
/* Enter a start condition.  This macro really ought to take a parameter,
 * but we do it the disgusting crufty way forced on us by the ()-less
 * definition of BEGIN.
 */
#define BEGIN yy_start = 1 + 2 *

/* Translate the current start state into a value that can be later handed
 * to BEGIN to return to the state.  The YYSTATE alias is for lex
 * compatibility.
 */
#define YY_START ((yy_start - 1) / 2)
#define YYSTATE YY_START
// ------- end: copied from flex output ------


// -------------------- RawFlexLexer -------------------
void RawFlexLexer::setState(int state)
{
  BEGIN(state);
}

int RawFlexLexer::getState() const
{
  return YY_START;
}


// -------------------- IncFlexLexer -------------------
IncFlexLexer::IncFlexLexer(RawFlexLexer * /*owner*/ raw)
  : rawLexer(raw),
    buffer(NULL),
    bufferLine(0),
    lineLength(0),
    nextSlurpCol(0)
{
  rawLexer->bufferFunc = &IncFlexLexer::fillBuffer;
  rawLexer->ths = (void*)this;
}


IncFlexLexer::~IncFlexLexer()
{
  delete rawLexer;
}


void IncFlexLexer::beginScan(BufferCore const *b, int line, int state)
{
  // set up variables so we'll be able to do LexerInput()
  buffer = b;
  bufferLine = line;
  lineLength = buffer->lineLength(line);
  nextSlurpCol = 0;
  rawLexer->setState(state);
}


// this is called by the Flex lexer when it needs more data for
// its internal buffer; interface is similar to read(2)
inline int IncFlexLexer::innerFillBuffer(char* buf, int max_size)
{
  if (nextSlurpCol == lineLength) {
    return 0;         // EOL
  }

  int len = min(max_size, lineLength-nextSlurpCol);
  buffer->getLine(bufferLine, nextSlurpCol, buf, len);
  nextSlurpCol += len;

  return len;
}

STATICDEF int IncFlexLexer::fillBuffer(void *ths, char* buf, int max_size)
{
  return ((IncFlexLexer*)ths)->innerFillBuffer(buf, max_size);
}


bool IncFlexLexer::getNextToken(int &len, int &code)
{
  code = rawLexer->yylex();
  if (!code) {
    return false;     // EOL
  }

  len = rawLexer->YYLeng();
  return true;
}


int IncFlexLexer::getState() const
{
  return rawLexer->getState();
}
