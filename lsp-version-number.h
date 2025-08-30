// lsp-version-number.h
// `LSP_VersionNumber`, representing the version numbers in LSP.

// See license.txt for copyright and terms of use.

#ifndef EDITOR_LSP_VERSION_NUMBER_H
#define EDITOR_LSP_VERSION_NUMBER_H

#include "lsp-version-number-fwd.h"    // fwds for this module

#include "td-version-number-fwd.h"     // TD_VersionNumber [n]
#include "wrapped-integer-iface.h"     // WrappedInteger

#include "smbase/compare-util-iface.h" // DECLARE_COMPARETO_AND_DEFINE_RELATIONALS_TO_OTHER
#include "smbase/sm-integer-fwd.h"     // smbase::Integer

#include <cstdint>                     // std::int32_t, etc.


/* Represents the version numbers in LSP.

   One reason this class exists is that `integer` in LSP, which is the
   type used for version numbers, is limited to the range of a signed
   32-bit integer:

     https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#integer

   There is an open issue asking for clarification upon overflow:

     https://github.com/microsoft/language-server-protocol/issues/2053

   but it has no resolution (the devs basically dismiss the problem as
   having no practical concern based on, IMO, excessively optimistic
   assumptions about the rate of version number use).

   Consequently, it's up to clients to avoid crossing that boundary, and
   this class is part of my strategy for that.
*/
class LSP_VersionNumber final :
                public WrappedInteger<std::int32_t, LSP_VersionNumber> {
public:      // types
  using Base = WrappedInteger<std::int32_t, LSP_VersionNumber>;
  friend Base;

protected:   // methods
  // The LSP protocol does not seem to impose any constraints beyond the
  // 32-bit limit, but I will insist that it be non-negative.
  static bool isValid(std::int32_t value)
    { return value >= 0; }

  static char const *getTypeName()
    { return "LSP_VersionNumber"; }

public:      // methods
  // Inherit ctors.
  using Base::Base;

  // Allow converting from wider integers.  These throw
  // `XNumericConversion` (`smbase/xoverflow.h`) if the value is beyond
  // what we can represent.  (However, if it is negative, then we throw
  // `XAssert` like other `WrappedInteger`s given invalid values.)
  explicit LSP_VersionNumber(std::uint32_t value);
  explicit LSP_VersionNumber(std::uint64_t value);
  explicit LSP_VersionNumber(std::int64_t value);
  explicit LSP_VersionNumber(smbase::Integer const &value);

  // Experiment: I want to avoid implicit conversions when constructing
  // this class, so let's provide, and delete, a constructor template.
  // This should ensure no value conversions happen when constructing
  // this type.
  template <typename T>
  explicit LSP_VersionNumber(T const &t) = delete;

  // Convert to a text document version, which is always safe.
  TD_VersionNumber toTD_VersionNumber() const;

  // Convert to an LSP version, throwing `XNumericConversion` if it
  // cannot be so represented.
  static LSP_VersionNumber fromTDVN(TD_VersionNumber value);

  // -------------------------- Binary tests ---------------------------
  // Keep the default comparisons.
  using Base::compareTo;

  // Allow freely comparing the two kinds of versions without having to
  // convert one to the other.
  DECLARE_COMPARETO_AND_DEFINE_RELATIONALS_TO_OTHER(
    LSP_VersionNumber, TD_VersionNumber);
};


#endif // EDITOR_LSP_VERSION_NUMBER_H
