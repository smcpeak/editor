// buffer.h
// a buffer of text (one file) in the editor

#ifndef BUFFER_H
#define BUFFER_H

#include "macros.h"      // NOTEQUAL_OPERATOR
#include "gaparray.h"    // GapArray

// the contents of a file; any attempt to read or write the contents
// must go through this interface
// NOTE: lines and columns are 0-based
class Buffer {
private:   // data
  // the spine of the editor; every element is either NULL, meaning a
  // blank line, or is an owner pointer to a '\n'-terminated array of
  // chars that represent the line's contents
  GapArray<char*> lines;

  // the most-recently edited line number, or -1 to mean that
  // no line's contents are stored
  int recentLineNum;

  // if recentLineNum != -1, then this holds the contents of that line,
  // overriding the contents stored in lines[recentLineNum]
  GapArray<char> recentLine;

public:    // data
  Buffer();
  ~Buffer();


  // ---- queries ----
  // two buffers are equal if they have the same number
  // of lines, and every line is equal
  bool operator == (Buffer const &obj) const;
  NOTEQUAL_OPERATOR(Buffer)     // defines !=

  // # of lines stored
  int numLines() const { return lines.length(); }

  // length of a given line
  int lineLength(int line) const;


  // ---- manipulation interface ----
  // insert a new blank line, where the new line will be line 'line';
  // 'line' must be in [0,numLines()]
  void insertLine(int line);

  // delete a blank line; the line must *already* be blank!
  void deleteLine(int line);

  // insert text into a given line, starting at the given column;
  // that 'col' must be in [0,lineLength(line)]
  void insertText(int line, int col, char const *text, int length);

  // delete text
  void deleteText(int line, int col, int length);


  // ---- debugging ----
  // print internal rep
  void dumpRepresentation() const;

  // how much memory am I using?
  void printMemStats() const;
};

#endif // BUFFER_H
