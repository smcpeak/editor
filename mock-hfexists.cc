// mock-hfexists.cc
// Code for `mock-hfexists` module.

#include "mock-hfexists.h"             // this module

// smbase
#include "smbase/map-util.h"           // smbase::{mapContains, mapFindOpt, mapInsertUnique}

// libc++
#include <cstdint>                     // std::int64_t
#include <optional>                    // std::optional

using namespace smbase;


void MockHFExists::add(HostAndResourceName const &harn)
{
  addTS(harn, m_existingHARNs.size() + 1);
}


void MockHFExists::addTS(HostAndResourceName const &harn, std::int64_t ts)
{
  mapInsertUnique(m_existingHARNs, harn, ts);
}


void MockHFExists::setTS(HostAndResourceName const &harn, std::int64_t ts)
{
  xassert(mapContains(m_existingHARNs, harn));
  m_existingHARNs.at(harn) = ts;
}


std::optional<std::int64_t> MockHFExists::hfModTime(
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
