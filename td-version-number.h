// td-version-number.h
// `TD_VersionNumber`, a version number for `TextDocument`s.

// See license.txt for copyright and terms of use.

#ifndef EDITOR_TD_VERSION_NUMBER_H
#define EDITOR_TD_VERSION_NUMBER_H

#include "td-version-number-fwd.h"     // fwds for this module

#include "wrapped-integer-iface.h"     // WrappedInteger

#include <cstdint>                     // std::int64_t


/* A version number for `TextDocument`s.

   The primary reason to have this is to coordinate with the LSP server.
   But it uses the comparatively limited `LSP_VersionNumber`.
   Internally I do not want to limit myself to 32 bits, so I store a
   64-bit number, and convert (with runtime checking) where needed.

   However, note that this class does not have any dependencies on
   anything specific to LSP, as it's meant to be independent of any
   particular analysis tool or protocol.  Instead `LSP_VersionNumber`
   has the conversion logic.
*/
class TD_VersionNumber final :
                 public WrappedInteger<std::int64_t, TD_VersionNumber> {
public:      // types
  using Base = WrappedInteger<std::int64_t, TD_VersionNumber>;
  friend Base;

protected:   // methods
  // I will insist that versions be non-negative.
  static bool isValid(std::int64_t value)
    { return value >= 0; }

  static char const *getTypeName()
    { return "TD_VersionNumber"; }

public:      // methods
  // Inherit ctors.
  using Base::Base;

  // `WrappedInteger::operator++()` does an overflow check.  I have at
  // least one line of code that was using this more explicit call to
  // also do that check.  I'll overload it so the call site still
  // explicitly advertises its safety.
  friend TD_VersionNumber &preIncrementWithOverflowCheck(TD_VersionNumber &obj)
  {
    return ++obj;
  }
};


#endif // EDITOR_TD_VERSION_NUMBER_H
