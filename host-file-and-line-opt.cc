// host-file-and-line-opt.cc
// Code for `host-file-and-line-opt` module.

#include "host-file-and-line-opt.h"    // this module

#include "smbase/xassert.h"            // xassert, xassertPrecondition


void HostFileAndLineOpt::selfCheck() const
{
  if (m_line) {
    m_line->selfCheck();
  }

  if (m_byteIndex) {
    xassert(*m_byteIndex >= 0);
  }
}


HostAndResourceName const &HostFileAndLineOpt::getHarn() const
{
  xassertPrecondition(hasFilename());
  return *m_harn;
}


LineNumber HostFileAndLineOpt::getLine() const
{
  xassertPrecondition(hasLine());
  return *m_line;
}


int HostFileAndLineOpt::getByteIndex() const
{
  xassertPrecondition(hasByteIndex());
  return *m_byteIndex;
}


void HostFileAndLineOpt::setHarn(HostAndResourceName const &harn)
{
  m_harn = harn;
}


// EOF
