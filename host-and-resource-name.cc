// host-and-resource-name.cc
// Code for host-and-resource-name.h.

#include "host-and-resource-name.h"    // this module

// smbase
#include "strutil.h"                   // quoted
#include "xassert.h"                   // xassert


HostAndResourceName::HostAndResourceName()
  : m_hostName(HostName::asLocal()),
    m_resourceName()
{}


HostAndResourceName::~HostAndResourceName()
{}


HostAndResourceName::HostAndResourceName(HostName const &hostName,
                                         string const &resourceName)
  : m_hostName(hostName),
    m_resourceName(resourceName)
{
  selfCheck();
}


void HostAndResourceName::selfCheck() const
{
  if (m_resourceName.empty()) {
    xassert(m_hostName.isLocal());
  }
}


StrongOrdering
  HostAndResourceName::compareTo(HostAndResourceName const &obj) const
{
  COMPARE_MEMB(m_hostName);
  COMPARE_MEMB(m_resourceName);

  return StrongOrdering::equal;
}


string HostAndResourceName::toString() const
{
  if (isLocal()) {
    return quoted(resourceName());
  }
  else {
    return stringb(m_hostName << ": " << quoted(resourceName()));
  }
}


std::ostream& HostAndResourceName::print(std::ostream &os) const
{
  return os << toString();
}


// EOF
