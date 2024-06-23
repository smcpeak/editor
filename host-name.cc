// host-name.cc
// Code for host-name.h.

#include "host-name.h"                 // this module

// smbase
#include "xassert.h"                   // xassert

// libc++
#include <iostream>


HostName::HostName()
  : m_sshHostName()
{}


HostName::~HostName()
{}


/*static*/ HostName HostName::asLocal()
{
  return HostName();
}


/*static*/ HostName HostName::asSSH(string const &hostname)
{
  HostName ret;
  ret.m_sshHostName = hostname;
  return ret;
}


StrongOrdering HostName::compareTo(HostName const &obj) const
{
  return strongOrderFromInt(m_sshHostName.compare(obj.m_sshHostName));
}


string HostName::getSSHHostName() const
{
  xassert(isSSH());
  return m_sshHostName;
}


string HostName::toString() const
{
  if (isLocal()) {
    return "local";
  }
  else {
    return stringb("ssh:" << m_sshHostName);
  }
}


std::ostream& HostName::print(std::ostream &os) const
{
  return os << toString();
}


// EOF
