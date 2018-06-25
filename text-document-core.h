// text-document-core.h
// Representation of a text document for use in a text editor.

// see discussion at end of file, regarding mapping between a
// file's on-disk representation and this in-memory representation

#ifndef TEXT_DOCUMENT_CORE_H
#define TEXT_DOCUMENT_CORE_H

#include "gap.h"                       // GapArray
#include "textcoord.h"                 // TextCoord

#include "str.h"                       // string
#include "sobjlist.h"                  // SObjList

// Forward in this file.
class TextDocumentObserver;


// A text document is a non-empty sequence of lines.
//
// To convert them to an on-disk text file, a single newline character
// is inserted *between* every pair of lines.  Consequently, the
// document consisting of one empty line corresponds to an on-disk file
// with 0 bytes.
//
// A line is a possibly empty sequence of Latin-1 code points, each
// in [0,255].  TODO: Change to Unicode code points.
//
// Column numbers as conveyed by TextCoord are in units of code points.
// Among other things, that means it is possible to name the pieces of
// a combining sequence individually.  (But Latin-1 has none.)
//
// All uses of char* in this interface use Latin-1 encoding.  If 'char'
// is signed, the values [-128,-1] are interpreted as naming the code
// points in [128,255] instead.  TODO: Change to UTF-8 encoding.
//
// This class is the "core" of a text document because it does not have
// any facilities for undo and redo.  Those are added by TextDocument
// (declared in text-document.h).
class TextDocumentCore {
private:   // data
  // This array is the spine of the document.  Every element is either
  // NULL, meaning a blank line, or is an owner pointer to a
  // '\n'-terminated array of chars that represent the line's contents.
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
  mutable SObjList<TextDocumentObserver> observers;

private:   // funcs
  // strlen, but NULL yields 0 and '\n' is terminator
  static int bufStrlen(char const *p);

  // bounds check line
  void bc(int line) const { xassert(0 <= line && line < numLines()); }

  // Counds check a coordinate.
  void bctc(TextCoord tc) const { xassert(validCoord(tc)); }

  // copy the given line into 'recentLine', with given hints as to
  // where the gap should go and how big it should be
  // postcondition: recent==tc.line
  void attachRecent(TextCoord tc, int insLength);

  // copy contents of 'recentLine', if any, back into lines[];
  // postcondition: recent==-1
  void detachRecent();

  // update 'longestLineSoFar', given the existence of a line
  // that is 'len' long
  void seenLineLength(int len);

public:    // funcs
  TextDocumentCore();        // one empty line
  ~TextDocumentCore();


  // ---- queries ----
  // # of lines stored; always at least 1
  int numLines() const { return lines.length(); }

  // length of a given line, not including the '\n'
  int lineLength(int line) const;

  // check if a given location is within or at the edge of the defined
  // buffer contents (i.e. such that an 'insertText' would be allowed)
  bool validCoord(TextCoord tc) const;

  // get part of a line's contents, starting at 'tc' and getting
  // 'destLen' chars; all chars must be in the line now; the retrieved
  // text never includes the '\n' character
  //
  // TODO: I want a method to get an arbitrary span.  I also want one
  // to retrieve an entire line, with or without its newline character.
  void getLine(TextCoord tc, char *dest, int destLen) const;

  // Maximum length of a line.  TODO: Implement this properly (right
  // now it just uses the length of the longest line ever seen, even
  // if that line is subsequently deleted).
  int maxLineLength() const { return longestLengthSoFar; }


  // ---- manipulation interface ----

  // TODO: I would rather the fundamental interface consist of a single
  // 'insert' and single 'delete' method because this interface makes
  // the undo/redo history unnecessarily complex.

  // insert a new blank line, where the new line will be line 'line';
  // 'line' must be in [0,numLines()]
  void insertLine(int line);

  // delete a blank line; the line must *already* be blank!  also,
  // you can't delete the last line
  void deleteLine(int line);

  // insert text into a given line, starting at the given column;
  // 'col' must be in [0,lineLength(line)]; the inserted text must
  // *not* contain the '\n' character
  void insertText(TextCoord tc, char const *text, int length);

  // delete text
  void deleteText(TextCoord tc, int length);


