// clampable-integer.h
// `ClampableInteger` CRTP mixin class.

// See license.txt for copyright and terms of use.

#ifndef EDITOR_CLAMPABLE_INTEGER_H
#define EDITOR_CLAMPABLE_INTEGER_H

#include "clampable-integer-iface.h"   // interface decls for this module


template <typename Derived, typename LimitType>
void ClampableInteger<Derived, LimitType>::clampLower(LimitType lowerBound)
{
  if (derived().get() < lowerBound.get()) {
    derived().set(lowerBound.get());
  }
}


#endif // EDITOR_CLAMPABLE_INTEGER_H
