// line-index.cc
// Code for `line-index` module.

#include "line-index.h"                // this module

#include "smbase/compare-util-iface.h" // COMPARE_MEMBERS
#include "smbase/gdvalue.h"            // gdv::{GDValue, GDVInteger}
#include "smbase/gdvalue-parser.h"     // gdv::GDValueParser
#include "smbase/sm-macros.h"          // DMEMB
#include "smbase/stringb.h"            // stringb

#include <iostream>                    // std::ostream
#include <optional>                    // std::optional

using namespace gdv;
using namespace smbase;


LineIndex::LineIndex(int value)
  : IMEMBFP(value)
{
  xassertPrecondition(value >= 0);
}


LineIndex::LineIndex(LineIndex const &obj)
  : DMEMB(m_value)
{}


LineIndex &LineIndex::operator=(LineIndex const &obj)
{
  if (this != &obj) {
    CMEMB(m_value);
  }
  return *this;
}


int LineIndex::compareTo(LineIndex const &b) const
{
  auto const &a = *this;
  RET_IF_COMPARE_MEMBERS(m_value);
  return 0;
}


LineIndex::operator gdv::GDValue() const
{
  return GDValue(m_value);
}


LineIndex::LineIndex(gdv::GDValueParser const &p)
  : m_value(0)
{
  p.checkIsInteger();

  GDVInteger v = p.integerGet();
  if (v.isNegative()) {
    p.throwError(stringb("LineIndex value is negative: " << v << "."));
  }

  if (std::optional<int> i = v.getAsOpt<int>()) {
    m_value = *i;
  }
  else {
    p.throwError(stringb("LineIndex value is too large: " << v << "."));
  }
}


void LineIndex::write(std::ostream &os) const
{
  os << m_value;
}


// EOF
