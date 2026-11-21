// ihfexists.cc
// Code for `ihfexists` module.

#include "ihfexists.h"                 // this module

// libc++
#include <optional>                    // std::optional


IHFExists::~IHFExists()
{}


bool IHFExists::hfExists(HostAndResourceName const &harn)
{
  std::optional<FileTimestamp> modTime = hfModTime(harn);
  return modTime.has_value();
}


// EOF
