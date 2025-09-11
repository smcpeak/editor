// line-index.cc
// Code for `line-index` module.

#include "line-index.h"                // this module

#include "clampable-wrapped-integer.h" // ClampableInteger method defns
#include "line-count.h"                // LineCount
#include "line-difference.h"           // LineDifference
#include "line-number.h"               // LineNumber
#include "positive-line-count.h"       // PositiveLineCount
#include "wrapped-integer.h"           // WrappedInteger method defns

#include "smbase/compare-util-iface.h" // smbase::compare
#include "smbase/overflow.h"           // addWithOverflowCheck, subtractWithOverflowCheck

using namespace smbase;


// Instantiate base class methods.
template class WrappedInteger<int, LineIndex>;
template class ClampableInteger<int, LineIndex, LineDifference>;


// ---------------------------- Conversion -----------------------------
LineIndex::LineIndex(LineDifference delta)
  : Base(delta.get())
{}


LineIndex::LineIndex(LineCount count)
  : Base(count.get())
{}


LineIndex::LineIndex(PositiveLineCount count)
  : Base(count.get())
{}


LineIndex::operator LineDifference() const
{
  return LineDifference(get());
}


LineIndex::operator LineCount() const
{
  return LineCount(get());
}


LineNumber LineIndex::toLineNumber() const
{
  return LineNumber(addWithOverflowCheck(get(), 1));
}


// --------------------------- Binary tests ----------------------------
int LineIndex::compareTo(PositiveLineCount const &b) const
{
  auto const &a = *this;
  return compare(a.get(), b.get());
}


// ----------------------------- Addition ------------------------------
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


// ---------------------------- Subtraction ----------------------------
LineDifference LineIndex::operator-() const
{
  return LineDifference(- get());
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
