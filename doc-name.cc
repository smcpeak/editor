// doc-name.cc
// Code for doc-name.h.

#include "doc-name.h"                  // this module

// smbase
#include "sm-file-util.h"              // SMFileUtil
#include "strutil.h"                   // quoted

// libc++
#include <iostream>                    // std::ostream


DocumentName::DocumentName()
  : m_resourceName(),
    m_hasFilename(false),
    m_directory()
{}


DocumentName::~DocumentName()
{}


StrongOrdering DocumentName::compareTo(DocumentName const &obj) const
{
  return strongOrderFromInt(m_resourceName.compareTo(obj.m_resourceName));
}


string DocumentName::filename() const
{
  xassert(hasFilename());
  return m_resourceName;
}


void DocumentName::setDirectory(string const &dir)
{
  SMFileUtil sfu;
  m_directory = sfu.ensureEndsWithDirectorySeparator(
    sfu.normalizePathSeparators(dir));
}


void DocumentName::setFilename(string const &filename)
{
  m_resourceName = filename;
  m_hasFilename = true;

  string dir, base;
  SMFileUtil().splitPath(dir, base, filename);
  this->setDirectory(dir);
}


void DocumentName::setNonFileName(string const &name, string const &dir)
{
  m_resourceName = name;
  m_hasFilename = false;

  this->setDirectory(dir);
}


string DocumentName::toString() const
{
  return quoted(resourceName());
}


std::ostream& DocumentName::print(std::ostream &os) const
{
  return os << toString();
}


// EOF
