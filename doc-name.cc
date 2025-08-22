// doc-name.cc
// Code for doc-name.h.

#include "doc-name.h"                  // this module

// smbase
#include "smbase/gdvalue.h"            // gdv::GDValue
#include "smbase/sm-file-util.h"       // SMFileUtil
#include "smbase/sm-macros.h"          // COMPARE_MEMB

// libc++
#include <iostream>                    // std::ostream

using namespace gdv;


DocumentName::DocumentName()
  : HostAndResourceName(),
    m_hasFilename(false),
    m_directory()
{
  selfCheck();
}


DocumentName::~DocumentName()
{}


string DocumentName::filename() const
{
  xassert(hasFilename());
  return m_resourceName;
}


bool DocumentName::isLocalFilename() const
{
  return !empty() && isLocal() && hasFilename();
}


void DocumentName::setDirectory(string const &dir)
{
  SMFileUtil sfu;
  m_directory = sfu.ensureEndsWithDirectorySeparator(
    sfu.normalizePathSeparators(dir));
}


void DocumentName::selfCheck() const
{
  HostAndResourceName::selfCheck();
}


DocumentName::operator gdv::GDValue() const
{
  GDValue m(HostAndResourceName::operator GDValue());
  m.taggedContainerSetTag("DocumentName"_sym);

  GDV_WRITE_MEMBER_SYM(m_hasFilename);
  GDV_WRITE_MEMBER_SYM(m_directory);
  return m;
}


StrongOrdering DocumentName::compareTo(DocumentName const &obj) const
{
  return HostAndResourceName::compareTo(obj);
}


void DocumentName::setFilename(HostName const &hostName,
                               string const &filename)
{
  m_hostName = hostName;
  m_resourceName = filename;
  m_hasFilename = true;

  string dir, base;
  SMFileUtil().splitPath(dir, base, filename);
  this->setDirectory(dir);

  selfCheck();
}


void DocumentName::setFilenameHarn(HostAndResourceName const &harn)
{
  setFilename(harn.hostName(), harn.resourceName());
}


void DocumentName::setNonFileResourceName(HostName const &hostName,
  string const &name, string const &dir)
{
  m_hostName = hostName;
  m_resourceName = name;
  m_hasFilename = false;

  this->setDirectory(dir);

  selfCheck();
}


std::string DocumentName::toString() const
{
  return harn().toString();
}


void DocumentName::write(std::ostream &os) const
{
  os << harn();
}


// EOF
