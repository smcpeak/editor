// textmcoord.h
// TextMCoord and TextMCoordRange classes.

#ifndef EDITOR_TEXTMCOORD_H
#define EDITOR_TEXTMCOORD_H

#include "smbase/compare-util-iface.h" // DECLARE_COMPARETO_AND_DEFINE_RELATIONALS
#include "smbase/gdvalue-fwd.h"        // gdv::GDValue
#include "smbase/sm-macros.h"          // DMEMB
#include "smbase/str.h"                // stringBuilder

#include <iostream>                    // std::ostream


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
class TextMCoord {
public:      // data
  // 0-based line number.  "Line" is a concept defined by
  // TextDocumentCore.  This should be in [0,numLines()-1] for the
  // document it refers to, but this class does not enforce any
  // constraints on the value.
  int m_line;

  // 0-based byte index into a line.  Should be in
  // [0,lineLengthBytes(line)] for the relevant document and line.  It
  // should not be in the middle of a multibyte code unit sequence.  An
  // index equal to the length refers to the end of the line, such that,
  // for example, inserting a character there would append it.
  int m_byteIndex;

public:      // funcs
  TextMCoord() : m_line(0), m_byteIndex(0) {}
  TextMCoord(int line, int byteIndex) : m_line(line), m_byteIndex(byteIndex) {}
  TextMCoord(TextMCoord const &obj) : DMEMB(m_line), DMEMB(m_byteIndex) {}

  TextMCoord& operator= (TextMCoord const &obj);

  // Lexicographic order by line then byteIndex.
  DECLARE_COMPARETO_AND_DEFINE_RELATIONALS(TextMCoord)

  bool isZero() const { return m_line==0 && m_byteIndex==0; }

  // Insert as "<line>:<byteIndex>".
  void insert(std::ostream &os) const;
  void insert(stringBuilder &sb) const;

  operator gdv::GDValue() const;
};


inline std::ostream& operator<< (std::ostream &os, TextMCoord const &tc)
{
  tc.insert(os);
  return os;
}

inline stringBuilder& operator<< (stringBuilder &sb, TextMCoord const &tc)
{
  tc.insert(sb);
  return sb;
}


// Range of text identified by coordinate endpoints.
//
// Note that it is not possible from a TextMCoordRange alone to
// determine how many characters are enclosed since that depends on the
// document.
class TextMCoordRange {
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

  DECLARE_COMPARETO_AND_DEFINE_RELATIONALS(TextMCoordRange)

  bool isZero() const { return m_start.isZero() && m_end.isZero(); }

  // True if both endpoints are on the same line.
  bool withinOneLine() const { return m_start.m_line == m_end.m_line; }

  bool isRectified() const { return m_start <= m_end; }

  // Swap 'start' and 'end'.
  void swapEnds();

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

  // Insert as "<start>-<end>".
  void insert(std::ostream &os) const;
  void insert(stringBuilder &sb) const;

  operator gdv::GDValue() const;
};


inline std::ostream& operator<< (std::ostream &os, TextMCoordRange const &tcr)
{
  tcr.insert(os);
  return os;
}

inline stringBuilder& operator<< (stringBuilder &sb, TextMCoordRange const &tcr)
{
  tcr.insert(sb);
  return sb;
}


#endif // EDITOR_TEXTMCOORD_H
