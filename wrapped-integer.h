// wrapped-integer.h
// `WrappedInteger` class template to use a common base for purpose-
// specific wrapped integer classes.

// See license.txt for copyright and terms of use.

#ifndef EDITOR_WRAPPED_INTEGER_H
#define EDITOR_WRAPPED_INTEGER_H

#include "wrapped-integer-iface.h"     // interface decls for this module

#include "smbase/compare-util.h"       // smbase::compare
#include "smbase/gdvalue.h"            // gdv::GDValue [n]
#include "smbase/gdvalue-parser.h"     // gdv::GDValueParser [n]
#include "smbase/overflow.h"           // addWithOverflowCheck
#include "smbase/xassert.h"            // xassert, xfailure_stringbc

#include <iostream>                    // std::ostream
#include <optional>                    // std::optional
#include <string>                      // std::string


template <typename UnderInt, typename Derived>
void WrappedInteger<UnderInt, Derived>::selfCheck() const
{
  if (!Derived::isValid(derivedC().get())) {
    xfailure_stringbc(
      "Value violates constraint for " << Derived::getTypeName() <<
      ": " << derivedC().get() << ".");
  }
}


// --------------------------- Binary tests ----------------------------
template <typename UnderInt, typename Derived>
int WrappedInteger<UnderInt, Derived>::compareTo(Derived const &b) const
{
  using smbase::compare;

  return compare(derivedC().get(), b.get());
}


template <typename UnderInt, typename Derived>
int WrappedInteger<UnderInt, Derived>::compareTo(UnderInt const &b) const
{
  using smbase::compare;

  return compare(derivedC().get(), b);
}


// ----------------------------- Addition ------------------------------
template <typename UnderInt, typename Derived>
Derived WrappedInteger<UnderInt, Derived>::operator+() const
{
  return derivedC();
}


template <typename UnderInt, typename Derived>
Derived WrappedInteger<UnderInt, Derived>::operator+(Derived delta) const
{
  return Derived(addWithOverflowCheck(derivedC().get(), delta.get()));
}


template <typename UnderInt, typename Derived>
Derived WrappedInteger<UnderInt, Derived>::succ() const
{
  return Derived(addWithOverflowCheck(derivedC().get(), 1));
}


template <typename UnderInt, typename Derived>
Derived &WrappedInteger<UnderInt, Derived>::operator+=(Derived delta)
{
  derived().set(addWithOverflowCheck(derivedC().get(), delta.get()));
  return derived();
}


template <typename UnderInt, typename Derived>
Derived &WrappedInteger<UnderInt, Derived>::operator++()
{
  derived().set(addWithOverflowCheck(derivedC().get(), 1));
  return derived();
}


template <typename UnderInt, typename Derived>
Derived WrappedInteger<UnderInt, Derived>::operator++(int)
{
  Derived ret(derivedC());
  derived().set(addWithOverflowCheck(derivedC().get(), 1));
  return ret;
}


// ---------------------- Subtraction/inversion ----------------------
template <typename UnderInt, typename Derived>
Derived WrappedInteger<UnderInt, Derived>::operator-() const
{
  return Derived(subtractWithOverflowCheck(0, derivedC().get()));
}


template <typename UnderInt, typename Derived>
Derived WrappedInteger<UnderInt, Derived>::operator-(Derived delta) const
{
  return Derived(subtractWithOverflowCheck(derivedC().get(), delta.get()));
}


template <typename UnderInt, typename Derived>
Derived WrappedInteger<UnderInt, Derived>::pred() const
{
  return Derived(subtractWithOverflowCheck(derivedC().get(), 1));
}


template <typename UnderInt, typename Derived>
Derived &WrappedInteger<UnderInt, Derived>::operator-=(Derived delta)
{
  derived().set(subtractWithOverflowCheck(derivedC().get(), delta.get()));
  return derived();
}


template <typename UnderInt, typename Derived>
Derived &WrappedInteger<UnderInt, Derived>::operator--()
{
  derived().set(subtractWithOverflowCheck(derivedC().get(), 1));
  return derived();
}


template <typename UnderInt, typename Derived>
Derived WrappedInteger<UnderInt, Derived>::operator--(int)
{
  Derived ret(derivedC());
  derived().set(subtractWithOverflowCheck(derivedC().get(), 1));
  return ret;
}


// -------------------------- Serialization --------------------------
template <typename UnderInt, typename Derived>
WrappedInteger<UnderInt, Derived>::operator gdv::GDValue() const
{
  return gdv::GDValue(derivedC().get());
}


template <typename UnderInt, typename Derived>
WrappedInteger<UnderInt, Derived>::WrappedInteger(gdv::GDValueParser const &p)
{
  p.checkIsInteger();

  gdv::GDVInteger v = p.integerGet();

  if (std::optional<UnderInt> i = v.getAsOpt<UnderInt>()) {
    if (!Derived::isValid(*i)) {
      p.throwError(stringb(
        "Invalid " << Derived::getTypeName() << ": " << v << "."));
    }
    derived().set(*i);
  }
  else {
    p.throwError(stringb(
      "Out of range for " << Derived::getTypeName() << ": " << v << "."));
  }
}


template <typename UnderInt, typename Derived>
void WrappedInteger<UnderInt, Derived>::write(std::ostream &os) const
{
  os << derivedC().get();
}


#endif // EDITOR_WRAPPED_INTEGER_H
