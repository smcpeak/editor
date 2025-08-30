// line-count.cc
// Code for `line-count` module.

#include "line-count.h"                // this module

#include "wrapped-integer.h"           // WrappedInteger method defns

#include "smbase/compare-util.h"       // smbase::compare, COMPARE_MEMBERS
#include "smbase/xassert.h"            // xassert

using namespace smbase;


// Explicitly instantiate the base class methods.
template class WrappedInteger<int, LineCount>;


// --------------------------- Binary tests ----------------------------
int LineCount::compareTo(LineDifference const &b) const
{
  return compare(m_value, b.get());
}


// ----------------------------- Addition ------------------------------
LineCount LineCount::operator+(int delta) const
{
  return LineCount(get() + delta);
}


LineCount &LineCount::operator+=(int delta)
{
  m_value += delta;
  selfCheck();
  return *this;
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


// EOF
