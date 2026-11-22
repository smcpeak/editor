// host-file-probe.cc
// Code for `host-file-probe` module.

#include "host-file-probe.h"           // this module

// libc++
#include <optional>                    // std::optional


HostFileProbe::~HostFileProbe()
{}


bool HostFileProbe::hfExists(HostAndResourceName const &harn)
{
  std::optional<FileTimestamp> modTime = hfModTime(harn);
  return modTime.has_value();
}


// EOF
