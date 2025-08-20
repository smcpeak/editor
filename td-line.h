// td-line.h
// TextDocumentLine

#ifndef TD_LINE_H
#define TD_LINE_H

#include "smbase/gdvalue-fwd.h"        // gdv::GDValue
#include "smbase/sm-macros.h"          // DMEMB, CMEMB
#include "smbase/xassert.h"            // xassert

#include <stddef.h>                    // NULL

// Holds one line of text.  A line is a sequence of bytes (octets).
//
// This class is agnostic to how those bytes are interpreted, although
// my general intent is to use UTF-8 encoding.
//
// This structure is meant to be an element of GapArray, where some
// instances are "active" and others are not.  As such, it can be
// copied using ordinary operator=, and the class that contains the
// GapArray is responsible for doing memory management.
//
// The data members are public to allow the client to manage memory, but
// the intention is the GapArray is private, limiting the code that
// directly accesses the data.
//
class TextDocumentLine {
public:
  // Number of bytes in the line.  If this is 0, then 'm_bytes' is NULL.
  unsigned m_length;

  // If 'm_length' is not zero, pointer to array of bytes in the line,
  // allocated with 'new[]'.  This is nominally an owner pointer, except
  // when this instance is an inactive element in a GapArray.  Again,
  // the class that contains the GapArray does memory management.
  //
  // TODO: Change this to 'unsigned char'.
  char *m_bytes;

public:
  // An empty line.
  TextDocumentLine() :
    m_length(0),
    m_bytes(NULL)
  {}

  // This does a *shallow* copy.
  //
  // The '= default' definitions for the copy ctor, copy assignment, and
  // dtor are required to allow use of 'memmove' to copy instances of
  // this class in GapArray.
  TextDocumentLine(TextDocumentLine const &obj) = default;

  // This takes ownership of 'bytes'.
  TextDocumentLine(unsigned length, char *bytes) :
    m_length(length),
    m_bytes(bytes)
  {}

  // The dtor does nothing.  The containing class is responsible for
  // memory management.
  ~TextDocumentLine() = default;

  // Again, a shallow copy.
  TextDocumentLine& operator= (TextDocumentLine const &obj) = default;

  // True if both objects represent the same sequence of bytes.
  bool operator==(TextDocumentLine const &obj) const;

  bool operator!=(TextDocumentLine const &obj) const
    { return !operator==(obj); }

  // Return the contents as a string, without any final newline.
  operator gdv::GDValue() const;

  bool isEmpty() const { return m_length == 0; }

  char at(unsigned index) const
  {
    xassert(index < m_length);
    return m_bytes[index];
  }

  // Length of sequence in bytes, not counting the assumed final newline
  // for a non-empty sequence.
  //
  // TODO: This is weird and inconsistent.
  unsigned lengthWithoutNL() const
  {
    if (m_length) {
      return m_length - 1;
    }
    else {
      return 0;
    }
  }
};


#endif // TD_LINE_H
