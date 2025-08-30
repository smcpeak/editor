// td-version-number.cc
// Code for `td-version-number` module.

#include "td-version-number.h"         // this module

#include "wrapped-integer.h"           // WrappedInteger method defns

#include <cstdint>                     // std::int64_t


// Explicitly instantiate the base class methods.
template class WrappedInteger<std::int64_t, TD_VersionNumber>;


// EOF
