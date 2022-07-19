// named-td.cc
// code for named-td.h

#include "named-td.h"                  // this module

// smbase
#include "dev-warning.h"               // DEV_WARNING
#include "sm-macros.h"                 // CMEMB
#include "nonport.h"                   // getFileModificationTime
#include "objcount.h"                  // CHECK_OBJECT_COUNT
#include "sm-file-util.h"              // SMFileUtil
#include "trace.h"                     // TRACE

// Qt
#include <qnamespace.h>                // Qt class/namespace


int NamedTextDocument::s_objectCount = 0;

CHECK_OBJECT_COUNT(NamedTextDocument);


NamedTextDocument::NamedTextDocument()
  : TextDocument(),
    m_docName(),
    m_hasFilename(false),
    m_directory(),
    m_lastFileTimestamp(0),
    m_title(),
    m_highlighter(NULL),
    m_highlightTrailingWhitespace(true)
{
  NamedTextDocument::s_objectCount++;
}

NamedTextDocument::~NamedTextDocument()
{
  NamedTextDocument::s_objectCount--;
  if (m_highlighter) {
    delete m_highlighter;
  }
}


void NamedTextDocument::setDocumentProcessStatus(DocumentProcessStatus status)
{
  this->TextDocument::setDocumentProcessStatus(status);

  if (this->isProcessOutput()) {
    this->m_highlightTrailingWhitespace = false;
  }
}


string NamedTextDocument::filename() const
{
  xassert(this->hasFilename());
  return m_docName;
}


void NamedTextDocument::setDirectory(string const &dir)
{
  SMFileUtil sfu;
  m_directory = sfu.ensureEndsWithDirectorySeparator(
    sfu.normalizePathSeparators(dir));
}


void NamedTextDocument::setFilename(string const &filename)
{
  m_docName = filename;
  m_hasFilename = true;

  string dir, base;
  SMFileUtil().splitPath(dir, base, filename);
  this->setDirectory(dir);
}


void NamedTextDocument::setNonFileName(string const &name, string const &dir)
{
  m_docName = name;
  m_hasFilename = false;

  this->setDirectory(dir);
}


static char const *documentProcessStatusIndicator(
  NamedTextDocument const *doc)
{
  switch (doc->documentProcessStatus()) {
    default:
      DEV_WARNING("Bad document process status");
      // fallthrough
    case DPS_NONE:           return "";
    case DPS_RUNNING:        return "<running> ";
    case DPS_FINISHED:       return "<finished> ";
  }
}

string NamedTextDocument::nameWithStatusIndicators() const
{
  stringBuilder sb;
  sb << documentProcessStatusIndicator(this);
  sb << this->docName();
  if (this->unsavedChanges()) {
    sb << " *";
  }
  return sb;
}


void NamedTextDocument::readFile()
{
  xassert(this->hasFilename());

  SMFileUtil sfu;

  std::vector<unsigned char> bytes(sfu.readFile(this->m_docName));
  this->replaceWholeFile(bytes);

  this->refreshModificationTime();

  if (sfu.isReadOnly(this->m_docName)) {
    this->setReadOnly(true);
  }
}


void NamedTextDocument::writeFile()
{
  xassert(this->hasFilename());

  SMFileUtil sfu;
  std::vector<unsigned char> bytes(this->getWholeFile());

  sfu.writeFile(this->m_docName, bytes);

  this->noUnsavedChanges();
  this->refreshModificationTime();
}


bool NamedTextDocument::getDiskModificationTime(int64_t &modTime) const
{
  bool ret = getFileModificationTime(this->m_docName.c_str(), modTime);
  TRACE("modtime", "on-disk ts for " << this->m_docName <<
                   " is " << modTime);
  return ret;
}


bool NamedTextDocument::hasStaleModificationTime() const
{
  if (!this->hasFilename()) {
    // The document is not actually associated with any file, the name
    // is just a placeholder.
    TRACE("modtime", "hasStale: returning false because isUntitled");
    return false;
  }

  int64_t diskTime;
  if (this->getDiskModificationTime(diskTime)) {
    bool ret = (diskTime != this->m_lastFileTimestamp);
    TRACE("modtime", "hasStale: returning " << ret);
    return ret;
  }
  else {
    // Failed to get time for on-disk file.  This is probably due
    // to the file having been removed, which we are about to resolve
    // by writing it again.  If the problem is a permission error,
    // the attempt to save will fail for and report that reason.
    //
    // Another way to get here is to start the editor with a command
    // line argument for a file that does not exist.
    //
    // In all cases, it should be safe to ignore the failure to get the
    // timestamp here and assume it is not stale.
    TRACE("modtime", "hasStale: returning false because getDiskModificationTime failed");
    return false;
  }
}


void NamedTextDocument::refreshModificationTime()
{
  TRACE("modtime", "refresh: old ts for " << this->m_docName <<
                   " is " << this->m_lastFileTimestamp);
  xassert(this->hasFilename());

  if (!this->getDiskModificationTime(this->m_lastFileTimestamp)) {
    // We ignore the error because we only
    // call this after we have already successfully read the file's
    // contents, so an error here is quite unlikely.  Furthermore, this
    // API does not provide a reason.  If it does fail, the timestamp
    // will be set to 0, which likely will agree with a subsequent call
    // since that would probably fail too, so at least we won't be
    // repeatedly bothering the user with spurious errors.
  }

  TRACE("modtime", "refresh: new ts for " << this->m_docName <<
                   " is " << this->m_lastFileTimestamp);
}


void NamedTextDocument::replaceFileAndStats(
  std::vector<unsigned char> const &contents,
  int64_t fileModificationTime,
  bool readOnly)
{
  this->replaceWholeFile(contents);
  this->m_lastFileTimestamp = fileModificationTime;
  this->setReadOnly(readOnly);
}


// EOF
