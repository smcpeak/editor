// position.h
// a position in a buffer

#ifndef POSITION_H
#define POSITION_H

#include "macros.h"      // NOTEQUAL_OPERATOR, etc.

class Buffer;            // buffer.h

// Position is a position in a buffer.  I explicitly *allow* the position
// to be beyond the right edge of a line.  Positions are always compared
// in terms of their line/col, regardless of whether the actual buffer
// text happens to have characters underneath those locations.
// (Symmetric: beyond EOF?  why not?)
class Position {
private:    // data
  Buffer *buffer;    // (serf) which buffer we refer to
  int _line;         // which line (0-based)
  int _col;          // which column (0-based)

public:
  Position(Buffer *buf);
  Position(Position const &obj);
  ~Position();

  // the defining accessors
  int line() const { return _line; }
  int col() const { return _col; }

  // move the position
  void set(int newLine, int newCol);
  void setLine(int line) { set(line, col()); }
  void setCol(int col) { set(line(), col); }

  void move(int deltaLines, int deltaCols)
    { set(_line+deltaLines, _col+deltaCols); }

  // true if the position is after the
  // last character in the last line
  bool beyondEnd() const;

  // move the position to the last character of
  // the last line
  void setToEnd();

  // assign one position to another; both must *already*
  // refer to the same buffer
  Position& operator = (Position const &obj);

  // comparisons, in terms of line/col (line dominates)
  bool operator == (Position const &obj) const;
  bool operator < (Position const &obj) const;
  RELATIONAL_OPERATORS(Position)        // relationals in terms of == and <
};


// given (references to) two position pointers, make it
// such that 'c1' is less-or-equal 'c2', by swapping
// if necessary
inline void positionPtrNormalize(Position *&c1, Position *&c2)
{
  if (*c2 < *c1) {
    Position *temp = c1;
    c1 = c2;
    c2 = temp;
  }
}


#endif // POSITION_H
