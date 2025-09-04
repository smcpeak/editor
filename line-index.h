// line-index.h
// `LineIndex`, to represent a 0-based index into an array of lines.

// See license.txt for copyright and terms of use.

#ifndef EDITOR_LINE_INDEX_H
#define EDITOR_LINE_INDEX_H

#include "line-index-fwd.h"            // fwds for this module

#include "line-count-fwd.h"            // LineCount [n]
#include "line-difference-fwd.h"       // LineDifference [n]
#include "line-number-fwd.h"           // LineNumber [n]
#include "positive-line-count-fwd.h"   // PositiveLineCount [n]
#include "wrapped-integer-iface.h"     // WrappedInteger

#include "smbase/compare-util-iface.h" // DECLARE_COMPARETO_AND_DEFINE_RELATIONALS_TO_OTHER


/* 0-based index into an array of lines, generally used in internal data
   structures.

   This type exists, among other reasons, to prevent confusion with
   `LineNumber`, the 1-based variation generally used in user
   interfaces.

   See doc/line-measures.txt for more on how this class relates to
   others it is semantically related to.
*/
class LineIndex final : public WrappedInteger<int, LineIndex> {
public:      // types
  using Base = WrappedInteger<int, LineIndex>;
  friend Base;

protected:   // methods
  static bool isValid(int value)
    { return value >= 0; }

  static char const *getTypeName()
    { return "LineIndex"; }

public:      // methods
  // Inherit ctors.
  using Base::Base;

  // --------------------------- Conversion ----------------------------
  // Explicit "down" conversions.
  explicit LineIndex(LineDifference delta);
  explicit LineIndex(LineCount count);

  // Explicit "cross" conversion.
  explicit LineIndex(PositiveLineCount count);

  // Implicit "up" conversions.
  operator LineDifference() const;
  operator LineCount() const;

  // Convert to line number by adding one.
  LineNumber toLineNumber() const;

  // -------------------------- Binary tests ---------------------------
  using Base::compareTo;

  // Enable "cross" comparison.  This has to be explicitly specified
  // because otherwise comparing LineIndex to PositiveLineCount is
  // ambiguous since both could first convert to either LineDifference
  // or LineCount to do the comparison, and as far as the language is
  // concerned, those are unrelated classes.
  DECLARE_COMPARETO_AND_DEFINE_RELATIONALS_TO_OTHER(LineIndex, PositiveLineCount);

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
