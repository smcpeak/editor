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


/* A positive `LineCount`.

   See doc/line-measures.txt for more on how this class relates to
   others it is semantically related to.
*/
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

  PositiveLineCount(PositiveLineCount const &obj)
    : Base(obj)
  {}

  // --------------------------- Conversion ----------------------------
  // Explicit "down" conversions.
  //
  // Requires: value > 0
  explicit PositiveLineCount(int value);
  explicit PositiveLineCount(LineDifference value);

  // Implicit "up" conversions.
  operator LineDifference() const;
  operator LineCount() const;

  // --------------------------- Unary tests ---------------------------
  // For `PositiveLineCount`, `operator bool()` can only return true, so
  // delete it instead so an attempt to test it will yield a compilation
  // error.
  explicit operator bool() const = delete;

  // ---------------------------- Addition -----------------------------
  // Inherit addition operators on `PositiveLineCount`.
  using Base::operator+;
  using Base::operator+=;

  // Requires: *this + delta > 0
  PositiveLineCount operator+(LineDifference delta) const;
  PositiveLineCount &operator+=(LineDifference delta);

  // ---------------------- Subtraction/inversion ----------------------
  // Inversion widens to the difference type.
  LineDifference operator-() const;
  LineDifference operator-(LineCount delta) const;
  LineDifference operator-(PositiveLineCount delta) const;

  // Requires: *this > delta
  PositiveLineCount operator-(LineDifference delta) const;
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
