// textcoord.h
// TextCoord class.

#ifndef TEXTCOORD_H
#define TEXTCOORD_H

#include "macros.h"                    // DMEMB
#include "str.h"                       // stringBuilder

#include <iostream>                    // std::ostream


// The coordinates of a location within a text document.  This is meant
// for use with TextDocumentCore.
//
// Both line and column are 0-based, even though user interfaces
// usually use 1-based coordinates.  This is done because 0-based
// arithmetic is much more natural in C++.  The UI layer will have to
// do the translation to 1-based coordinates.
class TextCoord {
public:      // data
  // 0-based line number.  Should not be negative, although nothing in
  // this class prohibits that, and it could potentially be useful to
  // allow a negative value in the middle of a calculation.
  int line;

  // 0-based column number.  Should not be negative.
  int column;

public:      // funcs
  TextCoord() : line(0), column(0) {}
  TextCoord(int line_, int column_) : line(line_), column(column_) {}
  TextCoord(TextCoord const &obj) : DMEMB(line), DMEMB(column) {}

  TextCoord& operator= (TextCoord const &obj);

  bool operator== (TextCoord const &obj) const;

  // Lexicographic order by line then column.
  bool operator< (TextCoord const &obj) const;

  RELATIONAL_OPERATORS(TextCoord);

  bool isZero() const { return line==0 && column==0; }

  // Insert as "<line>:<col>".
  void insert(std::ostream &os) const;
  void insert(stringBuilder &sb) const;
};


inline std::ostream& operator<< (std::ostream &os, TextCoord const &tc)
{
  tc.insert(os);
  return os;
}

inline stringBuilder& operator<< (stringBuilder &sb, TextCoord const &tc)
{
  tc.insert(sb);
  return sb;
}


// Range of text identified by coordinate endpoints.
//
// Note that it is not possible from a TextCoordRange alone to
// determine how many characters are enclosed.  A TextDocument is
// required as well for that.
class TextCoordRange {
public:      // data
  // First cell in the range.
  TextCoord start;

  // One past the last cell in the range.  For a 2D text document, this
  // can be on the same line, or at the start of the next line so the
  // range includes a final newline.
  //
  // If start==end, the range is empty.
  //
  // It is legal for start to be greater than end, but the range is
  // again empty.  However, see rectified().
  TextCoord end;

public:      // funcs
  TextCoordRange() : start(), end() {}
  TextCoordRange(TextCoord s, TextCoord e) : start(s), end(e) {}

  TextCoordRange(TextCoordRange const &obj) : DMEMB(start), DMEMB(end) {}

  TextCoordRange& operator= (TextCoordRange const &obj);

  bool operator== (TextCoordRange const &obj) const;
  NOTEQUAL_OPERATOR(TextCoordRange);

  bool isZero() const { return start.isZero() && end.isZero(); }

  TextCoordRange swapEnds() const {
    return TextCoordRange(end, start);
  }

  TextCoordRange rectified() const {
    if (start > end) {
      return swapEnds();
    }
    else {
      return *this;
    }
  }

  // Insert as "<start>-<end>".
  void insert(std::ostream &os) const;
  void insert(stringBuilder &sb) const;
};


inline std::ostream& operator<< (std::ostream &os, TextCoordRange const &tcr)
{
  tcr.insert(os);
  return os;
}

inline stringBuilder& operator<< (stringBuilder &sb, TextCoordRange const &tcr)
{
  tcr.insert(sb);
  return sb;
}


#endif // TEXTCOORD_H
