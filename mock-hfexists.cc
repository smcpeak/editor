// mock-hfexists.cc
// Code for `mock-hfexists` module.

#include "mock-hfexists.h"             // this module

// smbase
#include "smbase/map-util.h"           // smbase::{mapContains, mapFindOpt, mapInsertUnique}

// libc++
#include <optional>                    // std::optional

using namespace smbase;


void MockHFExists::add(HostAndResourceName const &harn)
{
  addTS(harn, m_existingHARNs.size() + 1);
}


void MockHFExists::addTS(HostAndResourceName const &harn, FileTimestamp ts)
{
  mapInsertUnique(m_existingHARNs, harn, ts);
}


void MockHFExists::setTS(HostAndResourceName const &harn, FileTimestamp ts)
{
  xassert(mapContains(m_existingHARNs, harn));
  m_existingHARNs.at(harn) = ts;
}


std::optional<FileTimestamp> MockHFExists::hfModTime(
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
