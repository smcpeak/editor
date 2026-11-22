// mock-host-file-probe.cc
// Code for `mock-host-file-probe` module.

#include "mock-host-file-probe.h"      // this module

// smbase
#include "smbase/map-util.h"           // smbase::{mapContains, mapFindOpt, mapInsertUnique}

// libc++
#include <optional>                    // std::optional

using namespace smbase;


void MockHostFileProbe::add(HostAndResourceName const &harn)
{
  addTS(harn, m_existingHARNs.size() + 1);
}


void MockHostFileProbe::addTS(HostAndResourceName const &harn, FileTimestamp ts)
{
  mapInsertUnique(m_existingHARNs, harn, ts);
}


void MockHostFileProbe::setTS(HostAndResourceName const &harn, FileTimestamp ts)
{
  xassert(mapContains(m_existingHARNs, harn));
  m_existingHARNs.at(harn) = ts;
}


std::optional<FileTimestamp> MockHostFileProbe::hfModTime(
  HostAndResourceName const &harn)
{
  if (auto it = mapFindOpt(m_existingHARNs, harn)) {
    return (**it).second;
  }
  else {
    return std::nullopt;
  }
}


// EOF
