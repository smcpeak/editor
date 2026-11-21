// ihfexists.cc
// Code for `ihfexists` module.

#include "ihfexists.h"                 // this module

// libc++
#include <optional>                    // std::optional


IHFExists::~IHFExists()
{}


bool IHFExists::hfExists(HostAndResourceName const &harn)
{
  std::optional<std::int64_t> modTime = hfModTime(harn);
  return modTime.has_value();
}


// EOF
