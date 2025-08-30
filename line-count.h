// line-count.h
// `LineCount`, a non-negative `LineDifference`.

// See license.txt for copyright and terms of use.

#ifndef EDITOR_LINE_COUNT_H
#define EDITOR_LINE_COUNT_H

#include "line-count-fwd.h"            // fwds for this module

#include "line-difference.h"           // LineDifference
#include "wrapped-integer-iface.h"     // WrappedInteger

#include "smbase/compare-util-iface.h" // DECLARE_COMPARETO_AND_DEFINE_RELATIONALS, DECLARE_COMPARETO_AND_DEFINE_RELATIONALS_TO_OTHER


// A non-negative `LineDifference`.
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

  // Requires: value >= 0
  explicit LineCount(LineDifference value)
    : Base(value.get())
  {}

  // Since a `LineCount` is logically just a restricted
  // `LineDifference`, allow it to be implicitly converted.
  operator LineDifference() const
    { return LineDifference(m_value); }

  // -------------------------- Binary tests ---------------------------
  // Keep the default comparisons.
  using Base::compareTo;

  // Allow comparing a count to a difference.
  DECLARE_COMPARETO_AND_DEFINE_RELATIONALS_TO_OTHER(LineCount, LineDifference);

  // ---------------------------- Addition -----------------------------
  // Inherit all addition operators.
  using Base::operator+;
  using Base::operator+=;

  // Requires: *this + delta >= 0
  LineCount operator+(int delta) const;

  // Requires: *this + delta >= 0
  LineCount &operator+=(int delta);

  // ---------------------- Subtraction/inversion ----------------------
  // Inversion widens to the difference type.
  LineDifference operator-() const;
  LineDifference operator-(LineCount delta) const;
  LineDifference operator-(int delta) const;
};


#endif // EDITOR_LINE_COUNT_H
