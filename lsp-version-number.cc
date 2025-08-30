// lsp-version-number.cc
// Code for `lsp-version-number` module.

#include "lsp-version-number.h"        // this module

#include "td-version-number.h"         // TD_VersionNumber
#include "wrapped-integer.h"           // WrappedInteger method defns

#include "smbase/compare-util.h"       // smbase::compare, COMPARE_MEMBERS
#include "smbase/overflow.h"           // convertNumber
#include "smbase/xassert.h"            // xassert

#include <cstdint>                     // std::int32_t, etc.

using namespace smbase;


// Explicitly instantiate the base class methods.
template class WrappedInteger<std::int32_t, LSP_VersionNumber>;



LSP_VersionNumber::LSP_VersionNumber(std::uint32_t value)
  : Base(convertNumber<std::int32_t>(value))
{}


LSP_VersionNumber::LSP_VersionNumber(std::uint64_t value)
  : Base(convertNumber<std::int32_t>(value))
{}


LSP_VersionNumber::LSP_VersionNumber(std::int64_t value)
  : Base(convertNumber<std::int32_t>(value))
{}


LSP_VersionNumber::LSP_VersionNumber(smbase::Integer const &value)
  : Base(value.getAs<std::int32_t>())
{}


TD_VersionNumber LSP_VersionNumber::toTD_VersionNumber() const
{
  return TD_VersionNumber(get());
}


/*static*/ LSP_VersionNumber LSP_VersionNumber::fromTDVN(
  TD_VersionNumber value)
{
  // This will use the `int64_t` case.
  return LSP_VersionNumber(value.get());
}


// --------------------------- Binary tests ----------------------------
int LSP_VersionNumber::compareTo(TD_VersionNumber const &b) const
{
  // Convert both to 64-bit signed int and compare.
  //
  // TODO: I would like to have an easy way to statically verify there
  // are no conversions here that could change either value.
  return compare(std::int64_t(get()), b.get());
}


// EOF
