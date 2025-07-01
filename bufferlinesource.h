// bufferlinesource.h
// Support class for flex-based incremental lexers.

#ifndef BUFFERLINESOURCE_H
#define BUFFERLINESOURCE_H

// editor
#include "td-core.h"                   // TextDocumentCore

// smbase
#include "smbase/array.h"              // ArrayStack
#include "smbase/refct-serf.h"         // RCSerf


// state for supplying flex with input from a line of a buffer
class BufferLineSource {
private:     // data
  // source of text to lex
  RCSerf<TextDocumentCore const> buffer;

  // which line we're working on
  int bufferLine;

  // Length of that line, including a synthetic final newline.
  int lineLength;

  // column (0-based) for next slurp into yyFlexLexer's internal buffer
  int nextSlurpCol;

  // Intermediate array into which we copy the data before copying it to
  // the 'buf' in 'fillBuffer'.
  //
  // Logically this could be a local variable in 'fillBuffer', but as an
  // optimization, it is stored in the class in order to reduce the
  // allocator traffic by reusing the array across calls.
  //
  // The existence of this member is a somewhat unfortunate consequence
  // of all the Document classes writing into ArrayStack but the Flex
  // lexer working with raw char*.  I could fix this in several ways,
  // but I'm also thinking of ditching Flex entirely for other reasons.
  ArrayStack<char> tmpArray;

public:      // funcs
  BufferLineSource();
  ~BufferLineSource();

  // set up the variables to begin reading from the given line
  void beginScan(TextDocumentCore const *buffer, int line);

  // read the next chunk of the current line, up to 'size' bytes;
  // returns # of bytes read, or 0 for end-of-input (end of line)
  int fillBuffer(void *dest, int size);

  // True if, after 'fillBuffer', we find that the line was empty.
  bool lineIsEmpty() const { return lineLength == 1; }
};


#endif // BUFFERLINESOURCE_H
