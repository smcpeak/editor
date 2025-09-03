// byte-difference.h
// `ByteDifference`, a difference between two byte indices.

// See license.txt for copyright and terms of use.

#ifndef EDITOR_BYTE_DIFFERENCE_H
#define EDITOR_BYTE_DIFFERENCE_H

#include "byte-difference-fwd.h"       // fwds for this module

#include "wrapped-integer-iface.h"     // WrappedInteger


// A difference between two byte indices.
//
// In the hierarchy of "byte" measures, a difference is the most general
// category.
//
class ByteDifference final : public WrappedInteger<int, ByteDifference> {
public:      // types
  using Base = WrappedInteger<int, ByteDifference>;
  friend Base;

protected:   // methods
  static bool isValid(int value)
    { return true; }

  static char const *getTypeName()
    { return "ByteDifference"; }

public:      // methods
  // Inherit ctors.
  using Base::Base;
};


// Allow using `ByteDifference` for pointer arithmetic on `char*`.
inline char const *operator+(char const *p, ByteDifference delta)
{
  return p + delta.get();
}

inline char *operator+(char *p, ByteDifference delta)
{
  return p + delta.get();
}

inline char const *&operator+=(char const *&p, ByteDifference delta)
{
  return p += delta.get();
}

inline char *&operator+=(char *&p, ByteDifference delta)
{
  return p += delta.get();
}


#endif // EDITOR_BYTE_DIFFERENCE_H
