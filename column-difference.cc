// column-difference.cc
// Code for `column-difference` module.

#include "column-difference.h"         // this module

#include "wrapped-integer.h"           // WrappedInteger method defns

#include "smbase/stringb.h"            // stringb


template class WrappedInteger<int, ColumnDifference>;


int operator*(int n, ColumnDifference delta)
{
  return delta * n;
}


std::string toString(ColumnDifference delta)
{
  return stringb(delta);
}


// EOF
