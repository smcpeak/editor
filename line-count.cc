// line-count.cc
// Code for `line-count` module.

#include "line-count.h"                // this module

#include "positive-line-count.h"       // PositiveLineCount
#include "wrapped-integer.h"           // WrappedInteger method defns

#include "smbase/compare-util.h"       // smbase::compare, COMPARE_MEMBERS
#include "smbase/overflow.h"           // {add,subtract}WithOverflowCheck
#include "smbase/xassert.h"            // xassert

using namespace smbase;


// Explicitly instantiate the base class methods.
template class WrappedInteger<int, LineCount>;


// ----------------------------- Addition ------------------------------
LineCount LineCount::operator+(LineDifference delta) const
{
  return LineCount(addWithOverflowCheck(get(), delta.get()));
}


LineCount &LineCount::operator+=(LineDifference delta)
{
  set(addWithOverflowCheck(get(), delta.get()));
  return *this;
}


// ----------------------- Subtraction/inversion -----------------------
LineDifference LineCount::operator-() const
{
  return LineDifference(-get());
}


LineDifference LineCount::operator-(LineCount delta) const
{
  return LineDifference(subtractWithOverflowCheck(get(), delta.get()));
}


LineDifference LineCount::operator-(PositiveLineCount delta) const
{
  return *this - delta.operator LineCount();
}


LineCount LineCount::operator-(LineDifference delta) const
{
  return LineCount(subtractWithOverflowCheck(get(), delta.get()));
}


LineCount &LineCount::operator-=(LineDifference delta)
{
  set(subtractWithOverflowCheck(get(), delta.get()));
  return *this;
}


// EOF
