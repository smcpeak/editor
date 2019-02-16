// bufferlinesource.cc
// code for bufferlinesource.h

#include "bufferlinesource.h"          // this module

// smbase
#include "td-core.h"                   // TextDocumentCore

// libc
#include <string.h>                    // memcpy


BufferLineSource::BufferLineSource()
  : buffer(NULL),
    bufferLine(0),
    lineLength(0),
    nextSlurpCol(0),
    tmpArray()
{}


BufferLineSource::~BufferLineSource()
{}


void BufferLineSource::beginScan(TextDocumentCore const *b, int line)
{
  // set up variables so we'll be able to do fillBuffer()
  buffer = b;
  bufferLine = line;
  lineLength = buffer->lineLengthBytes(line)+1;
  nextSlurpCol = 0;
}


// this is called by the Flex lexer when it needs more data for
// its internal buffer; interface is similar to read(2)
int BufferLineSource::fillBuffer(char* buf, int max_size)
{
  if (nextSlurpCol == lineLength) {
    return 0;         // EOL
  }

  int len = min(max_size, (lineLength-1)-nextSlurpCol);
  tmpArray.clear();
  buffer->getPartialLine(TextMCoord(bufferLine, nextSlurpCol), tmpArray, len);
  xassert(tmpArray.length() == len);
  nextSlurpCol += len;

  // If we reached the synthetic newline, and there is space for it, add it.
  if (nextSlurpCol == lineLength-1 && len < max_size) {
    tmpArray.push('\n');
    len++;
    nextSlurpCol++;
  }

  memcpy(buf, tmpArray.getArray(), len);

  return len;
}
