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
  void bc() const { bc(line(), col()); }

public:
  // initially empty
  Buffer();

  // -------------------- queries -----------------------

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

  // get the word following the given line/col, including any non-word
  // characters that precede that word; stop at end of line
  string getWordAfter(int line, int col) const;


  // get position of last+1 char in file
  void getLastPos(int &line, int &col) const;

  // on a particular line, get # of whitespace chars before first
  // non-ws char, or -1 if there are no non-ws chars
  int getIndentation(int line) const;

  // starting at 'line', go up until we find a line that is not
  // entirely blank (whitespace), and return the # of whitespace
  // chars to the left of the first non-whitespace char
  int getAboveIndentation(int line) const;


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


  // -------------------- modifications ------------------------
  // write the entire buffer contents to 'fname' ('readFile' is
  // available as a method of HistoryBuffer)
  void writeFile(char const *fname) const
    { ::writeFile(core(), fname); }


  // move by relative line/col
  void moveRelCursor(int deltaLine, int deltaCol);

  // move to absolute line/col
  void moveAbsCursor(int newLine, int newCol);

  // use a relative movement to go to a specific line/col; this is
  // used for restoring the cursor position after some sequence
  // of edits
  void moveRelCursorTo(int newLine, int newCol);
                                 
  // relative to same line, absolute to given column
  void moveAbsColumn(int newCol);

  // line++, col=0
  void moveToNextLineStart();

  // line--, col=length(line-1)
  void moveToPrevLineEnd();

  // advance cursor position forwards or backwards, wrapping
  // to the next/prev line at line edges
  void advanceWithWrap(bool backwards);


  // add whitespace to buffer as necessary so that the cursor becomes
  // within the defined buffer area
  void fillToCursor();


  // insert text that might have newline characters at the cursor;
  // line/col are updated to indicate the position at the end of the
  // inserted text; cursor must be a position within the defined
  // portion of the buffer
  void insertText(char const *text);

  void insertSpace() { insertText(" "); }
  void insertSpaces(int howMany);

  // split 'line' into two, putting everything after cursor column
  // into the next line; if 'col' is beyond the end of the line,
  // spaces are *not* appended to 'line' before inserting a blank line
  // after it; the function returns with line incremented by 1 and
  // col==0
  void insertNewline();


  // move the contents of 'line+1' onto the end of 'line'; it's
  // ok if 'line' is the last line (it's like deleting the newline
  // at the end of 'line')
  //void spliceNextLine(int line);

  // delete some characters to the right of the cursor; the cursor
  // must be in the defined area, and there must be at least 'len'
  // defined characters after it (possibly found by wrapping),
  // including newlines
  void deleteText(int len);

  void deleteChar() { deleteText(1); }

  // delete the characters between line1/col1 and line/col2; both
  // are truncated to ensure validity; final cursor is left at
  // line1/col1
  void deleteTextRange(int line1, int col1, int line2, int col2);


  // indent (or un-indent, if ind<0) the line range
  // [start,start+lines-1] by some # of spaces; if unindenting, but
  // there are not enough spaces, then the line is unindented as much
  // as possible w/o removing non-ws chars; the cursor is left in its
  // original position at the end
  void indentLines(int start, int lines, int ind);

};

ENUM_BITWISE_OPS(Buffer::FindStringFlags, Buffer::FS_ALL)


// save/restore cursor across an operation; uses a relative
// cursor movement to restore at the end, so the presumption
// is that only relative movements has appeared in between
class CursorRestorer {
  Buffer &buf;
  int origLine, origCol;

public:
  CursorRestorer(Buffer &b);
  ~CursorRestorer();
};


#endif // BUFFER_H
