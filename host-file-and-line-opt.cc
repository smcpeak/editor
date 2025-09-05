// host-file-and-line-opt.cc
// Code for `host-file-and-line-opt` module.

#include "host-file-and-line-opt.h"    // this module

#include "smbase/gdvalue-optional.h"   // gdv::toGDValue(std::optional)
#include "smbase/gdvalue.h"            // gdv::GDValue
#include "smbase/xassert.h"            // xassert, xassertPrecondition

using namespace gdv;


void HostFileAndLineOpt::selfCheck() const
{
  if (m_lineIndex) {
    m_lineIndex->selfCheck();
  }

  if (m_byteIndex) {
    xassert(m_lineIndex);
    m_byteIndex->selfCheck();
  }
}


std::string HostFileAndLineOpt::getFilename() const
{
  return getHarn().resourceName();
}


LineIndex HostFileAndLineOpt::getLineIndex() const
{
  xassertPrecondition(hasLineIndex());
  return *m_lineIndex;
}


ByteIndex HostFileAndLineOpt::getByteIndex() const
{
  xassertPrecondition(hasByteIndex());
  return *m_byteIndex;
}


void HostFileAndLineOpt::setHarn(HostAndResourceName const &harn)
{
  m_harn = harn;
}


HostFileAndLineOpt::operator gdv::GDValue() const
{
  GDValue m(GDVK_TAGGED_ORDERED_MAP, "HostFileAndLineOpt"_sym);
  GDV_WRITE_MEMBER_SYM(m_harn);
  GDV_WRITE_MEMBER_SYM(m_lineIndex);
  GDV_WRITE_MEMBER_SYM(m_byteIndex);
  return m;
}


// EOF
