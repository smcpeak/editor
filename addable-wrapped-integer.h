// addable-wrapped-integer.h
// `AddableWrappedInteger` CRTP mixin for `operator+`.

// See license.txt for copyright and terms of use.

#ifndef EDITOR_ADDABLE_WRAPPED_INTEGER_H
#define EDITOR_ADDABLE_WRAPPED_INTEGER_H

#include "addable-wrapped-integer-iface.h"       // iface for this module

#include "smbase/overflow.h"                     // addWithOverflowCheck


template <typename UnderInt, typename Derived, typename Difference>
Derived AddableWrappedInteger<UnderInt, Derived, Difference>::operator+(
  Difference delta) const
{
  return Derived(addWithOverflowCheck<UnderInt>(
    derivedC().get(), delta.get()));
}


template <typename UnderInt, typename Derived, typename Difference>
Derived &AddableWrappedInteger<UnderInt, Derived, Difference>::operator+=(
  Difference delta)
{
  return derived() = derivedC() + delta;
}


#endif // EDITOR_ADDABLE_WRAPPED_INTEGER_H
