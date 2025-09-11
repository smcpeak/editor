// column-index.h
// `ColumnIndex`, to represent a 0-based layout column index.

// See license.txt for copyright and terms of use.

#ifndef EDITOR_COLUMN_INDEX_H
#define EDITOR_COLUMN_INDEX_H

#include "column-index-fwd.h"                    // fwds for this module

#include "addable-wrapped-integer-iface.h"       // AddableWrappedInteger
#include "clampable-integer-iface.h"             // ClampableInteger
#include "column-count-fwd.h"                    // ColumnCount [n]
#include "column-difference-fwd.h"               // ColumnDifference [n]
#include "wrapped-integer-iface.h"               // WrappedInteger

#include "smbase/compare-util-iface.h"           // DECLARE_COMPARETO_AND_DEFINE_RELATIONALS_TO_OTHER


/* 0-based column index for use in layout coordinates.

   Always non-negative.

   This is a logical subclass of `ColumnCount`.
*/
class ColumnIndex final
  : public WrappedInteger<int, ColumnIndex>,
    public AddableWrappedInteger<int, ColumnIndex, ColumnDifference>,
    public ClampableInteger<int, ColumnIndex, ColumnDifference> {

public:      // types
  using Base = WrappedInteger<int, ColumnIndex>;
  friend Base;

  using Addable = AddableWrappedInteger<int, ColumnIndex, ColumnDifference>;

protected:   // methods
  static bool isValid(int value)
    { return value >= 0; }

  static char const *getTypeName()
    { return "ColumnIndex"; }

public:      // methods
  // Inherit ctors.
  using Base::Base;

  // --------------------------- Conversion ----------------------------
  // Explicit "down" conversions.
  explicit ColumnIndex(ColumnDifference delta);
  explicit ColumnIndex(ColumnCount delta);

  // Implicit "up" conversions.
  operator ColumnDifference() const;
  operator ColumnCount() const;

  // Return a 1-based column number for `ci`.
  //
  // For the moment at least, I do not have a `ColumnNumber` type, but
  // this function will help mark where that would be used.
  int toColumnNumber() const;

  // ---------------------------- Addition -----------------------------
  using Base::operator+;
  using Base::operator+=;
  using Addable::operator+;
  using Addable::operator+=;

  // ---------------------- Subtraction/inversion ----------------------
  // Don't inherit `operator-` or `operator-=`.

  // Subtracting two indices yields a difference.
  ColumnDifference operator-() const;
  ColumnDifference operator-(ColumnIndex index) const;

  // Same for a count.  Without this overload, "index-count" is
  // converted to "index-difference", which then cannot be negative.
  ColumnDifference operator-(ColumnCount count) const;

  // index-difference yields index.
  //
  // Requires: *this >= delta
  ColumnIndex operator-(ColumnDifference delta) const;
  ColumnIndex &operator-=(ColumnDifference delta);

  // ------------------------ Other arithmetic -------------------------
  // Return `*this` rounded up to the nearest multiple of `count`.
  //
  // Requires: count > 0
  ColumnIndex roundUpToMultipleOf(ColumnCount count) const;
};


#endif // EDITOR_COLUMN_INDEX_H
