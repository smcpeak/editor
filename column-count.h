// column-count.h
// `ColumnCount`, a non-negative `ColumnDifference`.

// See license.txt for copyright and terms of use.

#ifndef EDITOR_COLUMN_COUNT_H
#define EDITOR_COLUMN_COUNT_H

#include "column-count-fwd.h"                    // fwds for this module

#include "addable-wrapped-integer-iface.h"       // AddableWrappedInteger
#include "clampable-integer-iface.h"             // ClampableInteger
#include "column-difference-fwd.h"               // ColumnDifference [n]
#include "column-index-fwd.h"                    // ColumnIndex [n]
#include "wrapped-integer-iface.h"               // WrappedInteger

#include "smbase/compare-util-iface.h"           // DECLARE_COMPARETO_AND_DEFINE_RELATIONALS_TO_OTHER


// A non-negative `ColumnDifference`.
class ColumnCount final
  : public WrappedInteger<int, ColumnCount>,
    public AddableWrappedInteger<int, ColumnCount, ColumnDifference>,
    public ClampableInteger<int, ColumnCount, ColumnDifference> {

public:      // types
  using Base = WrappedInteger<int, ColumnCount>;
  friend Base;

  using Addable = AddableWrappedInteger<int, ColumnCount, ColumnDifference>;

protected:   // methods
  static bool isValid(int value)
    { return value >= 0; }

  static char const *getTypeName()
    { return "ColumnCount"; }

public:      // methods
  // Inherit ctors.
  using Base::Base;

  // --------------------------- Conversion ----------------------------
  // Explicit "down" conversion.
  //
  // Requires: value >= 0
  explicit ColumnCount(ColumnDifference value);

  // Implicit "up" conversion.
  operator ColumnDifference() const;

  // ---------------------------- Addition -----------------------------
  using Base::operator+;
  using Base::operator+=;
  using Addable::operator+;
  using Addable::operator+=;

  // count+index yields index.  Defining this resolves an ambiguity.
  ColumnIndex operator+(ColumnIndex delta) const;

  // ---------------------- Subtraction/inversion ----------------------
  // Inversion widens to the difference type.
  ColumnDifference operator-() const;

  // Subtraction of counts also widens.
  ColumnDifference operator-(ColumnCount delta) const;
  ColumnDifference operator-(ColumnIndex delta) const;

  // Requires: *this - delta >= 0
  ColumnCount operator-(ColumnDifference delta) const;
  ColumnCount &operator-=(ColumnDifference delta);
};


#endif // EDITOR_COLUMN_COUNT_H
