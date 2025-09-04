// line-number.h
// `LineNumber`, a 1-based line identifier.

// See license.txt for copyright and terms of use.

#ifndef EDITOR_LINE_NUMBER_H
#define EDITOR_LINE_NUMBER_H

#include "line-number-fwd.h"           // fwds for this module

#include "line-count-fwd.h"            // LineCount [n]
#include "line-index-fwd.h"            // LineIndex [n]
#include "line-difference-fwd.h"       // LineDifference [n]
#include "positive-line-count-fwd.h"   // PositiveLineCount [n]
#include "wrapped-integer-iface.h"     // WrappedInteger

#include "smbase/compare-util-iface.h" // DECLARE_COMPARETO_AND_DEFINE_RELATIONALS_TO_OTHER
#include "smbase/gdvalue-parser-fwd.h" // gdv::GDValueParser [n]


/* 1-based line identifier, generally used for user interfaces.

   This type exists, among other reasons, to prevent confusion with
   `LineIndex`, the 0-based variation generally used for internal data.

   This class has the same `isValid` requirement as `PositiveLineCount`,
   but conceptually is different since it is meant to identify a single
   line rather than measure a distance or count.

   This class is here in the Line measure logical hierarchy:

     LineDifference
       LineCount
         LineIndex
         PositiveLineCount
     LineNumber              <---

   Line numbers are Line measures in the most general sense, but are not
   a "difference" or "count" the way the others are, so they do not
   convert or compare between the others (not even explicitly, other
   than via `get()`).  They do, however, interact arithmetically,
   allowing differences to be added and subtracted.
*/
class LineNumber final : public WrappedInteger<int, LineNumber> {
public:      // types
  using Base = WrappedInteger<int, LineNumber>;
  friend Base;

protected:   // methods
  static bool isValid(int value)
    { return value > 0; }

  static char const *getTypeName()
    { return "LineNumber"; }

public:      // methods
  // We do not inherit the ctors because we want to require explicit
  // initialization due to the positivity requirement.

  LineNumber(LineNumber const &obj)
    : Base(obj)
  {}

  // --------------------------- Conversion ----------------------------
  // Requires: value > 0
  explicit LineNumber(int value);

  // Convert a number to an index by subtracting one.
  LineIndex toLineIndex() const;

  // --------------------------- Unary tests ---------------------------
  // For `LineNumber`, `operator bool()` can only return true, so delete
  // it instead so an attempt to test it will yield a compilation error.
  explicit operator bool() const = delete;

  // ---------------------------- Addition -----------------------------
  using Base::operator+;

  // Requires: *this + delta > 0
  LineNumber operator+(LineDifference delta) const;
  LineNumber& operator+=(LineDifference delta);

  // ---------------------- Subtraction/inversion ----------------------
  // Don't inherit `operator-` or `operator-=`.

  // Subtracting two line numbers yields a difference.
  LineDifference operator-(LineNumber b) const;

  // number-difference yields number.
  //
  // Requires: *this > delta
  LineNumber operator-(LineDifference delta) const;
  LineNumber &operator-=(LineDifference delta);

  // -------------------------- Serialization --------------------------
  // Since we didn't inherit the ctors, this has to be explicitly
  // repeated.
  explicit LineNumber(gdv::GDValueParser const &p);
};


#endif // EDITOR_LINE_NUMBER_H
