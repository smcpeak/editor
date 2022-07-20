// doc-name.cc
// Code for doc-name.h.

#include "doc-name.h"                  // this module

// smbase
#include "sm-file-util.h"              // SMFileUtil


DocumentName::DocumentName()
  : m_docName(),
    m_hasFilename(false),
    m_directory()
{}


DocumentName::~DocumentName()
{}


StrongOrdering DocumentName::compareTo(DocumentName const &obj) const
{
  return strongOrderFromInt(m_docName.compareTo(obj.m_docName));
}


string DocumentName::filename() const
{
  xassert(hasFilename());
  return m_docName;
}


void DocumentName::setDirectory(string const &dir)
{
  SMFileUtil sfu;
  m_directory = sfu.ensureEndsWithDirectorySeparator(
    sfu.normalizePathSeparators(dir));
}


void DocumentName::setFilename(string const &filename)
{
  m_docName = filename;
  m_hasFilename = true;

  string dir, base;
  SMFileUtil().splitPath(dir, base, filename);
  this->setDirectory(dir);
}


void DocumentName::setNonFileName(string const &name, string const &dir)
{
  m_docName = name;
  m_hasFilename = false;

  this->setDirectory(dir);
}


// EOF
