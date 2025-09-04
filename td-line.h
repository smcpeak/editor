// td-line.h
// TextDocumentLine

#ifndef EDITOR_TD_LINE_H
#define EDITOR_TD_LINE_H

#include "td-line-fwd.h"               // fwds for this module

#include "byte-count.h"                // ByteCount
#include "byte-difference.h"           // ByteDifference
#include "byte-index.h"                // ByteIndex
#include "td-core-fwd.h"               // TextDocumentCore [n]

#include "smbase/gdvalue-fwd.h"        // gdv::GDValue
#include "smbase/sm-macros.h"          // DMEMB, CMEMB
#include "smbase/xassert.h"            // xassert


/* Holds one line of text.  A line is a sequence of bytes (octets) that
   does *not* include a newline character.  An entire document is a
   non-empty sequence of such lines, separated by newlines.

   This class is agnostic to how those bytes are interpreted, although
   my general intent is to use UTF-8 encoding.

   This structure is meant to be an element of GapArray, where some
   instances are "active" and others are not.  As such, it can be
   copied using ordinary operator=, and the class that contains the
   GapArray is responsible for doing memory management.
*/
class TextDocumentLine {
  // TDC does the direct field manipulation.
  friend TextDocumentCore;

  // Test class (in `td-line-test.cc`) also does direct manipulation.
  friend TextDocumentLineTester;

private:     // data
  // Number of bytes in the line, which again never includes any
  // newline.  When this is 0, `m_bytes` is null.
  //
  // Logically this is a `ByteCount`, but because of how I do memory
  // management, this class has to be trivially copyable, so I use plain
  // `int` here.
  //
  int m_length;

  // If 'm_length' is not zero, pointer to array of bytes in the line,
  // allocated with 'new[]'.  This is nominally an owner pointer, except
  // when this instance is an inactive element in a GapArray.  Again,
  // the class that contains the GapArray does memory management.
  char *m_bytes;

public:      // methods
  // An empty line.
  TextDocumentLine() :
    m_length(0),
    m_bytes(nullptr)
  {}

  // This does a *shallow* copy.
  //
  // The '= default' definitions for the copy ctor, copy assignment, and
  // dtor are required to allow use of 'memmove' to copy instances of
  // this class in GapArray.
  TextDocumentLine(TextDocumentLine const &obj) = default;

  // This takes ownership of 'bytes'.
  //
  // Requires: (length.isZero()) == (bytes == nullptr)
  //
  // Requires: There are no newlines in bytes[0, length-1].
  explicit TextDocumentLine(char *bytes, ByteCount length);

  // The dtor does nothing.  The containing class is responsible for
  // memory management.
  ~TextDocumentLine() = default;

  // Assert invariants.
  void selfCheck() const;

  // Again, a shallow copy.
  TextDocumentLine& operator= (TextDocumentLine const &obj) = default;

  // True if both objects represent the same sequence of bytes.
  bool operator==(TextDocumentLine const &obj) const;

  bool operator!=(TextDocumentLine const &obj) const
    { return !operator==(obj); }

  // Return the contents as a string, without any final newline.
  operator gdv::GDValue() const;

  bool isEmpty() const { return m_length == 0; }

  char at(ByteIndex index) const
  {
    xassert(index < m_length);
    return m_bytes[index.get()];
  }

  // Number of bytes in the line, which never has a newline.
  ByteCount length() const
  {
    return ByteCount(m_length);
  }
};


#endif // EDITOR_TD_LINE_H
