// subbable-wrapped-integer-iface.h
// Interface decls for `subbable-wrapped-integer`.

// See license.txt for copyright and terms of use.

#ifndef EDITOR_SUBBABLE_WRAPPED_INTEGER_IFACE_H
#define EDITOR_SUBBABLE_WRAPPED_INTEGER_IFACE_H

#include "subbable-wrapped-integer-fwd.h"        // fwds for this module


// Similar to `AddableWrappedInteger`, but for subtraction.
template <typename UnderInt, typename Derived, typename Difference>
class SubbableWrappedInteger {
protected:   // methods
  // Access the derived class to allow static overriding.
  Derived const &derivedC() const
    { return *static_cast<Derived const *>(this); }
  Derived &derived()
    { return *static_cast<Derived *>(this); }

public:      // methods
  // Subtraction of `Derived` objects widens to the difference type.
  Difference operator-() const;
  Difference operator-(Derived other) const;

  // Or, `Derived` - `Difference` yields `Derived`.
  //
  // This requires that the difference be representable as a `Derived`.
  Derived operator-(Difference delta) const;
  Derived &operator-=(Difference delta);
};


#endif // EDITOR_SUBBABLE_WRAPPED_INTEGER_IFACE_H
