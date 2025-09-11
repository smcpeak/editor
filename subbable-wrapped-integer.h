// subbable-wrapped-integer.h
// `SubbableWrappedInteger` CRTP mixin class.

// See license.txt for copyright and terms of use.

#ifndef EDITOR_SUBBABLE_WRAPPED_INTEGER_H
#define EDITOR_SUBBABLE_WRAPPED_INTEGER_H

#include "subbable-wrapped-integer-iface.h"      // iface decls for this module

#include "smbase/overflow.h"                     // subtractWithOverflowCheck


template <typename UnderInt, typename Derived, typename Difference>
Difference SubbableWrappedInteger<UnderInt, Derived, Difference>::operator-() const
{
  return Difference(subtractWithOverflowCheck<UnderInt>(
    0, derivedC().get() ));
}


template <typename UnderInt, typename Derived, typename Difference>
Difference SubbableWrappedInteger<UnderInt, Derived, Difference>::operator-(
  Derived other) const
{
  return Difference(subtractWithOverflowCheck<UnderInt>(
    derivedC().get(), other.get() ));
}


template <typename UnderInt, typename Derived, typename Difference>
Derived SubbableWrappedInteger<UnderInt, Derived, Difference>::operator-(
  Difference delta) const
{
  return Derived(subtractWithOverflowCheck<UnderInt>(
    derivedC().get(), delta.get() ));
}


template <typename UnderInt, typename Derived, typename Difference>
Derived &SubbableWrappedInteger<UnderInt, Derived, Difference>::operator-=(
  Difference delta)
{
  return derived() = derivedC() - delta;
}


#endif // EDITOR_SUBBABLE_WRAPPED_INTEGER_H
