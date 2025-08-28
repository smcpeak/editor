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
#include "smbase/xassert.h"            // xassert, xfailure_stringbc

#include <iostream>                    // std::ostream
#include <optional>                    // std::optional
#include <string>                      // std::string


template <typename Derived>
void WrappedInteger<Derived>::selfCheck() const
{
  if (!Derived::isValid(derivedC().get())) {
    xfailure_stringbc(
      "Value violates constraint for " << Derived::getTypeName() <<
      ": " << derivedC().get() << ".");
  }
}


// --------------------------- Binary tests ----------------------------
template <typename Derived>
int WrappedInteger<Derived>::compareTo(Derived const &b) const
{
  using smbase::compare;

  return compare(derivedC().get(), b.get());
}


template <typename Derived>
int WrappedInteger<Derived>::compareTo(int const &b) const
{
  using smbase::compare;

  return compare(derivedC().get(), b);
}


// ----------------------------- Addition ------------------------------
template <typename Derived>
Derived WrappedInteger<Derived>::operator+() const
{
  return derivedC();
}


template <typename Derived>
Derived WrappedInteger<Derived>::operator+(Derived delta) const
{
  return Derived(derivedC().get() + delta.get());
}


template <typename Derived>
Derived WrappedInteger<Derived>::succ() const
{
  return Derived(derivedC().get() + 1);
}


template <typename Derived>
Derived &WrappedInteger<Derived>::operator+=(Derived delta)
{
  derived().set(derivedC().get() + delta.get());
  return derived();
}


template <typename Derived>
Derived &WrappedInteger<Derived>::operator++()
{
  derived().set(derivedC().get() + 1);
  return derived();
}


template <typename Derived>
Derived WrappedInteger<Derived>::operator++(int)
{
  Derived ret(derivedC());
  derived().set(derivedC().get() + 1);
  return ret;
}


// ---------------------- Subtraction/inversion ----------------------
template <typename Derived>
Derived WrappedInteger<Derived>::operator-() const
{
  return Derived(- derivedC().get());
}


template <typename Derived>
Derived WrappedInteger<Derived>::operator-(Derived delta) const
{
  return Derived(derivedC().get() - delta.get());
}


template <typename Derived>
Derived WrappedInteger<Derived>::pred() const
{
  return Derived(derivedC().get() - 1);
}


template <typename Derived>
Derived &WrappedInteger<Derived>::operator-=(Derived delta)
{
  derived().set(derivedC().get() - delta.get());
  return derived();
}


template <typename Derived>
Derived &WrappedInteger<Derived>::operator--()
{
  derived().set(derivedC().get() - 1);
  return derived();
}


template <typename Derived>
Derived WrappedInteger<Derived>::operator--(int)
{
  Derived ret(derivedC());
  derived().set(derivedC().get() - 1);
  return ret;
}


// -------------------------- Serialization --------------------------
template <typename Derived>
WrappedInteger<Derived>::operator gdv::GDValue() const
{
  return gdv::GDValue(derivedC().get());
}


template <typename Derived>
WrappedInteger<Derived>::WrappedInteger(gdv::GDValueParser const &p)
{
  p.checkIsInteger();

  gdv::GDVInteger v = p.integerGet();

  if (std::optional<int> i = v.getAsOpt<int>()) {
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


template <typename Derived>
void WrappedInteger<Derived>::write(std::ostream &os) const
{
  os << derivedC().get();
}


#endif // EDITOR_WRAPPED_INTEGER_H
