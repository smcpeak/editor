// host-file-and-line-opt.cc
// Code for `host-file-and-line-opt` module.

#include "host-file-and-line-opt.h"    // this module

#include "smbase/gdvalue-optional.h"   // gdv::toGDValue(std::optional)
#include "smbase/gdvalue.h"            // gdv::GDValue
#include "smbase/xassert.h"            // xassert, xassertPrecondition

using namespace gdv;


void HostFile_OptLineByte::selfCheck() const
{
  if (m_lineIndex) {
    m_lineIndex->selfCheck();
  }

  if (m_byteIndex) {
    xassert(m_lineIndex);
    m_byteIndex->selfCheck();
  }
}


std::string HostFile_OptLineByte::getFilename() const
{
  return getHarn().resourceName();
}


LineIndex HostFile_OptLineByte::getLineIndex() const
{
  xassertPrecondition(hasLineIndex());
  return *m_lineIndex;
}


ByteIndex HostFile_OptLineByte::getByteIndex() const
{
  xassertPrecondition(hasByteIndex());
  return *m_byteIndex;
}


void HostFile_OptLineByte::setHarn(HostAndResourceName const &harn)
{
  m_harn = harn;
}


HostFile_OptLineByte::operator gdv::GDValue() const
{
  GDValue m(GDVK_TAGGED_ORDERED_MAP, "HostFile_OptLineByte"_sym);
  GDV_WRITE_MEMBER_SYM(m_harn);
  GDV_WRITE_MEMBER_SYM(m_lineIndex);
  GDV_WRITE_MEMBER_SYM(m_byteIndex);
  return m;
}


// EOF
