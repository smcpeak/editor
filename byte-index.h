// byte-index.h
// `ByteIndex`, a 0-based byte index.

// See license.txt for copyright and terms of use.

#ifndef EDITOR_BYTE_INDEX_H
#define EDITOR_BYTE_INDEX_H

#include "byte-index-fwd.h"            // fwds for this module

#include "byte-difference-fwd.h"       // ByteDifference [n]
#include "byte-count-fwd.h"            // ByteCount [n]
#include "wrapped-integer-iface.h"     // WrappedInteger

#include "smbase/std-string-fwd.h"     // std::string

#include <cstddef>                     // std::{ptrdiff_t, size_t}


// A 0-based byte index into some array.  Always non-negative.
//
// This differs from `ByteCount` in that the latter is more like a size,
// whereas this class is more like a pointer.
//
// In the hierarchy of "byte" measures, an index is the most specific
// (compared to difference and count), as it is a count measured from a
// specific origin position.
//
class ByteIndex final : public WrappedInteger<int, ByteIndex> {
public:      // types
  using Base = WrappedInteger<int, ByteIndex>;
  friend Base;

protected:   // methods
  static bool isValid(int value)
    { return value >= 0; }

  static char const *getTypeName()
    { return "ByteIndex"; }

public:      // methods
  // Inherit ctors.
  using Base::Base;

  // --------------------------- Conversion ----------------------------
  // These check for range compatibility.
  explicit ByteIndex(std::ptrdiff_t size);
  explicit ByteIndex(std::size_t size);

  // Explicit "downcast": count -> index.
  explicit ByteIndex(ByteCount count);

  // Explicit "downcast": difference -> index.
  explicit ByteIndex(ByteDifference delta);

  // Implicit "upcast": index -> count.
  operator ByteCount() const;

  // Implicit "upcast": index -> difference.
  operator ByteDifference() const;

  // -------------------------- Binary tests ---------------------------
  using Base::compareTo;

  // Allow comparison with `ByteDifference` and `ByteCount`.
  DECLARE_COMPARETO_AND_DEFINE_RELATIONALS_TO_OTHER(ByteIndex, ByteDifference);
  DECLARE_COMPARETO_AND_DEFINE_RELATIONALS_TO_OTHER(ByteIndex, ByteCount);

  // ---------------------------- Addition -----------------------------
  using Base::operator+;
  using Base::operator+=;

  ByteIndex operator+(ByteCount delta) const;
  ByteIndex &operator+=(ByteCount delta);

  // Requires: `m_value+delta >= 0`, and the sum is representable.
  ByteIndex operator+(ByteDifference delta) const;
  ByteIndex &operator+=(ByteDifference delta);

  // ---------------------- Subtraction/inversion ----------------------
  // Don't inherit `operator-` or `operator-=`.

  // Subtracting two indices yields a difference.
  ByteDifference operator-() const;
  ByteDifference operator-(ByteIndex b) const;

  // index-difference yields index.
  //
  // Requires: *this >= delta
  ByteIndex operator-(ByteDifference delta) const;
  ByteIndex &operator-=(ByteDifference delta);

  // `*this -= delta`, except do not go below `lowerLimit`.
  void clampDecrease(
    ByteDifference delta, ByteIndex lowerLimit = ByteIndex(0));
};


// Allow using a `ByteIndex` as an index for `std::string`.  Except, we
// cannot overload `operator[]` here, so make do with `at`.
char const &at(std::string const &str, ByteIndex index);

// Also allow extracting a substring.
std::string substr(
  std::string const &str, ByteIndex start, ByteCount count);


#endif // EDITOR_BYTE_INDEX_H
