// bufferlinesource.h
// Support class for flex-based incremental lexers.

#ifndef BUFFERLINESOURCE_H
#define BUFFERLINESOURCE_H

#include "td-core.h"                   // TextDocumentCore

#include "refct-serf.h"                // RCSerf


// state for supplying flex with input from a line of a buffer
class BufferLineSource {
public:      // data
  // source of text to lex
  RCSerf<TextDocumentCore const> buffer;

  // which line we're working on
  int bufferLine;

  // length of that line
  int lineLength;

  // column (0-based) for next slurp into yyFlexLexer's internal buffer
  int nextSlurpCol;

public:      // funcs
  BufferLineSource();
  ~BufferLineSource();

  // set up the variables to begin reading from the given line
  void beginScan(TextDocumentCore const *buffer, int line);

  // read the next chunk of the current line, up to 'max_size' bytes;
  // returns # of bytes read, or 0 for end-of-input (end of line)
  int fillBuffer(char *buf, int max_size);
};


#endif // BUFFERLINESOURCE_H
