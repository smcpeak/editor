// line-index.h
// `LineIndex`, to represent a 0-based index into an array of lines.

// See license.txt for copyright and terms of use.

#ifndef EDITOR_LINE_INDEX_H
#define EDITOR_LINE_INDEX_H

#include "line-index-fwd.h"            // fwds for this module

#include "line-count-fwd.h"            // LineCount [n]
#include "line-difference-fwd.h"       // LineDifference [n]
#include "wrapped-integer-iface.h"     // WrappedInteger

#include "smbase/compare-util-iface.h" // DECLARE_COMPARETO_AND_DEFINE_RELATIONALS, DECLARE_COMPARETO_AND_DEFINE_RELATIONALS_TO_OTHER
#include "smbase/sm-macros.h"          // IMEMBFP, CMEMB, DMEMB
#include "smbase/xassert.h"            // xassertPrecondition


/* 0-based index into an array of lines, generally used in internal data
   structures.

   This type exists, among other reasons, to prevent confusion with
   `LineNumber`, the 1-based variation generally used in user
   interfaces.
*/
class LineIndex final : public WrappedInteger<LineIndex> {
public:      // types
  using Base = WrappedInteger<LineIndex>;
  friend Base;

protected:   // methods
  static bool isValid(int value)
    { return value >= 0; }

  static char const *getTypeName()
    { return "LineIndex"; }

public:      // methods
  // Inherit ctors.
  using Base::Base;

  // Conversion from `LineCount` is safe.
  explicit LineIndex(LineCount value);

  // I'm using this as a sort of marker, for places where for the moment
  // I need `get()`, but my intention is to change the interface of the
  // thing receiving the `int` to the `get()` call is not needed.
  int getForNow() const
    { return get(); }

  // -------------------------- Binary tests ---------------------------
  using Base::compareTo;

  // Allow comparison with `LineDifference`, primarily so the latter can
  // act as a loop bound.
  DECLARE_COMPARETO_AND_DEFINE_RELATIONALS_TO_OTHER(LineIndex, LineDifference);

  // ---------------------------- Addition -----------------------------
  using Base::operator+;
  using Base::operator+=;

  // Requires: `m_value+delta >= 0`, and the sum is representable.
  LineIndex operator+(LineDifference delta) const;

  LineIndex &operator+=(LineDifference delta);

  // If `*this += delta` is valid, do it and return true.  Otherwise
  // return false.
  bool tryIncrease(LineDifference delta);

  // Nominally `m_value += delta`.  If the result would be less than
  // `limit, set `*this` to `limit`.  Also the addition must not
  // overflow.
  void clampIncrease(
    LineDifference delta, LineIndex limit = LineIndex(0));

  // Like `clampIncrease`, but returning a new object.
  LineIndex clampIncreased(
    LineDifference delta, LineIndex limit = LineIndex(0)) const;

  // ---------------------- Subtraction/inversion ----------------------
  // Don't inherit `operator-` or `operator-=`.

  // Subtracting two indices yields a difference.
  LineDifference operator-(LineIndex b) const;

  // index-difference yields index.
  //
  // Requires: *this >= delta
  LineIndex operator-(LineDifference delta) const;
  LineIndex &operator-=(LineDifference delta);

  // Return `clampIncreased(-1)`.
  LineIndex predClamped() const;
};


#endif // EDITOR_LINE_INDEX_H
