// buffer.h
// a buffer of text (one file) in the editor

#ifndef BUFFER_H
#define BUFFER_H

class Cursor;           // cursor.h
class TextLine;         // textline.h

// the contents of a file; any attempt to change the contents
// must go through the Buffer (or Line) interface
class Buffer {
private:   // data
  TextLine *lines;       // (owner) array of lines in the file
  int numLines;          // # of lines in 'lines'
  int linesAllocated;    // # of entries allocated in 'lines'

  // invariant: entries 0 through numLines-1 are properly
  // initialized TextLines; outside this range, they are
  // not considered initialized

private:   // funcs         
  // set numLines, realloc if necessary (with margin)
  void setNumLines(int num);

public:    // data
  Buffer();
  ~Buffer();

  void readFile(char const *fname);
  void writeFile(char const *fname);

  TextLine const *getLineC(int lineNumber);
  TextLine *getLine(int lineNumber)
    { return const_cast<TextLine*>(getLineC(lineNumber)); }

  // insert some text at a cursor
  void insertText(Cursor *c, char const *text, int length);

  // delete the text between two cursors; c1 must be
  // less than c2
  void deleteText(Cursor *c1, Cursor *c2);
  
  // debugging: print internal rep
  void dumpRepresentation();
};

#endif // BUFFER_H
