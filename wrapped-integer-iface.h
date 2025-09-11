// wrapped-integer-iface.h
// Interface declarations for the `wrapped-integer` module.

// See license.txt for copyright and terms of use.

// I envision eventually moving this to `smbase`, but I want to gain
// more experience with it first.

#ifndef EDITOR_WRAPPED_INTEGER_IFACE_H
#define EDITOR_WRAPPED_INTEGER_IFACE_H

#include "wrapped-integer-fwd.h"       // fwds for this module

#include "smbase/compare-util-iface.h" // DECLARE_COMPARETO_AND_DEFINE_RELATIONALS, DECLARE_COMPARETO_AND_DEFINE_RELATIONALS_TO_OTHER, smbase::compare
#include "smbase/gdvalue-fwd.h"        // gdv::GDValue [n]
#include "smbase/gdvalue-parser-fwd.h" // gdv::GDValueParser [n]
#include "smbase/sm-macros.h"          // IMEMBFP, CMEMB, DMEMB
#include "smbase/std-string-fwd.h"     // std::string

#include <iosfwd>                      // std::ostream
#include <type_traits>                 // std::{enable_if_t, is_integral_v}


/* Class template to use a common base for purpose-specific wrapped
   integer classes.  There are two aspects to a "purpose" here:

     1. Constraining the set of representable values.  That is done by
        overriding `isValid`.

     2. Constraining the set of allowed operations, and with what other
        types.  This is much more varied, but the unifying idea is to
        regard each `WrappedInteger` type as having some particular
        *units*, and then making operations consistent with dimensional
        analysis.  See `doc/line-measures.txt` for more on this.

   This is meant to be inherited using the Curiously-Recurring Template
   Pattern (CRTP) like this (see `wrapped-integer-test.cc`):

     class NonNegativeInteger : public WrappedInteger<NonNegativeInteger> {
     public:      // types
       using Base = WrappedInteger<NonNegativeInteger>;
       friend Base;

     protected:   // methods
       static bool isValid(int value)
         { return value >= 0; }

       static char const *getTypeName()
         { return "NonNegativeInteger"; }

     public:      // methods
       // Inherit ctors.
       using Base::Base;

       // Possibly additional methods or overrides here.
     };

   Thus, the `Derived` type here is that "derived" from a specialization
   of this one.

   It is necessary for the derived class to befriend its base class
   since `isValid` and `getTypeName` are protected.
*/
template <typename UnderInt, typename Derived>
class WrappedInteger {
protected:   // data
  // The wrapped value.
  UnderInt m_value;

protected:   // methods
  // Access the derived class to allow static overriding.
  Derived const &derivedC() const
    { return *static_cast<Derived const *>(this); }
  Derived &derived()
    { return *static_cast<Derived *>(this); }

  // Condition for validity of a value.  This is meant to be statically
  // overridden by `Derived`.
  static bool isValid(UnderInt value)
    { return true; }

  // Return the name of this type.  Meant to be overridden.
  static char const *getTypeName()
    { return "WrappedInteger"; }

public:      // methods
  // One of the purposes of the "wrapped integer" concept is to be a
  // distinct integer type, so conversions in and out are explicit.
  explicit WrappedInteger(UnderInt value = 0)
    : m_value(value)
  {
    derivedC().selfCheck();
  }

  WrappedInteger(WrappedInteger const &obj)
    : DMEMB(m_value)
  {
    derivedC().selfCheck();
  }

  Derived &operator=(WrappedInteger const &obj)
  {
    if (this != &obj) {
      CMEMB(m_value);
      derivedC().selfCheck();
    }
    return derived();
  }

  // Assert invariants.
  void selfCheck() const;

  UnderInt get() const
  {
    return m_value;
  }

  void set(UnderInt value)
  {
    m_value = value;
    derivedC().selfCheck();
  }

  // --------------------------- Conversion ----------------------------
  // Convert to integral `T`, confirming value preservation.
  template <typename T,
            typename = std::enable_if<std::is_integral_v<T>>>
  inline T getAs() const;

  // --------------------------- Unary tests ---------------------------
  bool isZero() const
    { return m_value == 0; }
  bool isPositive() const
    { return m_value > 0; }
  bool isNegative() const
    { return m_value < 0; }
  bool isNonZero() const
    { return !isZero(); }
  bool isNonPositive() const
    { return !isPositive(); }
  bool isNonNegative() const
    { return !isNegative(); }

  explicit operator bool() const
    { return !derivedC().isZero(); }

  // -------------------------- Binary tests ---------------------------
  // Compare in the usual order for integers.
  DECLARE_COMPARETO_AND_DEFINE_RELATIONALS(Derived);
  DECLARE_COMPARETO_AND_DEFINE_RELATIONALS_TO_OTHER(Derived, UnderInt);

  // ---------------------------- Addition -----------------------------
  Derived operator+() const;
  Derived operator+(Derived delta) const;

  // Successor, i.e., `*this + 1`.
  Derived succ() const;

  Derived &operator+=(Derived delta);

  Derived &operator++();
  Derived operator++(int);

  // ---------------------- Subtraction/inversion ----------------------
  Derived operator-() const;
  Derived operator-(Derived delta) const;

  // Predecessor, i.e., `*this - 1`.
  Derived pred() const;

  Derived &operator-=(Derived delta);

  Derived &operator--();
  Derived operator--(int);

  // ------------------------- Multiplication --------------------------
  // Multiplication by int yields int.
  //
  // Unlike addition and subtraction, which require both operands to
  // have the same dimensions, multiplication is always sensible, but we
  // have to regard the result as having unknown dimension.
  //
  // I only define it this way, rather than also allowing int*Derived,
  // so it is easy in the derived class to hide this operator if
  // desired.
  UnderInt operator*(UnderInt n) const;

  // -------------------------- Serialization --------------------------
  // Returns a GDV integer.
  operator gdv::GDValue() const;

  // Expects an integer, throws `XGDValueError` if it is negative or
  // too large to represent.
  explicit WrappedInteger(gdv::GDValueParser const &p);

  // Write using `os << m_value`.
  void write(std::ostream &os) const;

  friend std::ostream &operator<<(std::ostream &os,
                                  Derived const &obj)
  {
    obj.write(os);
    return os;
  }
};


#endif // EDITOR_WRAPPED_INTEGER_IFACE_H
