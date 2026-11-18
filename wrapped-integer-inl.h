// wrapped-integer-inl.h
// Inline operations for `wrapped-integer`.

/*
  This flie is an experiment: I have (for the moment) one method of
  WrappedInteger that is sufficiently performance-sensitive that I want
  to inline it.  But I don't want every module that uses that one method
  to incur the entire dependency set of `wrapped-integer.h`.

  Therefore I have put that one method definition into its own file,
  which I can then include where needed without getting everything else.

  That said, even just `smbase/overflow.h` is heavier than I would like
  in this context...
*/

// See license.txt for copyright and terms of use.

#ifndef EDITOR_WRAPPED_INTEGER_INL_H
#define EDITOR_WRAPPED_INTEGER_INL_H

#include "wrapped-integer-iface.h"     // interface decls for this module

#include "smbase/overflow.h"           // addWithOverflowCheck


// ----------------------------- Addition ------------------------------
template <typename UnderInt, typename Derived>
Derived &WrappedInteger<UnderInt, Derived>::operator++()
{
  derived().set(addWithOverflowCheck<UnderInt>(derivedC().get(), 1));
  return derived();
}


#endif // EDITOR_WRAPPED_INTEGER_INL_H
