// column-difference.h
// `ColumnDifference`, to represent a difference between two layout
// column indices or numbers.

// See license.txt for copyright and terms of use.

#ifndef EDITOR_COLUMN_DIFFERENCE_H
#define EDITOR_COLUMN_DIFFERENCE_H

#include "column-difference-fwd.h"     // fwds for this module

#include "wrapped-integer-iface.h"     // WrappedInteger


/* Difference between two layout column indices or counts.  Can be
   positive, negative, or zero.

   Note that layout columns are quite different from the byte indices
   used in the model coordinate system.  See comments in `textlcoord.h`.

   See doc/line-measures.txt for discussion of the logical hierarchy of
   line measures.  This class is meant to be the root of a similar
   hierarchy of layout column measures.
*/
class ColumnDifference final : public WrappedInteger<int, ColumnDifference> {
public:      // types
  using Base = WrappedInteger<int, ColumnDifference>;
  friend Base;

protected:   // methods
  static bool isValid(int value)
    { return true; }

  static char const *getTypeName()
    { return "ColumnDifference"; }

public:      // methods
  // Inherit ctors.
  using Base::Base;

  // Modify `*this` so it is no smaller than `lowerBound`.
  //
  // TODO: Add this to `WrappedInteger`.
  void clampLower(ColumnDifference lowerBound);
};


// Provide `int*ColumnDifference` (and hence multiplication with the
// other Column types too).  `WrappedInteger` already provides the
// opposite order.
int operator*(int n, ColumnDifference delta);


// Needed for compatibility with `astgen`.
std::string toString(ColumnDifference delta);


#endif // EDITOR_COLUMN_DIFFERENCE_H
