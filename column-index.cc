// column-index.cc
// Code for `column-index` module.

#include "column-index.h"              // this module

#include "addable-wrapped-integer.h"   // AddableWrappedInteger method defns
#include "clampable-integer.h"         // ClampableInteger method defns
#include "column-count.h"              // ColumnCount
#include "column-difference.h"         // ColumnDifference
#include "wrapped-integer.h"           // WrappedInteger method defns

#include "smbase/compare-util-iface.h" // smbase::compare
#include "smbase/overflow.h"           // addWithOverflowCheck, subtractWithOverflowCheck
#include "smbase/xassert.h"            // xassertPrecondition

using namespace smbase;


template class WrappedInteger<int, ColumnIndex>;
template class AddableWrappedInteger<int, ColumnIndex, ColumnDifference>;
template class ClampableInteger<int, ColumnIndex, ColumnDifference>;


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
ColumnDifference ColumnIndex::operator-() const
{
  return ColumnDifference(- get());
}


ColumnDifference ColumnIndex::operator-(ColumnIndex index) const
{
  return ColumnDifference(subtractWithOverflowCheck(get(), index.get()));
}


ColumnDifference ColumnIndex::operator-(ColumnCount count) const
{
  return ColumnDifference(subtractWithOverflowCheck(get(), count.get()));
}


ColumnIndex ColumnIndex::operator-(ColumnDifference b) const
{
  return ColumnIndex(subtractWithOverflowCheck(get(), b.get()));
}


ColumnIndex &ColumnIndex::operator-=(ColumnDifference delta)
{
  return *this = *this - delta;
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