  // ---- debugging ----
  // print internal rep
  void dumpRepresentation() const;

  // how much memory am I using?
  void printMemStats() const;
};


// utilities:
// The functions here are organizationally like methods of TextDocumentCore,
// except they cannot access that class's private fields.

// Return the coordinates of the end of 'doc'.
TextCoord endCoord(TextDocumentCore const &doc);

// clear buffer contents, returning to just one empty line
void clear(TextDocumentCore &buf);

// Note: Currently, the file I/O operations assume that LF is
// the sole line terminator.  Any CR characters in the file become
// part of the in-memory line contents, and will then be written
// out as such as well, like any other character.  This is not
// ideal of course.

// Clear 'buf', then read a file into it.  If the file cannot be
// opened, throws XOpen and does not modify 'buf'.  However, if
// there is a read error after that point, 'buf' will contain
// unpredictable partial file contents.
void readFile(TextDocumentCore &buf, char const *fname);

// write a file
void writeFile(TextDocumentCore const &buf, char const *fname);


// walk the cursor forwards (right, then down; len>0) or backwards
// (left, then up; len<0) through the defined contents of the file;
// line/col must initially be in the defined area, but if by walking
// we get out of bounds, then the function simply returns false
// (otherwise true)
bool walkCursor(TextDocumentCore const &buf, TextCoord &cursor, int len);

inline bool walkBackwards(TextDocumentCore const &buf, TextCoord &cursor, int len)
  { return walkCursor(buf, cursor, -len); }

// Truncate the given coordinate so it's within the defined area.
void truncateCursor(TextDocumentCore const &buf, TextCoord &tc);


// retrieve text that may span line boundaries; line boundaries are
// represented in the returned string as newlines; the span begins at
// 'tc' (which must be in the defined area) and proceeds for
// 'textLen' chars, but if that goes beyond the end then this simply
// returns false (otherwise true); if it returns true then exactly
// 'textLen' chars have been written into 'text'
bool getTextSpan(TextDocumentCore const &buf, TextCoord tc,
                 char *text, int textLen);

// given a coordinate that might be outside the buffer area (but must
// both be nonnegative), compute how many rows and spaces need to
// be added (to EOF, and 'line', respectively) so that coordinate will
// be in the defined area
void computeSpaceFill(TextDocumentCore const &buf, TextCoord tc,
                      int &rowfill, int &colfill);

// Given two locations that are within the defined area, and with
// tc1 <= tc2, compute the # of chars between them, counting line
// boundaries as one char.
int computeSpanLength(TextDocumentCore const &buf, TextCoord tc1,
                      TextCoord tc2);


// interface for observing changes to a TextDocumentCore
class TextDocumentObserver {
protected:
  // this is to silence a g++ warning; it is *not* the case that
  // clients are allowed to delete objects known only as
  // TextDocumentObserver implementors
  virtual ~TextDocumentObserver() {}

public:
  // These are analogues of the TextDocumentCore manipulation interface, but
  // we also pass the TextDocumentCore itself so the observer doesn't need
  // to remember which buffer it's observing.  These are called
  // *after* the TextDocumentCore updates its internal representation.  The
  // default implementations do nothing.
  virtual void observeInsertLine(TextDocumentCore const &buf, int line);
  virtual void observeDeleteLine(TextDocumentCore const &buf, int line);
  virtual void observeInsertText(TextDocumentCore const &buf, TextCoord tc, char const *text, int length);
  virtual void observeDeleteText(TextDocumentCore const &buf, TextCoord tc, int length);
};



/*
  2018-06-25: The following discussion is potentially out of date, as I
  am in the middle of a major refactoring.  Hopefully I will remember
  to revise it when I am done.

  For my purposes, mathematically a file is a sequence of lines, each
  of which is a sequence of characters.  TextDocumentCore embodies this
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
  on-disk representation of that condition.  TextDocumentCore maintains the
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
  its validCoord() method, but the surrounding functions
  and classes (esp. HistoryBuffer and Buffer) do, and since
  TextDocumentCore's design is motivated by the desire to support the
  notion of editing with a cursor, I include that notion in this
  discussion.

*/

#endif // TEXT_DOCUMENT_CORE_H
