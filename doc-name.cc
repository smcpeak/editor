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
  : m_harn(),
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
  return m_harn.resourceName();
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


/*static*/ DocumentName DocumentName::fromLocalFilename(
  string const &filename)
{
  return fromFilename(HostName::asLocal(), filename);
}


/*static*/ DocumentName DocumentName::fromFilename(
  HostName const &hostName,
  string const &filename)
{
  DocumentName ret;
  ret.setFilename(hostName, filename);
  return ret;
}


/*static*/ DocumentName DocumentName::fromFilenameHarn(
  HostAndResourceName const &harn)
{
  DocumentName ret;
  ret.setFilenameHarn(harn);
  return ret;
}


/*static*/ DocumentName DocumentName::fromNonFileResourceName(
  HostName const &hostName,
  string const &name,
  string const &dir)
{
  DocumentName ret;
  ret.setNonFileResourceName(hostName, name, dir);
  return ret;
}


void DocumentName::selfCheck() const
{
  m_harn.selfCheck();

  if (!empty()) {
    /* There is a somewhat subtle problem here.  The names of files, and
       hence directories, are meant to be interpreted by a partiicular
       file system on a particular host, but I am enforcing rules based
       on whatever the primary file system of the host is that the
       editor is running under.

       Checking that the name ends with "/" is, fortunately, fine for
       both POSIX and Windows.  But I cannot check that the separators
       are normalized because that is host-dependent.  There are other
       places like this in the editor that enforce local host rules for
       paths that could be remote.

       Now, I think the only practical consequence is that the editor,
       when running on Windows, cannot edit a file on a remote POSIX
       host that has a backslash in its name, which is rare (it could
       not be checked in to a git repo without causing portability
       problems for that repo, for example).  But as a matter of
       principle, it would be nice to sort these issues out.

       This also touches on the (in principle) fact that a single host
       could have multiple file systems with differing name semantics,
       whereas I (in `smbase/sm-file-util`) just assume that a POSIX
       host uses POSIX file system rules, and likewise for Windows using
       NTFS rules.
    */
    xassert(endsWith(m_directory, "/"));
  }
}


DocumentName::operator gdv::GDValue() const
{
  // Just add to what `m_harn` writes since the existence of that layer
  // is not really important to potential consumers.
  GDValue m(m_harn.operator GDValue());
  m.taggedContainerSetTag("DocumentName"_sym);

  GDV_WRITE_MEMBER_SYM(m_hasFilename);
  GDV_WRITE_MEMBER_SYM(m_directory);

  return m;
}


StrongOrdering DocumentName::compareTo(DocumentName const &obj) const
{
  COMPARE_MEMB(m_harn);
  COMPARE_MEMB(m_hasFilename);
  COMPARE_MEMB(m_directory);
  return StrongOrdering::equal;
}


void DocumentName::setFilename(HostName const &hostName,
                               string const &filename)
{
  m_harn = HostAndResourceName(hostName, filename);
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


void DocumentName::setNonFileResourceName(
  HostName const &hostName,
  string const &name,
  string const &dir)
{
  m_harn = HostAndResourceName(hostName, name);
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
