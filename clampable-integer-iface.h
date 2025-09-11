// clampable-integer-iface.h
// Interface decls for `clampable-integer`.

// See license.txt for copyright and terms of use.

#ifndef EDITOR_CLAMPABLE_INTEGER_IFACE_H
#define EDITOR_CLAMPABLE_INTEGER_IFACE_H

#include "clampable-integer-fwd.h"     // fwds for this module


/* This is meant to be derived from as a CRTP base.

   `UnderInt` is the underlying integer type in which calculations are
   performed.  (One might think we could get that from `Derived`, but
   the name lookup rules don't entirely cooperate.)

   `Derived` has `set` and `get` methods.

   `Difference` has a `get` method.

   The methods are assumed to produce or accept `UnderIntType`.

   My expectation is both are subclasses of `WrappedInteger`.
*/
template <typename UnderInt, typename Derived, typename Difference>
class ClampableInteger {
protected:   // methods
  // Access the derived class to allow static overriding.
  Derived const &derivedC() const
    { return *static_cast<Derived const *>(this); }
  Derived &derived()
    { return *static_cast<Derived *>(this); }

public:      // methods
  // Modify `*this` so it is no smaller than `lowerBound`.
  //
  // Note that `lowerBound` does not necessarily have a value that can
  // be represented as a `Derived`.  This can be useful when the
  // calculation that produces the limit might in some cases yield a
  // limit that cannot be represented; for such a value, the limit will
  // simply not have any effect.
  void clampLower(Difference lowerBound);

  // Nominally `m_value += delta`.  If the result would be less than
  // `limit`, set `*this` to `limit`.  Also the addition must not
  // overflow.
  void clampIncrease(Difference delta, Difference limit);

  // Same, with an implicit limit of 0.
  void clampIncrease(Difference delta);

  // Like `clampIncrease`, but returning a new object.
  Derived clampIncreased(Difference delta, Difference limit) const;
  Derived clampIncreased(Difference delta) const;
};


#endif // EDITOR_CLAMPABLE_INTEGER_IFACE_H
