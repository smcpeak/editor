// byte-count.h
// `ByteCount`, a count of bytes.

// See license.txt for copyright and terms of use.

#ifndef EDITOR_BYTE_COUNT_H
#define EDITOR_BYTE_COUNT_H

#include "byte-count-fwd.h"            // fwds for this module

#include "byte-difference-fwd.h"       // ByteDifference [n]
#include "wrapped-integer-iface.h"     // WrappedInteger

#include "smbase/compare-util-iface.h" // DECLARE_COMPARETO_AND_DEFINE_RELATIONALS_TO_OTHER
#include "smbase/std-string-fwd.h"     // std::string [n]

#include <cstddef>                     // std::{ptrdiff_t, size_t}


// A count of bytes.  Always non-negative.
//
// This differs from `ByteIndex` in that the latter is more like a
// pointer, whereas this class is more like a size.
//
// In the hierarchy of "byte" measures, a count is more specific than a
// difference, but less specific than an index.
//
// See doc/byte-measures.txt.
//
class ByteCount final : public WrappedInteger<int, ByteCount> {
public:      // types
  using Base = WrappedInteger<int, ByteCount>;
  friend Base;

protected:   // methods
  static bool isValid(int value)
    { return value >= 0; }

  static char const *getTypeName()
    { return "ByteCount"; }

public:      // methods
  // Inherit ctors.
  using Base::Base;

  // --------------------------- Conversion ----------------------------
  // These check for range compatibility.
  explicit ByteCount(std::ptrdiff_t size);
  explicit ByteCount(std::size_t size);

  // Explicit "down" conversion.
  //
  // Requires: delta >= 0
  explicit ByteCount(ByteDifference delta);

  // Implicit "up" conversion.
  operator ByteDifference() const;

  // ---------------------------- Addition -----------------------------
  using Base::operator+;
  using Base::operator+=;

  // Requires: `m_value+delta >= 0`, and the sum is representable.
  ByteCount operator+(ByteDifference delta) const;
  ByteCount &operator+=(ByteDifference delta);

  // ---------------------- Subtraction/inversion ----------------------
  // Don't inherit `operator-` or `operator-=`.

  // Subtracting two indices yields a difference.
  ByteDifference operator-() const;
  ByteDifference operator-(ByteCount count) const;

  // index-difference yields index.
  //
  // Requires: *this >= delta
  ByteCount operator-(ByteDifference delta) const;
  ByteCount &operator-=(ByteDifference delta);
};


// `strlen`, but returning a `ByteCount`.
ByteCount strlenBC(char const *str);

// `memchr`, but using a `ByteCount`.
void const *memchrBC(void const *p, char c, ByteCount length);

// `memcmp`, but using a `ByteCount`.
int memcmpBC(void const *a, void const *b, ByteCount length);

// `memcpy`, but using a `ByteCount`.
void memcpyBC(char *dest, char const *src, ByteCount length);

// `str.size()`, but returning a `ByteCount`.
ByteCount sizeBC(std::string const &str);

// Make a string using a `ByteCount` length.
//
// TODO: Rename to `stringBC`.
std::string mkString(char const *text, ByteCount length);


#endif // EDITOR_BYTE_COUNT_H
