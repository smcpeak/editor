// clampable-integer.h
// `ClampableInteger` CRTP mixin class.

// See license.txt for copyright and terms of use.

#ifndef EDITOR_CLAMPABLE_INTEGER_H
#define EDITOR_CLAMPABLE_INTEGER_H


/* This is meant to be derived from as a CRTP base.

   `Derived` has `set` and `get` methods.

   `LimitType` has a `get` method.

   These methods are assumed to produce or accept a common underlying
   integer type.

   My expectation is both are subclasses of `WrappedInteger`.
*/
template <typename Derived, typename LimitType = Derived>
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
  void clampLower(LimitType lowerBound)
  {
    if (derived().get() < lowerBound.get()) {
      derived().set(lowerBound.get());
    }
  }
};


#endif // EDITOR_CLAMPABLE_INTEGER_H
