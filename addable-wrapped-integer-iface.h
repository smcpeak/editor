// addable-wrapped-integer-iface.h
// Interface decls for `addable-wrapped-integer`.

// See license.txt for copyright and terms of use.

#ifndef EDITOR_ADDABLE_WRAPPED_INTEGER_IFACE_H
#define EDITOR_ADDABLE_WRAPPED_INTEGER_IFACE_H

#include "addable-wrapped-integer-fwd.h"         // fwds for this module


/* This is meant to be derived from as a CRTP base.

   `UnderInt` is the underlying integer type in which calculations are
   performed.  (One might think we could get that from `Derived`, but
   the name lookup rules don't entirely cooperate.)

   `Derived` is the type we are contributing methods to.

   `Difference` is what we want to allow to be added.

   My expectation is both are subclasses of `WrappedInteger`, which
   provides addition for two `Derived` objects.  To enable both
   homogeneous and heterogeneous addition, one typically does:

     using Base = WrappedInteger<...>;
     using Addable = AddableWrappedInteger<...>;

     using Base::operator+;
     using Base::operator+=;
     using Addable::operator+;
     using Addable::operator+=;

   Otherwise none will be usable because operators defined in two
   different base classes do not get implicitly combined into a single
   overload set.
*/
template <typename UnderInt, typename Derived, typename Difference>
class AddableWrappedInteger {
protected:   // methods
  // Access the derived class to allow static overriding.
  Derived const &derivedC() const
    { return *static_cast<Derived const *>(this); }
  Derived &derived()
    { return *static_cast<Derived *>(this); }

public:      // methods
  Derived operator+(Difference delta) const;
  Derived &operator+=(Difference delta);
};


#endif // EDITOR_ADDABLE_WRAPPED_INTEGER_IFACE_H
