// buffer.h
// a buffer of text (one file) in the editor

#ifndef BUFFER_H
#define BUFFER_H

#include "gap.h"        // GapArray

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
  int recent;

  // if recent != -1, then this holds the contents of that line,
  // and lines[recent] is NULL
  GapArray<char> recentLine;

  // invariants:
  //   - recent >= -1
  //   - if recent>=0, lines[recent] == NULL
  //   - if recent<0, recentLine.length() == 0
  //   - every lines[n] is NULL or valid

private:   // funcs
  // strlen, but NULL yields 0 and '\n' is terminator
  static int bufStrlen(char const *p);

  // bounds check line
  void bc(int line) const { xassert(0 <= line && line < numLines()); }

  // copy the given line into 'recentLine', with given hints as to
  // where the gap should go and how big it should be
  // postcondition: recent==line
  void attachRecent(int line, int insCol, int insLength);

  // copy contents of 'recentLine', if any, back into lines[];
  // postcondition: recent==-1
  void detachRecent();

public:    // funcs
  Buffer();
  ~Buffer();


  // ---- queries ----
  // # of lines stored
  int numLines() const { return lines.length(); }

  // length of a given line, not including the '\n'
  int lineLength(int line) const;

  // get part of a line's contents, starting at 'col' and getting
  // 'destLen' chars; all chars must be in the line now; the retrieved
  // text never includes the '\n' character
  void getLine(int line, int col, char *dest, int destLen) const;

  // maximum length of a line; TODO: implement this
  int maxLineLength() const { return 100; }


  // ---- manipulation interface ----
  // insert a new blank line, where the new line will be line 'line';
  // 'line' must be in [0,numLines()]
  void insertLine(int line);

  // delete a blank line; the line must *already* be blank!
  void deleteLine(int line);

  // insert text into a given line, starting at the given column;
  // 'col' must be in [0,lineLength(line)]; the inserted text must
  // *not* contain the '\n' character
  void insertText(int line, int col, char const *text, int length);

  // delete text
  void deleteText(int line, int col, int length);

  
  // ---- convenience ----         
  // insert the contents of 'fname' into the top of this buffer
  void readFile(char const *fname);

  // write the entire buffer contents to 'fname'
  void writeFile(char const *fname) const;


  // ---- debugging ----
  // print internal rep
  void dumpRepresentation() const;

  // how much memory am I using?
  void printMemStats() const;
};

#endif // BUFFER_H
