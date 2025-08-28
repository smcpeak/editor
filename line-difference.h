// line-difference.h
// `LineDifference`, to represent a difference between two line indices
// or line numbers.

// See license.txt for copyright and terms of use.

#ifndef EDITOR_LINE_DIFFERENCE_H
#define EDITOR_LINE_DIFFERENCE_H

#include "line-difference-fwd.h"       // fwds for this module

#include "wrapped-integer-iface.h"     // WrappedInteger

#include "smbase/std-string-fwd.h"     // std::string


// Represent a difference between two line indices or line numbers.  Can
// be negative or positive (or zero).
class LineDifference final : public WrappedInteger<LineDifference> {
public:      // types
  using Base = WrappedInteger<LineDifference>;
  friend Base;

protected:   // methods
  static bool isValid(int value)
    { return true; }

  static char const *getTypeName()
    { return "LineDifference"; }

public:      // methods
  // Inherit ctors.
  using Base::Base;

  // Also explicitly expose these operators, since they would be
  // otherwise hidden by the additional overloads below.
  using Base::operator+;
  using Base::operator+=;
  using Base::operator-;
  using Base::operator-=;

  // `WrappedInteger` does not provide these, but for this class at
  // least, they should be safe.
  LineDifference operator+(int delta) const;
  LineDifference &operator+=(int delta);
  LineDifference operator-(int delta) const;
  LineDifference &operator-=(int delta);

  // If `m_value < value`, set it equal to `value`, such that `value`
  // acts as a lower bound to be clamped to.
  void clampLower(int value);
};


// Return the string that `obj.write` writes.  This is needed by the
// `astgen` infrastructure.
std::string toString(LineDifference const &obj);


#endif // EDITOR_LINE_DIFFERENCE_H
