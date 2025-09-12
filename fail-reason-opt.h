// fail-reason-opt.h
// `FailReasonOpt`, an optional failure reason.

// See license.txt for copyright and terms of use.

#ifndef EDITOR_FAIL_REASON_OPT_H
#define EDITOR_FAIL_REASON_OPT_H

#include <optional>                    // std::optional
#include <string>                      // std::string


// This type is used as the return value for functions that either do
// something successfully, or else return a string containing an English
// sentence explaining why it could not be done.
//
// Note: This is *not* to be used as a general shorthand for an optional
// string!
using FailReasonOpt =
  std::optional<std::string>;


#endif // EDITOR_FAIL_REASON_OPT_H
