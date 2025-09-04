// positive-line-count.cc
// Code for `positive-line-count` module.

#include "positive-line-count.h"       // this module

#include "wrapped-integer.h"           // WrappedInteger method defns

#include "smbase/compare-util.h"       // smbase::compare, COMPARE_MEMBERS
#include "smbase/overflow.h"           // {add,subtract}WithOverflowCheck
#include "smbase/xassert.h"            // xassert

using namespace smbase;


// Explicitly instantiate the base class methods.
template class WrappedInteger<int, PositiveLineCount>;


// ---------------------------- Conversion -----------------------------
PositiveLineCount::PositiveLineCount(int value)
  : Base(value)
{}


PositiveLineCount::PositiveLineCount(LineDifference delta)
  : Base(delta.get())
{}


PositiveLineCount::operator LineDifference() const
{
  return LineDifference(get());
}


PositiveLineCount::operator LineCount() const
{
  return LineCount(get());
}


// ----------------------------- Addition ------------------------------
PositiveLineCount PositiveLineCount::operator+(LineDifference delta) const
{
  return PositiveLineCount(addWithOverflowCheck(get(), delta.get()));
}


PositiveLineCount &PositiveLineCount::operator+=(LineDifference delta)
{
  set(addWithOverflowCheck(get(), delta.get()));
  return *this;
}


// ----------------------- Subtraction/inversion -----------------------
LineDifference PositiveLineCount::operator-() const
{
  return LineDifference(-get());
}



LineDifference PositiveLineCount::operator-(LineCount delta) const
{
  return LineDifference(subtractWithOverflowCheck(get(), delta.get()));
}


LineDifference PositiveLineCount::operator-(PositiveLineCount delta) const
{
  return *this - delta.operator LineCount();
}


PositiveLineCount PositiveLineCount::operator-(LineDifference delta) const
{
  return PositiveLineCount(subtractWithOverflowCheck(get(), delta.get()));
}


PositiveLineCount &PositiveLineCount::operator-=(LineDifference delta)
{
  set(subtractWithOverflowCheck(get(), delta.get()));
  return *this;
}


LineCount PositiveLineCount::pred() const
{
  return LineCount(get() - 1);
}


PositiveLineCount PositiveLineCount::predPLC() const
{
  return PositiveLineCount(get() - 1);
}


// --------------------------- Serialization ---------------------------
PositiveLineCount::PositiveLineCount(gdv::GDValueParser const &p)
  : Base(p)
{}


// EOF
