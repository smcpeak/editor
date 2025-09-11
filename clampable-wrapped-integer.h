// clampable-wrapped-integer.h
// `ClampableWrappedInteger` CRTP mixin class.

// See license.txt for copyright and terms of use.

#ifndef EDITOR_CLAMPABLE_WRAPPED_INTEGER_H
#define EDITOR_CLAMPABLE_WRAPPED_INTEGER_H

#include "clampable-wrapped-integer-iface.h"     // interface decls for this module

#include "smbase/overflow.h"                     // addWithOverflowCheck


template <typename UnderInt, typename Derived, typename Difference>
void ClampableWrappedInteger<UnderInt, Derived, Difference>::clampLower(
  Difference lowerBound)
{
  if (derivedC().get() < lowerBound.get()) {
    derived().set(lowerBound.get());
  }
}


template <typename UnderInt, typename Derived, typename Difference>
void ClampableWrappedInteger<UnderInt, Derived, Difference>::clampIncrease(
  Difference delta, Difference limit)
{
  UnderInt newValue =
    addWithOverflowCheck<UnderInt>(derivedC().get(), delta.get());
  if (newValue >= limit.get()) {
    derived().set(newValue);
  }
  else {
    derived().set(limit.get());
  }
}


template <typename UnderInt, typename Derived, typename Difference>
void ClampableWrappedInteger<UnderInt, Derived, Difference>::clampIncrease(
  Difference delta)
{
  derived().clampIncrease(delta, Difference(0));
}


template <typename UnderInt, typename Derived, typename Difference>
Derived ClampableWrappedInteger<UnderInt, Derived, Difference>::clampIncreased(
  Difference delta, Difference limit) const
{
  Derived ret(derivedC());
  ret.clampIncrease(delta, limit);
  return ret;
}


template <typename UnderInt, typename Derived, typename Difference>
Derived ClampableWrappedInteger<UnderInt, Derived, Difference>::clampIncreased(
  Difference delta) const
{
  return derivedC().clampIncreased(delta, Difference(0));
}


#endif // EDITOR_CLAMPABLE_WRAPPED_INTEGER_H
