// buffercore.h
// representation of a buffer of text (one file) in the editor

// see disussion at end of file, regarding mapping between a
// file's on-disk representation and this in-memory representation

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
  // This array is the spine of the editor.  Every element is either
  // NULL, meaning a blank line, or is an owner pointer to a
  // '\n'-terminated array of chars that represent the line's contents.
  //
  // When I designed this, I didn't pay any attention to character
  // encodings.  Currently, when it paints the screen, editor.cc
  // imposes the interpretation that the text is stored as some 8-bit
  // encoding, and the glyphs in my default (and currently only!) font
  // further constrain the encoding to specifically be Latin-1.
  //
  // That is somewhat at odds with using 'char' rather than 'unsigned
  // char', since in C++ it is implementation-defined whether plain
  // 'char' is signed, as I intend to represent values in [0,255].
  // However, changing everything to 'unsigned char' would accomplish
  // very little in practice (since I only interpret the meaning of the
  // data in one place, in editor.cc), and would not address the more
  // fundamental problem of being restricted to an 8-bit encoding.
  //
  // TODO: This unintentional and limited design needs to be replaced.
  // Note that it affects not just this member but the interface of
  // every member function that gets or manipulates the stored text,
  // or that deals with "character" positions or counts.
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
  // list of observers; changeable even when '*this' is const
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
  BufferCore();        // file initially contains one empty line (see comments at end)
  ~BufferCore();


  // ---- queries ----
  // # of lines stored; always at least 1
  int numLines() const { return lines.length(); }

  // length of a given line, not including the '\n'
  int lineLength(int line) const;

  // get part of a line's contents, starting at 'col' and getting
  // 'destLen' chars; all chars must be in the line now; the retrieved
  // text never includes the '\n' character
  void getLine(int line, int col, char *dest, int destLen) const;

  // Maximum length of a line.  TODO: Implement this properly (right
  // now it just uses the length of the longest line ever seen, even
  // if that line is subsequently deleted).
  int maxLineLength() const { return longestLengthSoFar; }

  // check if a given location is within or at the edge of the defined
  // buffer contents (i.e. such that an 'insertText' would be allowed)
  bool locationInDefined(int line, int col) const;

  // True if line/col is the very end of the defined area.
  bool locationAtEnd(int line, int col) const;


  // ---- manipulation interface ----
  // insert a new blank line, where the new line will be line 'line';
  // 'line' must be in [0,numLines()]
  void insertLine(int line);

  // delete a blank line; the line must *already* be blank!  also,
  // you can't delete the last line
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

// clear buffer contents, returning to just one empty line
void clear(BufferCore &buf);

// Note: Currently, the file I/O operations assume that LF is
// the sole line terminator.  Any CR characters in the file become
// part of the in-memory line contents, and will then be written
// out as such as well, like any other character.  This is not
// ideal of course.

// Clear 'buf', then read a file into it.  If the file cannot be
// opened, throws XOpen and does not modify 'buf'.  However, if
// there is a read error after that point, 'buf' will contain
// unpredictable partial file contents.
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
  virtual void observeInsertLine(BufferCore const &buf, int line);
  virtual void observeDeleteLine(BufferCore const &buf, int line);
  virtual void observeInsertText(BufferCore const &buf, int line, int col, char const *text, int length);
  virtual void observeDeleteText(BufferCore const &buf, int line, int col, int length);
};



/*

  For my purposes, mathematically a file is a sequence of lines, each
  of which is a sequence of characters.  BufferCore embodies this
  abstraction of what a file is.

  On disk, however, a file is a sequence of bytes.  (For now I'm going
  to ignore the distinction between bytes and characters.)  Obviously,
  we need to describe the mapping between the on-disk and in-memory
  abstractions.

  One possibility is to interpret an on-disk file as a sequence of
  line records, terminated by newlines.  Unfortunately, this doesn't
  work well for two reasons:
    - It can't handle files whose last line lacks a newline.
    - It doesn't match well with an editing paradigm where one can
      insert new text at an arbitrary cursor location, that text
      possibly containing newline characters.

  Therefore I adopt a slightly different interpretation, where an
  on-disk file is a sequence of lines *separated* by newlines.  Thus,
  even a 0-length file is interpreted as having one (empty) line.  By
  seeing newlines as separators instead of terminators, files lacking
  a newline are easy to handle, as are insertions that contain
  newlines.

  The one unexpected consequence of this mapping is that, since I want
  the mapping to be invertible, I must disallow the possibility of a
  file containing no lines at all, since there's no corresponding
  on-disk representation of that condition.  BufferCore maintains the
  invariant that there is always at least one line, so that we never
  have to deal with a file that it outside the disk-to-memory map
  range.


  The next key concept is that of a cursor.  Thinking of a file in
  its on-disk form, a cursor is a location between any two bytes,
  or at the beginning or end (which might be the same place).
  Isomorphically, in the in-memory or mathematical descriptions, a
  cursor is "on" some line, and between any two bytes on that line,
  or at the beginning or end of that line.

  For example, the 0,0 line/col cursor, i.e. at the beginning of the
  first line, corresponds to the on-disk location of 0, i.e. at the
  beginning of the file.  The end of the first line is isomorphic
  with the location just before the first on-disk newline, and the
  beginning of the second line is just after that newline.  The
  end of the last line is the end of the file.

  CursorBuffer itself doesn't deal with cursors much, except in
  its locationInDefined() method, but the surrounding functions
  and classes (esp. HistoryBuffer and Buffer) do, and since
  BufferCore's design is motivated by the desire to support the
  notion of editing with a cursor, I include that notion in this
  discussion.

*/

#endif // BUFFERCORE_H
