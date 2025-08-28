// line-index.cc
// Code for `line-index` module.

#include "line-index.h"                // this module

#include "line-count.h"                // LineCount
#include "line-difference.h"           // LineDifference

#include "smbase/compare-util-iface.h" // COMPARE_MEMBERS
#include "smbase/gdvalue.h"            // gdv::{GDValue, GDVInteger}
#include "smbase/gdvalue-parser.h"     // gdv::GDValueParser
#include "smbase/overflow.h"           // addWithOverflowCheck, preIncrementWithOverflowCheck
#include "smbase/sm-macros.h"          // DMEMB
#include "smbase/stringb.h"            // stringb

#include <iostream>                    // std::ostream
#include <limits>                      // std::numeric_limits
#include <optional>                    // std::optional

using namespace gdv;
using namespace smbase;


LineIndex::LineIndex(LineCount value)
  : m_value(value.get())
{
  selfCheck();
}


void LineIndex::selfCheck() const
{
  xassert(m_value >= 0);
}


int LineIndex::compareTo(LineIndex const &b) const
{
  auto const &a = *this;
  RET_IF_COMPARE_MEMBERS(m_value);
  return 0;
}


int LineIndex::compareTo(LineDifference const &b) const
{
  auto const &a = *this;
  return compare(a.get(), b.get());
}


LineIndex &LineIndex::operator++()
{
  preIncrementWithOverflowCheck(m_value);
  return *this;
}


LineIndex &LineIndex::operator--()
{
  xassert(isPositive());
  --m_value;
  selfCheck();
  return *this;
}


LineIndex LineIndex::operator+(LineDifference delta) const
{
  return LineIndex(addWithOverflowCheck(m_value, delta.get()));
}


LineIndex &LineIndex::operator+=(LineDifference delta)
{
  return *this = *this + delta;
}


bool LineIndex::tryIncrease(LineDifference delta)
{
  try {
    int newValue = addWithOverflowCheck(m_value, delta.get());
    if (newValue >= 0) {
      m_value = newValue;
      selfCheck();
      return true;
    }
    else {
      return false;
    }
  }
  catch (...) {
    return false;
  }
}


void LineIndex::clampIncrease(
  LineDifference delta, LineIndex limit)
{
  int newValue = addWithOverflowCheck(m_value, delta.get());
  if (newValue >= limit.get()) {
    m_value = newValue;
  }
  else {
    m_value = limit.get();
  }
}


LineIndex LineIndex::clampIncreased(
  LineDifference delta, LineIndex limit) const
{
  LineIndex ret(*this);
  ret.clampIncrease(delta, limit);
  return ret;
}


LineIndex LineIndex::succ() const
{
  return clampIncreased(LineDifference(+1));
}


LineIndex LineIndex::pred() const
{
  xassertPrecondition(isPositive());
  return predClamped();
}


LineIndex LineIndex::predClamped() const
{
  return clampIncreased(LineDifference(-1));
}


LineDifference LineIndex::operator-(LineIndex b) const
{
  // Since both are non-negative, this cannot overflow, although it can
  // of course return a negative value.
  return LineDifference(m_value - b.m_value);
}


LineIndex LineIndex::operator-(LineDifference b) const
{
  return LineIndex(m_value - b.get());
}


LineIndex &LineIndex::operator-=(LineDifference delta)
{
  return *this = *this - delta;
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
