// line-count.cc
// Code for `line-count` module.

#include "line-count.h"                // this module

#include "smbase/compare-util.h"       // smbase::compare, COMPARE_MEMBERS
#include "smbase/gdvalue-parser.h"     // gdv::GDValueParser
#include "smbase/gdvalue.h"            // gdv::GDValue
#include "smbase/xassert.h"            // xassert

#include <iostream>                    // std::ostream
#include <optional>                    // std::optional

using namespace gdv;
using namespace smbase;


void LineCount::selfCheck() const
{
  xassert(m_value >= 0);
}


// --------------------------- Binary tests ----------------------------
int LineCount::compareTo(LineCount const &b) const
{
  auto const &a = *this;
  return COMPARE_MEMBERS(m_value);
}


int LineCount::compareTo(int const &b) const
{
  return compare(m_value, b);
}


int LineCount::compareTo(LineDifference const &b) const
{
  return compare(m_value, b.get());
}


// ----------------------------- Addition ------------------------------
LineCount LineCount::operator+() const
{
  return *this;
}


LineCount LineCount::operator+(LineCount delta) const
{
  return LineCount(get() + delta.get());
}


LineCount LineCount::operator+(int delta) const
{
  return LineCount(get() + delta);
}


LineCount &LineCount::operator+=(LineCount delta)
{
  m_value += delta.get();
  selfCheck();
  return *this;
}


LineCount &LineCount::operator+=(int delta)
{
  m_value += delta;
  selfCheck();
  return *this;
}


LineCount &LineCount::operator++()
{
  ++m_value;
  return *this;
}


LineCount LineCount::operator++(int)
{
  LineCount ret(*this);
  ++m_value;
  return ret;
}


// ----------------------- Subtraction/inversion -----------------------
LineDifference LineCount::operator-() const
{
  return LineDifference(-get());
}


LineDifference LineCount::operator-(LineCount delta) const
{
  return LineDifference(get() - delta.get());
}


LineDifference LineCount::operator-(int delta) const
{
  return LineDifference(get() - delta);
}


LineCount &LineCount::operator-=(LineCount delta)
{
  set(*this - delta);
  return *this;
}


LineCount &LineCount::operator--()
{
  xassert(isPositive());
  --m_value;
  return *this;
}


LineCount LineCount::operator--(int)
{
  xassert(isPositive());
  LineCount ret(*this);
  --m_value;
  return ret;
}


LineCount LineCount::nzpred() const
{
  xassert(isPositive());
  return LineCount(get() - 1);
}


// --------------------------- Serialization ---------------------------
LineCount::operator gdv::GDValue() const
{
  return GDValue(get());
}


LineCount::LineCount(gdv::GDValueParser const &p)
  : m_value(0)
{
  p.checkIsInteger();

  GDVInteger v = p.integerGet();

  if (std::optional<int> i = v.getAsOpt<int>()) {
    if (*i >= 0) {
      m_value = *i;
      selfCheck();
    }
    else {
      p.throwError(stringb("LineCount value is negative: " << *i << "."));
    }
  }
  else {
    p.throwError(stringb("LineCount value out of range: " << v << "."));
  }
}


void LineCount::write(std::ostream &os) const
{
  os << m_value;
}


// EOF
