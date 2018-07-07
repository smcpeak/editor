// td-core.h
// Representation of a text document for use in a text editor.

#ifndef TD_CORE_H
#define TD_CORE_H

#include "gap.h"                       // GapArray
#include "textcoord.h"                 // TextCoord

#include "str.h"                       // string
#include "sobjlist.h"                  // SObjList

class TextDocument;                    // td.h

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
// (declared in td.h).
class TextDocumentCore {
public:      // static data
  // For testing purposes, this can be set to a non-zero value, and after
  // reading this many bytes, an error will be injected.
  static int s_injectedErrorCountdown;

private:     // data
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

  // List of observers.  This is mutable because the highlighter wants
  // to declare, through its signature, that it does not modify the
  // document on which it operates, yet it needs to make itself an
  // observer of that document.
  mutable SObjList<TextDocumentObserver> observers;

private:     // funcs
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

  // Non-atomic core of 'readFile'.  Failure will leave the file in
  // an unpredictable state.
  void nonAtomicReadFile(char const *fname);

public:    // funcs
  TextDocumentCore();        // one empty line
  ~TextDocumentCore();

  // ---------------------- document shape ------------------------
  // # of lines stored; always at least 1
  int numLines() const { return lines.length(); }

  // length of a given line, not including the '\n'
  int lineLength(int line) const;

  // check if a given location is within or at the edge of the defined
  // buffer contents (i.e. such that an 'insertText' would be allowed)
  bool validCoord(TextCoord tc) const;

  // Return the last valid coordinate.
  TextCoord endCoord() const;

  // Maximum length of a line.  TODO: Implement this properly (right
  // now it just uses the length of the longest line ever seen, even
  // if that line is subsequently deleted).
  int maxLineLength() const { return longestLengthSoFar; }

  // --------------------- line contents ------------------------
  // get part of a line's contents, starting at 'tc' and getting
  // 'destLen' chars; all chars must be in the line now; the retrieved
  // text never includes the '\n' character
  void getLine(TextCoord tc, char *dest, int destLen) const;

  // retrieve text that may span line boundaries; line boundaries are
  // represented in the returned string as newlines; the span begins at
  // 'tc' (which must be in the defined area) and proceeds for
  // 'textLen' chars, but if that goes beyond the end then this simply
  // returns false (otherwise true); if it returns true then exactly
  // 'textLen' chars have been written into 'text'
  bool getTextSpan(TextCoord tc, char *text, int textLen) const;

  // Return the number of consecutive spaces and tabs at the start of
  // the given line.
  int countLeadingSpacesTabs(int line) const;

  // Return the number of consecutive spaces and tabs at the end of
  // the given line.
  int countTrailingSpacesTabs(int line) const;

  // ----------------- manipulation interface -------------------
  // This interface is deliberately very simple to *implement*: you are
  // either inserting or removing *blank* lines, or are editing the
  // contents of a *single* line.  TextDocumentEditor, built on top of
  // this one, provides a more convenient interface for clients.

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

  // Delete 'length' characters at and the right of 'tc'.
  void deleteText(TextCoord tc, int length);

  // ---------------------- whole file -------------------------
  // clear buffer contents, returning to just one empty line
  void clear();

  // Swap the contents of two documents.  Afterward, logically, each
  // contains the sequence of lines originally in the other.  It is
  // not guaranteed that internal caches, etc., are swapped, since
  // those are not visible to clients.
  void swapWith(TextDocumentCore &other) noexcept;

  // write a file
  void writeFile(char const *fname) const;

  // Note: Currently, the file I/O operations assume that LF is
  // the sole line terminator.  Any CR characters in the file become
  // part of the in-memory line contents, and will then be written
  // out as such as well, like any other character.  This is not
  // ideal of course.
  //
  // Clear 'this', then read a file into it.  If there is an error,
  // throw an exception.  This function is guaranteed to only modify
  // the object if it succeeds; failure is atomic.
  void readFile(char const *fname);

  // ---------------------- observers ---------------------------
  // Add an observer.  It must not already be there.  This is 'const'
  // for the same reason 'observers' is mutable.
  void addObserver(TextDocumentObserver *observer) const;

  // Remove an observer.  It must be present now.
  void removeObserver(TextDocumentObserver *observer) const;

  // Send the 'observeUnsavedChangesChange' message to all observers.
  void notifyUnsavedChangesChange(TextDocument const *doc) const;

  // ---------------------- debugging ---------------------------
  // print internal rep
  void dumpRepresentation() const;

  // how much memory am I using?
  void printMemStats() const;
};


inline void swap(TextDocumentCore &a, TextDocumentCore &b) noexcept
{
  a.swapWith(b);
}


// Interface for observing changes to a TextDocumentCore.
//
// All methods have 'noexcept'.  From the perspective of the observee,
// these cannot fail, as there is nothing the observee can do about it
// nor an appropriate way to report it.  Observers are obligated to
// catch any exceptions they throw.
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
  virtual void observeInsertLine(TextDocumentCore const &buf, int line) noexcept;
  virtual void observeDeleteLine(TextDocumentCore const &buf, int line) noexcept;
  virtual void observeInsertText(TextDocumentCore const &buf, TextCoord tc, char const *text, int length) noexcept;
  virtual void observeDeleteText(TextDocumentCore const &buf, TextCoord tc, int length) noexcept;

  // The document has changed in some major way that does not easily
  // allow for incremental updates.  Observers must refresh completely.
  virtual void observeTotalChange(TextDocumentCore const &doc) noexcept;

  // This notification is sent to observers if the observee is actually
  // a TextDocument (i.e., with undo/redo history) and the "has unsaved
  // changes" property may have changed.
  //
  // This method is a slight abuse of the observer pattern.
  virtual void observeUnsavedChangesChange(TextDocument const *doc) noexcept;
};

#endif // TD_CORE_H
