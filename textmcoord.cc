// textmcoord.cc
// code for textmcoord.h

#include "textmcoord.h"                // this module

#include "byte-difference.h"           // ByteDifference
#include "line-number.h"               // LineNumber

#include "smbase/compare-util.h"       // RET_IF_COMPARE_MEMBERS
#include "smbase/gdvalue-parser.h"     // gdv::GDValueParser
#include "smbase/gdvalue.h"            // gdv::GDValue
#include "smbase/sm-swap.h"            // swap
#include "smbase/stringb.h"            // stringb

#include <iostream>                    // std::ostream


using namespace gdv;
using namespace smbase;


// ------------------------ TextMCoord -----------------------------
TextMCoord& TextMCoord::operator= (TextMCoord const &obj)
{
  CMEMB(m_line);
  CMEMB(m_byteIndex);
  return *this;
}


int TextMCoord::compareTo(TextMCoord const &b) const
{
  auto const &a = *this;
  RET_IF_COMPARE_MEMBERS(m_line);
  RET_IF_COMPARE_MEMBERS(m_byteIndex);
  return 0;
}


TextMCoord TextMCoord::plusBytes(ByteDifference n) const
{
  return TextMCoord(m_line, m_byteIndex + n);
}


void TextMCoord::insert(std::ostream &os) const
{
  os << this->m_line << ':' << this->m_byteIndex;
}


std::string TextMCoord::toLineColNumberString() const
{
  return stringb(m_line.toLineNumber() << ':' <<
                 m_byteIndex.toByteColumnNumber());
}


TextMCoord::operator GDValue() const
{
  return GDVTaggedTuple("MC"_sym, {
    toGDValue(m_line),
    toGDValue(m_byteIndex)
  });
}


TextMCoord::TextMCoord(GDValueParser const &p)
:
  m_line(p.tupleGetValueAt(0)),
  m_byteIndex(p.tupleGetValueAt(1))
{
  p.checkTaggedTupleSize("MC", 2);
}


// ------------------------ TextMCoordRange -----------------------------
TextMCoordRange& TextMCoordRange::operator= (TextMCoordRange const &obj)
{
  CMEMB(m_start);
  CMEMB(m_end);
  return *this;
}


int TextMCoordRange::compareTo(TextMCoordRange const &b) const
{
  auto const &a = *this;
  RET_IF_COMPARE_MEMBERS(m_start);
  RET_IF_COMPARE_MEMBERS(m_end);
  return 0;
}


void TextMCoordRange::swapEnds()
{
  swap(m_start, m_end);
}


bool TextMCoordRange::contains_orAtCollapsed(TextMCoord tc) const
{
  return rangeContains_orAtCollapsed(m_start, m_end, tc);
}


void TextMCoordRange::insert(std::ostream &os) const
{
  os << this->m_start << '-' << this->m_end;
}


TextMCoordRange::operator GDValue() const
{
  return GDVTaggedTuple("MCR"_sym, {
    toGDValue(m_start),
    toGDValue(m_end)
  });
}


TextMCoordRange::TextMCoordRange(GDValueParser const &p)
:
  m_start(p.tupleGetValueAt(0)),
  m_end(p.tupleGetValueAt(1))
{
  p.checkTaggedTupleSize("MCR", 2);
}


// EOF
