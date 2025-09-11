// column-index.cc
// Code for `column-index` module.

#include "column-index.h"              // this module

#include "addable-wrapped-integer.h"   // AddableWrappedInteger method defns
#include "clampable-wrapped-integer.h" // ClampableWrappedInteger method defns
#include "column-count.h"              // ColumnCount
#include "column-difference.h"         // ColumnDifference
#include "subbable-wrapped-integer.h"  // SubbableWrappedInteger method defns
#include "wrapped-integer.h"           // WrappedInteger method defns

#include "smbase/compare-util-iface.h" // smbase::compare
#include "smbase/overflow.h"           // addWithOverflowCheck, subtractWithOverflowCheck
#include "smbase/xassert.h"            // xassertPrecondition

using namespace smbase;


template class WrappedInteger<int, ColumnIndex>;
template class AddableWrappedInteger<int, ColumnIndex, ColumnDifference>;
template class SubbableWrappedInteger<int, ColumnIndex, ColumnDifference>;
template class ClampableWrappedInteger<int, ColumnIndex, ColumnDifference>;


// ---------------------------- Conversion -----------------------------
ColumnIndex::ColumnIndex(ColumnDifference delta)
  : Base(delta.get())
{}


ColumnIndex::ColumnIndex(ColumnCount count)
  : Base(count.get())
{}


ColumnIndex::operator ColumnDifference() const
{
  return ColumnDifference(get());
}


ColumnIndex::operator ColumnCount() const
{
  return ColumnCount(get());
}


int ColumnIndex::toColumnNumber() const
{
  return addWithOverflowCheck(get(), 1);
}


// ---------------------------- Subtraction ----------------------------
ColumnDifference ColumnIndex::operator-(ColumnCount count) const
{
  return ColumnDifference(subtractWithOverflowCheck(get(), count.get()));
}


// ------------------------- Other arithmetic --------------------------
// TODO: Move this to someplace more general.
static int roundUp(int n, int unit)
{
  xassertPrecondition(unit > 0);

  int bigger = addWithOverflowCheck(n, unit-1);

  return (bigger / unit) * unit;
}


ColumnIndex ColumnIndex::roundUpToMultipleOf(ColumnCount count) const
{
  return ColumnIndex(roundUp(get(), count.get()));
}


// EOF
