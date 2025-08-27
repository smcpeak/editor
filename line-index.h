// line-index.h
// `LineIndex`, to represent a 0-based index into an array of lines.

// See license.txt for copyright and terms of use.

#ifndef EDITOR_LINE_INDEX_H
#define EDITOR_LINE_INDEX_H

#include "line-index-fwd.h"            // fwds for this module

#include "line-count-fwd.h"            // LineCount [n]
#include "line-difference-fwd.h"       // LineDifference [n]

#include "smbase/compare-util-iface.h" // DECLARE_COMPARETO_AND_DEFINE_RELATIONALS
#include "smbase/gdvalue-fwd.h"        // gdv::GDValue [n]
#include "smbase/gdvalue-parser-fwd.h" // gdv::GDValueParser [n]
#include "smbase/sm-macros.h"          // IMEMBFP, CMEMB, DMEMB
#include "smbase/xassert.h"            // xassertPrecondition

#include <iosfwd>                      // std::ostream


/* 0-based index into an array of lines, generally used in internal data
   structures.

   This type exists to prevent confusion with `LineNumber`, the 1-based
   variation generally used in user interfaces.

   TODO: This class is an example of a family of classes that works like
   an integer but is not implicitly convertible to other integers and
   has a constraint on the value.  `smbase::DistinctNumber` does the
   former but not the latter.  I'd like to find a way to generalize this
   fully so this can just be a template specialization.
*/
class LineIndex final {
private:     // data
  // The index.  Non-negative.
  int m_value;

public:      // methods
  // Requires: value >= 0
  explicit LineIndex(int value = 0)
    : m_value(value)
  {
    selfCheck();
  }

  explicit LineIndex(LineCount value);

  LineIndex(LineIndex const &obj)
    : DMEMB(m_value)
  {
    selfCheck();
  }

  LineIndex &operator=(LineIndex const &obj)
  {
    if (this != &obj) {
      CMEMB(m_value);
      selfCheck();
    }
    return *this;
  }

  // Assert invariants.
  void selfCheck() const;

  bool isZero() const
    { return m_value == 0; }

  // Opposite of `isZero()`.
  bool isPositive() const
    { return m_value > 0; }

  // Ensures: return >= 0
  int get() const
    { return m_value; }

  // I'm using this as a sort of marker, for places where for the moment
  // I need `get()`, but my intention is to change the interface of the
  // thing receiving the `int` to the `get()` call is not needed.
  int getForNow() const
    { return get(); }

  // Compare in the usual order for integers.
  DECLARE_COMPARETO_AND_DEFINE_RELATIONALS(LineIndex);

  // I want to be able to use `LineIndex` as a `for` loop variable where
  // the upper bound is a line count, not a line index.
  //
  // TODO: Idea: I could define a class `LineCount` and more freely
  // permit arithmetic and comparison with it.
  bool operator<(int i) const
    { return m_value < i; }
  bool operator<=(int i) const
    { return m_value <= i; }

  bool operator<(LineDifference i) const;
  bool operator<=(LineDifference i) const;

  // Requires: m_value is not the max representable.
  LineIndex &operator++();

  // Requires: isPositive()
  LineIndex &operator--();

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

  // Return `clampIncreased(+1)`.
  LineIndex succ() const;

  // Return `clampIncreased(-1)`.
  LineIndex pred() const;

  // Like `pred`, but first assert we are Not Zero.
  LineIndex nzpred() const;

  LineDifference operator-(LineIndex b) const;
  LineIndex operator-(LineDifference b) const;
  LineIndex &operator-=(LineDifference delta);

  // Returns a GDV integer.
  operator gdv::GDValue() const;

  // Expects an integer, throws `XGDValueError` if it is negative or
  // too large to represent.
  explicit LineIndex(gdv::GDValueParser const &p);

  // Write using `os << m_value`.
  void write(std::ostream &os) const;

  friend std::ostream &operator<<(std::ostream &os,
                                  LineIndex const &obj)
  {
    obj.write(os);
    return os;
  }
};


#endif // EDITOR_LINE_INDEX_H
