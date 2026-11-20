// mock-hfexists.cc
// Code for `mock-hfexists` module.

#include "mock-hfexists.h"             // this module

// smbase
#include "smbase/container-util.h"     // smbase::contains

using namespace smbase;


bool MockHFExists::hfExists(HostAndResourceName const &harn)
{
  return contains(m_existingHARNs, harn);
}


// EOF
