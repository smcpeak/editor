// positive-line-count.h
// `LineCount`, a non-negative `LineDifference`.

// See license.txt for copyright and terms of use.

#ifndef EDITOR_POSITIVE_LINE_COUNT_H
#define EDITOR_POSITIVE_LINE_COUNT_H

#include "positive-line-count-fwd.h"   // fwds for this module

#include "line-count.h"                // LineCount
#include "line-difference.h"           // LineDifference
#include "wrapped-integer-iface.h"     // WrappedInteger

#include "smbase/compare-util-iface.h" // DECLARE_COMPARETO_AND_DEFINE_RELATIONALS, DECLARE_COMPARETO_AND_DEFINE_RELATIONALS_TO_OTHER
#include "smbase/gdvalue-parser-fwd.h" // gdv::GDValueParser [n]


// A positive `LineCount`.
class PositiveLineCount final : public WrappedInteger<int, PositiveLineCount> {
public:      // types
  using Base = WrappedInteger<int, PositiveLineCount>;
  friend Base;

protected:   // methods
  static bool isValid(int value)
    { return value > 0; }

  static char const *getTypeName()
    { return "PositiveLineCount"; }

public:      // methods
  // We do not inherit the ctors because we want to require explicit
  // initialization.  Default-initialization would normally produce a
  // zero value, but that is not allowed here.  I could of course
  // default-initialize to 1, but I think that would be confusing.

  // Requires: value > 0
  explicit PositiveLineCount(int value)
    : Base(value)
  {}

  PositiveLineCount(PositiveLineCount const &obj)
    : Base(obj)
  {}

  // Requires: value > 0
  explicit PositiveLineCount(LineDifference value)
    : Base(value.get())
  {}

  // Requires: value > 0
  explicit PositiveLineCount(LineCount value)
    : Base(value.get())
  {}

  // Since a `PositiveLineCount` is logically just a restricted
  // `LineCount`, allow it to be implicitly converted.
  operator LineCount() const
    { return LineCount(m_value); }

  // Also allow conversion to the even more general class.  This is how
  // comparison of `LineIndex` and `PositiveLineCount` works.
  operator LineDifference() const
    { return LineDifference(m_value); }

  // --------------------------- Unary tests ---------------------------
  // For `PositiveLineCount`, `operator bool()` can only return true, so
  // delete it instead so an attempt to test it will yield a compilation
  // error.
  explicit operator bool() const = delete;

  // -------------------------- Binary tests ---------------------------
  // Keep the default comparisons.
  using Base::compareTo;

  // Allow comparing a count to a difference.
  DECLARE_COMPARETO_AND_DEFINE_RELATIONALS_TO_OTHER(
    PositiveLineCount, LineDifference);

  // And to a non-negative count.
  DECLARE_COMPARETO_AND_DEFINE_RELATIONALS_TO_OTHER(
    PositiveLineCount, LineCount);

  // ---------------------------- Addition -----------------------------
  // Inherit all addition operators.
  using Base::operator+;
  using Base::operator+=;

  // Requires: *this + delta > 0
  PositiveLineCount operator+(int delta) const;

  // Requires: *this + delta > 0
  PositiveLineCount &operator+=(int delta);

  // Always safe.
  PositiveLineCount &operator+=(LineCount delta);

  // ---------------------- Subtraction/inversion ----------------------
  // Inversion widens to the difference type.
  LineDifference operator-() const;
  LineDifference operator-(PositiveLineCount delta) const;
  LineDifference operator-(int delta) const;

  // Requires: *this > delta
  PositiveLineCount &operator-=(LineDifference delta);

  // The predecessor yields a `LineCount`, which is always safe (for one
  // step).
  LineCount pred() const;

  // Predecessor as a `PositiveLineCount`.
  //
  // Requires: *this > 1
  PositiveLineCount predPLC() const;

  // -------------------------- Serialization --------------------------
  // Since we didn't inherit the ctors, this has to be explicitly
  // repeated.
  explicit PositiveLineCount(gdv::GDValueParser const &p);
};


#endif // EDITOR_POSITIVE_LINE_COUNT_H
