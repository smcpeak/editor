// column-count.cc
// Code for `column-count` module.

#include "column-count.h"              // this module

#include "addable-wrapped-integer.h"   // AddableWrappedInteger method defns
#include "clampable-wrapped-integer.h" // ClampableWrappedInteger method defns
#include "column-difference.h"         // ColumnDifference
#include "column-index.h"              // ColumnIndex
#include "subbable-wrapped-integer.h"  // SubbableWrappedInteger method defns
#include "wrapped-integer.h"           // WrappedInteger method defns

#include "smbase/compare-util.h"       // smbase::compare, COMPARE_MEMBERS
#include "smbase/overflow.h"           // {add,subtract}WithOverflowCheck
#include "smbase/xassert.h"            // xassert

using namespace smbase;


// Explicitly instantiate the base class methods.
template class WrappedInteger<int, ColumnCount>;
template class AddableWrappedInteger<int, ColumnCount, ColumnDifference>;
template class SubbableWrappedInteger<int, ColumnCount, ColumnDifference>;
template class ClampableWrappedInteger<int, ColumnCount, ColumnDifference>;


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
ColumnDifference ColumnCount::operator-(ColumnIndex delta) const
{
  return *this - delta.operator ColumnCount();
}


// EOF
