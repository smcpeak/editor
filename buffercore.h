// buffercore.h
// representation of a buffer of text (one file) in the editor

#ifndef BUFFERCORE_H
#define BUFFERCORE_H

#include "gap.h"        // GapArray
#include "str.h"        // string
#include "sobjlist.h"   // SObjList

// fwd in this file
class BufferObserver;


// the contents of a file; any attempt to read or write the contents
// must go through this interface
// NOTE: lines and columns are 0-based
class BufferCore {
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

  // length of the longest line this file has ever had; this is
  // my poor-man's substitute for a proper interval map, etc., to
  // be able to answer the 'maxLineLength()' query
  int longestLengthSoFar;

  // invariants:
  //   - recent >= -1
  //   - if recent>=0, lines[recent] == NULL
  //   - if recent<0, recentLine.length() == 0
  //   - every lines[n] is NULL or valid
  //   - longestLineSoFar >= 0

public:    // data
  // list of observers
  mutable SObjList<BufferObserver> observers;

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

  // update 'longestLineSoFar', given the existence of a line
  // that is 'len' long
  void seenLineLength(int len);

public:    // funcs
  BufferCore();
  ~BufferCore();


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
  int maxLineLength() const { return longestLengthSoFar; }
  
  // check if a given location is within or at the edge of the defined
  // buffer contents (i.e. such that an 'insertText' would be allowed)
  bool locationInDefined(int line, int col) const;


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


  // ---- debugging ----
  // print internal rep
  void dumpRepresentation() const;

  // how much memory am I using?
  void printMemStats() const;
};


// utilities:
// The functions here are organizationally like methods of BufferCore,
// except they cannot access that class's private fields.

// clear buffer contents
void clear(BufferCore &buf);

// clear, then read a file
void readFile(BufferCore &buf, char const *fname);

// write a file
void writeFile(BufferCore const &buf, char const *fname);


// walk the cursor forwards (right, then down; len>0) or backwards
// (left, then up; len<0) through the defined contents of the file;
// line/col must initially be in the defined area, but if by walking
// we get out of bounds, then the function simply returns false
// (otherwise true)
bool walkCursor(BufferCore const &buf, int &line, int &col, int len);

inline bool walkBackwards(BufferCore const &buf, int &line, int &col, int len)
  { return walkCursor(buf, line, col, -len); }

// truncate the given line/col so it's within the defined area
void truncateCursor(BufferCore const &buf, int &line, int &col);


// retrieve text that may span line boundaries; line boundaries are
// represented in the returned string as newlines; the span begins at
// line/col (which must be in the defined area) and proceeds for
// 'textLen' chars, but if that goes beyond the end then this simply
// returns false (otherwise true); if it returns true then exactly
// 'textLen' chars have been written into 'text'
bool getTextSpan(BufferCore const &buf, int line, int col,
                 char *text, int textLen);

// given a line/col that might be outside the buffer area (but must
// both be nonnegative), compute how many rows and spaces need to
// be added (to EOF, and 'line', respectively) so that line/col will
// be in the defined area
void computeSpaceFill(BufferCore const &buf, int line, int col,
                      int &rowfill, int &colfill);

// given two locations that are within the defined area, and with
// line1/col1 <= line2/col2, compute the # of chars between them,
// counting line boundaries as one char
int computeSpanLength(BufferCore const &buf, int line1, int col1,
                      int line2, int col2);


// interface for observing changes to a BufferCore
class BufferObserver {
protected:
  // this is to silence a g++ warning; it is *not* the case that
  // clients are allowed to delete objects known only as
  // BufferObserver implementors
  virtual ~BufferObserver() {}

public:
  // These are analogues of the BufferCore manipulation interface, but
  // we also pass the BufferCore itself so the observer doesn't need
  // to remember which buffer it's observing.  These are called
  // *after* the BufferCore updates its internal representation.  The
  // default implementations do nothing.
  virtual void insertLine(BufferCore const &buf, int line);
  virtual void deleteLine(BufferCore const &buf, int line);
  virtual void insertText(BufferCore const &buf, int line, int col, char const *text, int length);
  virtual void deleteText(BufferCore const &buf, int line, int col, int length);
};


#endif // BUFFERCORE_H
