// line-index.cc
// Code for `line-index` module.

#include "line-index.h"                // this module

#include "line-count.h"                // LineCount
#include "line-difference.h"           // LineDifference
#include "line-number.h"               // LineNumber
#include "wrapped-integer.h"           // WrappedInteger method defns

#include "smbase/compare-util-iface.h" // smbase::compare
#include "smbase/overflow.h"           // addWithOverflowCheck, subtractWithOverflowCheck

using namespace smbase;


// Instantiate base class methods.
template class WrappedInteger<int, LineIndex>;


LineIndex::LineIndex(LineCount value)
  : Base(value.get())
{}


LineNumber LineIndex::toLineNumber() const
{
  return LineNumber(addWithOverflowCheck(get(), 1));
}


int LineIndex::compareTo(LineDifference const &b) const
{
  auto const &a = *this;
  return compare(a.get(), b.get());
}


LineIndex LineIndex::operator+(LineDifference delta) const
{
  return LineIndex(addWithOverflowCheck(get(), delta.get()));
}


LineIndex &LineIndex::operator+=(LineDifference delta)
{
  return *this = *this + delta;
}


bool LineIndex::tryIncrease(LineDifference delta)
{
  try {
    int newValue = addWithOverflowCheck(get(), delta.get());
    if (newValue >= 0) {
      set(newValue);
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
  int newValue = addWithOverflowCheck(get(), delta.get());
  if (newValue >= limit.get()) {
    set(newValue);
  }
  else {
    set(limit.get());
  }
}


LineIndex LineIndex::clampIncreased(
  LineDifference delta, LineIndex limit) const
{
  LineIndex ret(*this);
  ret.clampIncrease(delta, limit);
  return ret;
}


LineDifference LineIndex::operator-(LineIndex b) const
{
  // Since both are non-negative, this cannot overflow, although it can
  // of course return a negative value.
  return LineDifference(get() - b.get());
}


LineIndex LineIndex::operator-(LineDifference b) const
{
  return LineIndex(subtractWithOverflowCheck(get(), b.get()));
}


LineIndex &LineIndex::operator-=(LineDifference delta)
{
  return *this = *this - delta;
}


LineIndex LineIndex::predClamped() const
{
  return clampIncreased(LineDifference(-1));
}


// EOF
