// buffer.h
// a buffer of text (one file) in the editor

#ifndef BUFFER_H
#define BUFFER_H

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
  // be able to answer to 'maxLineLength()' query
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


// a convenience layer on top of BufferCore; the implementations of
// thefunctions in Buffer are restricted to using BufferCore's
// public methods
class Buffer : public BufferCore {
private:                                                         
  // check that line/col is positive
  static void pos(int line, int col);

  // check that a given line/col is within the defined portion of
  // the buffer (it's ok to be at the end of a line)
  void bc(int line, int col) const;

public:    
  // initializes with a single empty line, which is expected to
  // be the representation an empty file (to accomodate files that
  // don't end with newlines)
  Buffer();

  // insert the contents of 'fname' into the top of this buffer
  void readFile(char const *fname);

  // write the entire buffer contents to 'fname'
  void writeFile(char const *fname) const;
  
  
  // line length, or 0 if it's beyond the end
  int lineLengthLoose(int line) const;

  // get a range of text from a line, but if the position is outside
  // the defined range, pretend the line exists (if necessary) and
  // that there are space characters up to 'col' (if necessary)
  void getLineLoose(int line, int col, char *dest, int destLen) const;


  // retrieve the text between two positions, as in a text editor
  // where the positions are the selection endpoints and the user
  // wants a string to put in the clipboard; it must be the case
  // that line1/col1 <= line2/col2; characters outside defined area
  // are taken to be whitespace
  string getTextRange(int line1, int col1, int line2, int col2) const;
                                        
  // split 'line' into two, putting everything after 'col' into the
  // next line; if 'col' is beyond the end of the line, spaces are
  // *not* appended to 'line' before inserting a blank line after it;
  // the function returns with line incremented by 1 and col==0
  void insertNewline(int &line, int &col);

  // insert text that might have newline characters at a particular
  // point in the buffer; line/col are updated to indicate the
  // position at the end of the inserted text; line/col must be
  // a position within the defined portion of the buffer
  void insertTextRange(int &line, int &col, char const *text);


  // move the contents of 'line+1' onto the end of 'line'; it's
  // ok if 'line' is the last line (it's like deleting the newline
  // at the end of 'line')
  void spliceNextLine(int line);
  
  // delete the characters between line1/col1 and line/col2, both
  // of which must be valid locations (no; change this)
  void deleteTextRange(int line1, int col1, int line2, int col2);
};


// interface for observing changes to a BufferCore
class BufferObserver {
public:
  virtual ~BufferObserver() {}       // silence warning
  
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


#endif // BUFFER_H
