// textmcoord.h
// TextMCoord and TextMCoordRange classes.

#ifndef EDITOR_TEXTMCOORD_H
#define EDITOR_TEXTMCOORD_H

#include "textmcoord-fwd.h"            // fwds for this module

#include "byte-difference-fwd.h"       // ByteDifference [n]
#include "byte-index.h"                // ByteIndex
#include "line-index.h"                // LineIndex

#include "smbase/compare-util-iface.h" // DECLARE_COMPARETO_AND_DEFINE_RELATIONALS
#include "smbase/gdvalue-parser-fwd.h" // gdv::GDValueParser [n]
#include "smbase/gdvalue-fwd.h"        // gdv::GDValue [n]
#include "smbase/sm-macros.h"          // DMEMB

#include <iosfwd>                      // std::ostream


// The coordinates of a location within a text document *model*.  This
// is meant for use with TextDocumentCore, which is the model.
//
// Here, "model" is meant as opposed to "layout", the way the text
// appears onscreen.  On example of the difference is the treatment of
// Tab characters: in the model, Tab is one byte.  In the layout, Tab
// is a variable number of columns from 1 to 7.
//
// Another difference is the handling of UTF-8 multibyte code points,
// which are typically just one column in the layout.
class TextMCoord final {
public:      // data
  // 0-based line number.  "Line" is a concept defined by
  // TextDocumentCore.  This should be in [0,numLines()-1] for the
  // document it refers to, but this class does not enforce any
  // upper limit on the value.
  LineIndex m_line;

  // 0-based byte index into a line.  Should be in
  // [0,lineLengthBytes(line)] for the relevant document and line.  It
  // should not be in the middle of a multibyte code unit sequence.  An
  // index equal to the length refers to the end of the line, such that,
  // for example, inserting a character there would append it.
  ByteIndex m_byteIndex;

public:      // funcs
  TextMCoord() : m_line(0), m_byteIndex(0) {}
  TextMCoord(LineIndex line, ByteIndex byteIndex) : m_line(line), m_byteIndex(byteIndex) {}
  TextMCoord(TextMCoord const &obj) : DMEMB(m_line), DMEMB(m_byteIndex) {}

  TextMCoord& operator= (TextMCoord const &obj);

  // Lexicographic order by line then byteIndex.
  DECLARE_COMPARETO_AND_DEFINE_RELATIONALS(TextMCoord)

  bool isZero() const { return m_line.isZero() && m_byteIndex.isZero(); }

  // Return `*this` except with `m_byteIndex` increased by `n`.
  TextMCoord plusBytes(ByteDifference n) const;

  // Insert as "<line>:<byteIndex>".
  void insert(std::ostream &os) const;

  // Return "<line+1>:<byteIndex+1>".
  std::string toLineColNumberString() const;

  operator gdv::GDValue() const;
  explicit TextMCoord(gdv::GDValueParser const &p);
};


inline std::ostream& operator<< (std::ostream &os, TextMCoord const &tc)
{
  tc.insert(os);
  return os;
}


// Range of text identified by coordinate endpoints.
//
// Note that it is not possible from a TextMCoordRange alone to
// determine how many characters are enclosed since that depends on the
// document.
class TextMCoordRange final {
public:      // data
  // First byte in the range.
  TextMCoord m_start;

  // One past the last byte in the range.  For a text document, this can
  // be on the same line, or at the start of the next line so the range
  // includes a final newline.
  //
  // If start==end, the range is empty.
  //
  // It is legal for start to be greater than end, but the range is
  // again empty.  However, see rectify() and rectified().
  TextMCoord m_end;

public:      // funcs
  TextMCoordRange() : m_start(), m_end() {}
  TextMCoordRange(TextMCoord s, TextMCoord e) : m_start(s), m_end(e) {}

  TextMCoordRange(TextMCoordRange const &obj) : DMEMB(m_start), DMEMB(m_end) {}

  TextMCoordRange& operator= (TextMCoordRange const &obj);

  // For a range, the order is lexicographic, except the order of the
  // second element (`m_end`) is reversed.  That way, whenever one range
  // is contained in another, the larger one is always considered to be
  // less than the smaller one.  This can be interpreted as
  // "specificity", as the more specific range comes later.
  DECLARE_COMPARETO_AND_DEFINE_RELATIONALS(TextMCoordRange)

  // Both coordinates are zero.
  bool isZero() const { return m_start.isZero() && m_end.isZero(); }

  // True if both endpoints are on the same line.
  bool withinOneLine() const { return m_start.m_line == m_end.m_line; }

  // True if start <= end.
  bool isRectified() const { return m_start <= m_end; }

  // Swap 'start' and 'end'.
  void swapEnds();

  // True if end <= start.
  bool empty() const { return m_end <= m_start; }

  TextMCoordRange rectified() const {
    TextMCoordRange ret(*this);
    ret.rectify();
    return ret;
  }

  void rectify() {
    if (m_start > m_end) {
      swapEnds();
    }
  }

  // If the end is before the start, return a range where both endpoints
  // are where the start was, thus signifying an empty range at that
  // location.
  TextMCoordRange normalized() const {
    if (m_start > m_end) {
      return TextMCoordRange(m_start, m_start);
    }
    else {
      return *this;
    }
  }

  // True if the range contains `tc`, or is collapsed at `tc`.
  bool contains_orAtCollapsed(TextMCoord tc) const;

  // Insert as "<start>-<end>".
  void insert(std::ostream &os) const;

  operator gdv::GDValue() const;
  explicit TextMCoordRange(gdv::GDValueParser const &p);
};


inline std::ostream& operator<< (std::ostream &os, TextMCoordRange const &tcr)
{
  tcr.insert(os);
  return os;
}


// For a type with relational operators, return true if `value` is in
// `[start, end)`, or if `start==end==value`.
template <typename T>
bool rangeContains_orAtCollapsed(
  T const &start, T const &end, T const &value)
{
  if (start <= value) {
    if (value < end) {
      // In the non-collapsed range.
      return true;
    }

    if (value == end && value == start) {
      // At the collapsed range location.
      return true;
    }
  }

  return false;
}


// Convenience functions meant to be imported by test code in order to
// reduce verbosity there.  Non-test code should use the usual
// constructors for the additional type safety.
namespace textmcoord_test {
  // Build a `TextMCoord` from a pair of integers.
  TextMCoord tmc(int l, int b);

  // Build a `TextMCoordRange` from a quad of integers.
  TextMCoordRange tmcr(int sl, int sb, int el, int eb);
}


#endif // EDITOR_TEXTMCOORD_H
