// bufferlinesource.cc
// code for bufferlinesource.h

#include "bufferlinesource.h"          // this module

#include "td-core.h"                   // TextDocumentCore


BufferLineSource::BufferLineSource()
  : buffer(NULL),
    bufferLine(0),
    lineLength(0),
    nextSlurpCol(0)
{}


BufferLineSource::~BufferLineSource()
{}


void BufferLineSource::beginScan(TextDocumentCore const *b, int line)
{
  // set up variables so we'll be able to do fillBuffer()
  buffer = b;
  bufferLine = line;
  lineLength = buffer->lineLengthBytes(line);
  nextSlurpCol = 0;
}


// this is called by the Flex lexer when it needs more data for
// its internal buffer; interface is similar to read(2)
int BufferLineSource::fillBuffer(char* buf, int max_size)
{
  if (nextSlurpCol == lineLength) {
    return 0;         // EOL
  }

  int len = min(max_size, lineLength-nextSlurpCol);
  buffer->getPartialLine(TextMCoord(bufferLine, nextSlurpCol), buf, len);
  nextSlurpCol += len;

  return len;
}
