// buffer.h
// a buffer of text (one file) in the editor

#ifndef BUFFER_H
#define BUFFER_H

#include "array.h"      // Array

class Cursor;           // cursor.h

// the contents of a file; any attempt to change the contents
// must go through the Buffer (or Line) interface
class Buffer {
public:    // types
  // one line in the file
  class Line : public Array<char> {
  public:
    // nothing else yet..
  };

private:   // data
  Array<Line> lines;     // array of lines in the file

public:
  Buffer();
  ~Buffer();

  void readFile(char const *fname);
  void writeFile(char const *fname);

  Line const *getLineC(int lineNumber);
  Line *getLine(int lineNumber)
    { return const_cast<Line*>(getLineC(lineNumber)); }

  // insert some text at a cursor
  void insertText(Cursor *c, char const *text, int length);

  // delete the text between two cursors; c1 must be
  // less than c2
  void deleteText(Cursor *c1, Cursor *c2);
  
  // debugging: print internal rep
  void dumpRepresentation();
};

#endif // BUFFER_H
