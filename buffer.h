// buffer.h
// buffer as used by the editor:
//   - core representation (BufferCore)
//   - undo/redo history (HistoryBuffer)
//   - convenience functions (this module)

#ifndef BUFFER_H
#define BUFFER_H

#include "historybuf.h"   // HistoryBuffer


// a convenience layer on top of HistoryBuffer
class Buffer : public HistoryBuffer {
private:
  // check that line/col is positive
  static void pos(int line, int col);

  // check that a given line/col is within the defined portion of
  // the buffer (it's ok to be at the end of a line)
  void bc(int line, int col) const;

public:
  // initially empty
  Buffer();


  // write the entire buffer contents to 'fname' ('readFile' is
  // available as a method of HistoryBuffer)
  void writeFile(char const *fname) const
    { writeFile(getCoreC(), fname); }


  // line length, or 0 if it's beyond the end of the file
  int lineLengthLoose(int line) const;

  // get a range of text from a line, but if the position is outside
  // the defined range, pretend the line exists (if necessary) and
  // that there are space characters up to 'col+destLen' (if necessary)
  void getLineLoose(int line, int col, char *dest, int destLen) const;


  // retrieve the text between two positions, as in a text editor
  // where the positions are the selection endpoints and the user
  // wants a string to put in the clipboard; it must be the case
  // that line1/col1 <= line2/col2; characters outside defined area
  // are taken to be whitespace
  string getTextRange(int line1, int col1, int line2, int col2) const;

  // get a complete line
  string getWholeLine(int line) const
    { return getTextRange(line, 0, line, lineLength(line)); }

  // get the word following the cursor, including any non-word
  // characters that preceed that word; stop at end of line
  string getWordAfter(int line, int col) const;
     

  // get position of last+1 char in file
  void getLastPos(int &line, int &col) const;

  // advance cursor position forwards or backwards, wrapping
  // to the next/prev line at line edges
  void advanceWithWrap(bool backwards) const;

  // on a particular line, get # of whitespace chars before first
  // non-ws char, or -1 if there are no non-ws chars
  int getIndentation(int line) const;

  // starting at 'line', go up until we find a line that is not
  // entirely blank (whitespace), and return the # of whitespace
  // chars to the left of the first non-whitespace char
  int getAboveIndentation(int line) const;


  // split 'line' into two, putting everything after cursor column
  // into the next line; if 'col' is beyond the end of the line,
  // spaces are *not* appended to 'line' before inserting a blank line
  // after it; the function returns with line incremented by 1 and
  // col==0
  void insertNewline();

  // insert text that might have newline characters at the cursor;
  // line/col are updated to indicate the position at the end of the
  // inserted text; cursor must be a position within the defined
  // portion of the buffer
  void insertTextRange(char const *text);
  
  // indent (or un-indent, if ind<0) the given line by some # of spaces;
  // if unindenting, but there are not enough spaces, then the line
  // is unindented as much as possible w/o removing non-ws chars
  void indentLine(int line, int ind);


  // move the contents of 'line+1' onto the end of 'line'; it's
  // ok if 'line' is the last line (it's like deleting the newline
  // at the end of 'line')
  void spliceNextLine(int line);
  
  // delete the characters between line1/col1 and line/col2, both
  // of which must be valid locations (no; change this)
  void deleteTextRange(int line1, int col1, int line2, int col2);


  // flags for findString()
  enum FindStringFlags {
    FS_NONE                = 0x00, // nothing special

    FS_CASE_INSENSITIVE    = 0x01, // case insensitive
    FS_BACKWARDS           = 0x02, // search backwards in file
    FS_ADVANCE_ONCE        = 0x04, // advance meta-cursor once before searching
    FS_ONE_LINE            = 0x08, // only search the named line

    FS_ALL                 = 0x0F  // all flags
  };   

  // search from line/col to find the first occurrence of
  // 'text', and update line/col to the beginning of the
  // match; return false for no match; 'text' will not be
  // tested for matches that span multiple lines
  bool findString(int &line, int &col, char const *text,
                  FindStringFlags flags = FS_NONE) const;
};

ENUM_BITWISE_OPS(Buffer::FindStringFlags, Buffer::FS_ALL)


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


#endif // BUFFER_H
