// textlcoord.h
// TextLCoord and TextLCoordRange classes.

#ifndef TEXTLCOORD_H
#define TEXTLCOORD_H

#include "macros.h"                    // DMEMB
#include "str.h"                       // stringBuilder

#include <iostream>                    // std::ostream


// The coordinates of a location within a 2D text document *layout*.
// This is meant for use with TextDocumentEditor, which provides an
// interface for editing a text document as it appears when layed out.
//
// The coordinates identify a cell in an infinite, regular 2D grid.  The
// cell need not correspond to any code point in the document.  It could
// be beyond EOL, beyond EOF, in the middle of a code point that
// occupies multiple columns (e.g., Tab), or at a location containing
// multiple code points (e.g., composed characters or zero-width
// characters).
//
// Semantically, we think of the coordinate as being *between* code
// points.  Specifically, it is before any code point whose layout
// rectangle starts at or beyond the cell in the usual left-to-right,
// top-to-bottom writing order, and after any other.
//
// For comparison, see TextMCoord in textmcoord.h, which is the
// coordinate system for the document *model*.  The interplay between
// these two coordinate systems is essential to the design of the
// editor.
//
// Both line and column are 0-based.  The UI translates them to 1-based
// coordinates for interaction with the user.
class TextLCoord {
public:      // data
  // 0-based line number.  Should not be negative, although nothing in
  // this class prohibits that, and it could potentially be useful to
  // allow a negative value in the middle of a calculation.
  //
  // Eventually I plan to replace "line" with "row" in order to decouple
  // the vertical dimension of layout and model, just as "byte" and
  // "column" decouple the horizontal dimension.  But, currently, the
  // model and layout line numbers are always the same.
  int m_line;

  // 0-based column number.  Should not be negative.
  int m_column;

public:      // funcs
  TextLCoord() : m_line(0), m_column(0) {}
  TextLCoord(int line_, int column_) : m_line(line_), m_column(column_) {}
  TextLCoord(TextLCoord const &obj) : DMEMB(m_line), DMEMB(m_column) {}

  TextLCoord& operator= (TextLCoord const &obj);

  bool operator== (TextLCoord const &obj) const;

  // Lexicographic order by line then column.
  bool operator< (TextLCoord const &obj) const;

  RELATIONAL_OPERATORS(TextLCoord);

  bool isZero() const { return m_line==0 && m_column==0; }

  // Although not disallowed, we provide a convenient way to test that
  // coordinates are non-negative in case clients want to enforce that
  // in certain places.
  bool nonNegative() const { return m_line>=0 && m_column>=0; }

  // Insert as "<line>:<col>".
  void insert(std::ostream &os) const;
  void insert(stringBuilder &sb) const;
};


inline std::ostream& operator<< (std::ostream &os, TextLCoord const &tc)
{
  tc.insert(os);
  return os;
}

inline stringBuilder& operator<< (stringBuilder &sb, TextLCoord const &tc)
{
  tc.insert(sb);
  return sb;
}


// Range of text identified by coordinate endpoints.
class TextLCoordRange {
public:      // data
  // First cell in the range.
  TextLCoord m_start;

  // One past the last cell in the range.  The range identifies all of
  // the code points that are at-or-after 'm_start' and before 'm_end'.
  //
  // If start==end, the range is empty.
  //
  // It is legal for start to be greater than end, but the range is
  // again empty.  However, see rectify() and rectified().
  TextLCoord m_end;

public:      // funcs
  TextLCoordRange() : m_start(), m_end() {}
  TextLCoordRange(TextLCoord s, TextLCoord e) : m_start(s), m_end(e) {}

  TextLCoordRange(TextLCoordRange const &obj) : DMEMB(m_start), DMEMB(m_end) {}

  TextLCoordRange& operator= (TextLCoordRange const &obj);

  bool operator== (TextLCoordRange const &obj) const;
  NOTEQUAL_OPERATOR(TextLCoordRange);

  bool isZero() const { return m_start.isZero() && m_end.isZero(); }
  bool nonNegative() const { return m_start.nonNegative() && m_end.nonNegative(); }

  // True if both endpoints are on the same line.
  bool withinOneLine() const { return m_start.m_line == m_end.m_line; }

  bool isRectified() const { return m_start <= m_end; }

  // Swap 'start' and 'end'.
  void swapEnds();

  TextLCoordRange rectified() const {
    TextLCoordRange ret(*this);
    ret.rectify();
    return ret;
  }

  void rectify() {
    if (m_start > m_end) {
      swapEnds();
    }
  }

  // Insert as "<start>-<end>".
  void insert(std::ostream &os) const;
  void insert(stringBuilder &sb) const;
};


inline std::ostream& operator<< (std::ostream &os, TextLCoordRange const &tcr)
{
  tcr.insert(os);
  return os;
}

inline stringBuilder& operator<< (stringBuilder &sb, TextLCoordRange const &tcr)
{
  tcr.insert(sb);
  return sb;
}


#endif // TEXTLCOORD_H
