// flexlexer.h                
// some common elements of flex-based incremental lexers

#ifndef FLEXLEXER_H
#define FLEXLEXER_H

class BufferCore;        // buffer.h


// state for supplying flex with input from a line of a buffer
class BufferLineSource {
public:      // data
  // source of text to lex
  BufferCore const *buffer;          // (serf)

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
  void beginScan(BufferCore const *buffer, int line);

  // read the next chunk of the current line, up to 'max_size' bytes;
  // returns # of bytes read, or 0 for end-of-input (end of line)
  int fillBuffer(char *buf, int max_size);
};


#endif // FLEXLEXER_H
