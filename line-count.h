// line-count.h
// `LineCount`, a non-negative `LineDifference`.

// See license.txt for copyright and terms of use.

#ifndef EDITOR_LINE_COUNT_H
#define EDITOR_LINE_COUNT_H

#include "line-count-fwd.h"            // fwds for this module

#include "line-difference.h"           // LineDifference
#include "positive-line-count-fwd.h"   // PositiveLineCount [n]
#include "wrapped-integer-iface.h"     // WrappedInteger

#include "smbase/compare-util-iface.h" // DECLARE_COMPARETO_AND_DEFINE_RELATIONALS_TO_OTHER


/* A non-negative `LineDifference`.

   See doc/line-measures.txt for more on how this class relates to
   others it is semantically related to.
*/
class LineCount final : public WrappedInteger<int, LineCount> {
public:      // types
  using Base = WrappedInteger<int, LineCount>;
  friend Base;

protected:   // methods
  static bool isValid(int value)
    { return value >= 0; }

  static char const *getTypeName()
    { return "LineCount"; }

public:      // methods
  // Inherit ctors.
  using Base::Base;

  // --------------------------- Conversion ----------------------------
  // Explicit "down" conversion.
  //
  // Requires: value >= 0
  explicit LineCount(LineDifference value)
    : Base(value.get())
  {}

  // Implicit "up" conversion.
  operator LineDifference() const
    { return LineDifference(get()); }

  // ---------------------------- Addition -----------------------------
  // Inherit all addition operators.
  using Base::operator+;
  using Base::operator+=;

  // Requires: *this + delta >= 0
  LineCount operator+(LineDifference delta) const;
  LineCount &operator+=(LineDifference delta);

  // ---------------------- Subtraction/inversion ----------------------
  // Inversion widens to the difference type.
  LineDifference operator-() const;

  // Subtraction of counts also widens.
  LineDifference operator-(LineCount delta) const;
  LineDifference operator-(PositiveLineCount delta) const;

  // Requires: *this - delta >= 0
  LineCount operator-(LineDifference delta) const;
  LineCount &operator-=(LineDifference delta);
};


#endif // EDITOR_LINE_COUNT_H
