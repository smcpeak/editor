// cursor.h
// a position in a buffer

#ifndef CURSOR_H
#define CURSOR_H

#include "macros.h"      // NOTEQUAL_OPERATOR, etc.

class Buffer;            // buffer.h

// Cursor is a position in a buffer.  I explicitly *allow* the cursor
// to be beyond the right edge of a line.  Cursors are always compared
// in terms of their line/col, regardless of whether the actual buffer
// text happens to have characters underneath those locations.
// (Symmetric: beyond EOF?  why not?)
class Cursor {
private:    // data
  Buffer *buffer;    // (serf) which buffer we refer to
  int line;          // which line (0-based)
  int col;           // which column (0-based)

public:
  Cursor(Buffer *buf);
  Cursor(Cursor const &obj);
  ~Cursor();                

  // the defining accessors
  int getLine() const { return line; }
  int getCol() const { return col; }

  // assign one cursor to another; both must *already*
  // refer to the same buffer
  Cursor& operator = (Cursor const &obj);

  // comparisons, in terms of line/col (line dominates)
  bool operator == (Cursor const &obj) const;
  bool operator < (Cursor const &obj) const;
  RELATIONAL_OPERATORS(Cursor)        // relationals in terms of == and <
};


// given (references to) two cursor pointers, make it
// such that 'c1' is less-or-equal 'c2', by swapping
// if necessary
inline void cursorPtrNormalize(Cursor *&c1, Cursor *&c2)
{
  if (*c2 < *c1) {
    Cursor *temp = c1;
    c1 = c2;
    c2 = temp;
  }
}


#endif // CURSOR_H
