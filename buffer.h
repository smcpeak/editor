// buffer.h
// a buffer of text (one file) in the editor

#ifndef BUFFER_H
#define BUFFER_H

#include "macros.h"     // NOTEQUAL_OPERATOR

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
  // zero everything
  void init();

  // set numLines, realloc if necessary (with margin)
  void setNumLines(int num);

  // extend the # of lines if necessary so the 
  // given line number is valid
  void ensureLineExists(int n);

public:    // data
  Buffer() { init(); }
  Buffer(char const *initText, int initLength);
  ~Buffer();

  // two buffers are equal if they have the same number
  // of lines, and every line is equal (by TextLine's ==)
  bool operator == (Buffer const &obj) const;
  NOTEQUAL_OPERATOR(Buffer)     // defines !=

  void readFile(char const *fname);
  void writeFile(char const *fname);

  int totLines() const { return numLines; }

  TextLine const *getLineC(int lineNumber) const;
  TextLine *getLine(int lineNumber)
    { return const_cast<TextLine*>(getLineC(lineNumber)); }

  TextLine const *lastLineC() const
    { return getLineC(totLines()-1); }    

  // insert some new blank lines, where the first 
  // new line will be line number 'n' (0-based)
  void insertLinesAt(int n, int howmany);
  void insertLineAt(int n) { insertLinesAt(n, 1); }

  // remove some lines
  void removeLines(int startLine, int linesToRemove);

  // insert some text at a cursor; this will parse any
  // newlines into additional line separators
  void insertText(Cursor *c, char const *text, int length);

  // delete the text between two cursors; c1 must be
  // less than c2; updates 'c2'
  void deleteText(Cursor const *c1, Cursor *c2);
  
  // debugging: print internal rep
  void dumpRepresentation();
};

#endif // BUFFER_H
