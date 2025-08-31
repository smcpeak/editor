// line-number.cc
// Code for `line-number` module.

#include "line-number.h"               // this module

#include "line-difference.h"           // LineDifference
#include "line-index.h"                // LineIndex
#include "wrapped-integer.h"           // WrappedInteger method defns

#include "smbase/compare-util-iface.h" // smbase::compare
#include "smbase/overflow.h"           // subtractWithOverflowCheck

using namespace smbase;


// Instantiate base class methods.
template class WrappedInteger<int, LineNumber>;


LineIndex LineNumber::toLineIndex() const
{
  return LineIndex(get() - 1);
}


LineNumber LineNumber::operator+(LineDifference delta) const
{
  return LineNumber(get() + delta.get());
}


LineDifference LineNumber::operator-(LineNumber b) const
{
  // Overflow is not possible.
  return LineDifference(get() - b.get());
}


LineNumber LineNumber::operator-(LineDifference b) const
{
  return LineNumber(subtractWithOverflowCheck(get(), b.get()));
}


LineNumber &LineNumber::operator-=(LineDifference delta)
{
  return *this = *this - delta;
}


// EOF
