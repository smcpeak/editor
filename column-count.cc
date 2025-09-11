// column-count.cc
// Code for `column-count` module.

#include "column-count.h"              // this module

#include "addable-wrapped-integer.h"   // AddableWrappedInteger method defns
#include "clampable-integer.h"         // ClampableInteger method defns
#include "column-difference.h"         // ColumnDifference
#include "column-index.h"              // ColumnIndex
#include "wrapped-integer.h"           // WrappedInteger method defns

#include "smbase/compare-util.h"       // smbase::compare, COMPARE_MEMBERS
#include "smbase/overflow.h"           // {add,subtract}WithOverflowCheck
#include "smbase/xassert.h"            // xassert

using namespace smbase;


// Explicitly instantiate the base class methods.
template class WrappedInteger<int, ColumnCount>;
template class ClampableInteger<int, ColumnCount, ColumnDifference>;
template class AddableWrappedInteger<int, ColumnCount, ColumnDifference>;


// ---------------------------- Conversion -----------------------------
ColumnCount::ColumnCount(ColumnDifference value)
  : Base(value.get())
{}


ColumnCount::operator ColumnDifference() const
{
  return ColumnDifference(get());
}


// ----------------------------- Addition ------------------------------
ColumnIndex ColumnCount::operator+(ColumnIndex delta) const
{
  return ColumnIndex(addWithOverflowCheck(get(), delta.get()));
}


// ----------------------- Subtraction/inversion -----------------------
ColumnDifference ColumnCount::operator-() const
{
  return ColumnDifference(-get());
}


ColumnDifference ColumnCount::operator-(ColumnCount delta) const
{
  return ColumnDifference(subtractWithOverflowCheck(get(), delta.get()));
}


ColumnDifference ColumnCount::operator-(ColumnIndex delta) const
{
  return *this - delta.operator ColumnCount();
}


ColumnCount ColumnCount::operator-(ColumnDifference delta) const
{
  return ColumnCount(subtractWithOverflowCheck(get(), delta.get()));
}


ColumnCount &ColumnCount::operator-=(ColumnDifference delta)
{
  set(subtractWithOverflowCheck(get(), delta.get()));
  return *this;
}


// EOF
