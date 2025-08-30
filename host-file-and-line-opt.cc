// host-file-and-line-opt.cc
// Code for `host-file-and-line-opt` module.

#include "host-file-and-line-opt.h"    // this module

#include "smbase/xassert.h"            // xassert


void HostFileAndLineOpt::selfCheck() const
{
  if (m_line) {
    m_line->selfCheck();
  }

  if (m_byteIndex) {
    xassert(*m_byteIndex >= 0);
  }
}


// EOF
