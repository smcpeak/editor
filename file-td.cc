// file-td.cc
// code for file-td.h

#include "file-td.h"                   // this module

// smbase
#include "macros.h"                    // CMEMB
#include "nonport.h"                   // getFileModificationTime
#include "objcount.h"                  // CHECK_OBJECT_COUNT
#include "trace.h"                     // TRACE

// Qt
#include <qnamespace.h>                // Qt class/namespace


// Do not start with 0 because QVariant::toInt() returns 0 to
// indicate failure.
int FileTextDocument::s_nextWindowMenuId = 1;

int FileTextDocument::s_objectCount = 0;

CHECK_OBJECT_COUNT(FileTextDocument);


FileTextDocument::FileTextDocument()
  : TextDocument(),
    m_hasHotkeyDigit(false),
    m_hotkeyDigit(0),
    m_name(),
    m_hasFilename(false),
    m_lastFileTimestamp(0),
    m_title(),
    m_windowMenuId(s_nextWindowMenuId++),
    m_highlighter(NULL),
    m_highlightTrailingWhitespace(true)
{
  FileTextDocument::s_objectCount++;
}

FileTextDocument::~FileTextDocument()
{
  FileTextDocument::s_objectCount--;
  if (m_highlighter) {
    delete m_highlighter;
  }
}


void FileTextDocument::setDocumentProcessStatus(DocumentProcessStatus status)
{
  this->TextDocument::setDocumentProcessStatus(status);

  if (this->isProcessOutput()) {
    this->m_highlightTrailingWhitespace = false;
  }
}


string FileTextDocument::filename() const
{
  xassert(this->hasFilename());
  return m_name;
}


void FileTextDocument::setFilename(string const &filename)
{
  m_name = filename;
  m_hasFilename = true;
}


void FileTextDocument::setNonFileName(string const &name)
{
  m_name = name;
  m_hasFilename = false;
}


int FileTextDocument::getHotkeyDigit() const
{
  xassert(this->hasHotkey());
  return this->m_hotkeyDigit;
}


string FileTextDocument::hotkeyDesc() const
{
  if (!this->hasHotkey()) {
    return "";
  }

  return stringc << "Alt+" << this->getHotkeyDigit();
}


void FileTextDocument::clearHotkey()
{
  this->m_hasHotkeyDigit = false;
  this->m_hotkeyDigit = 0;
}


void FileTextDocument::setHotkeyDigit(int digit)
{
  xassert(0 <= digit && digit <= 9);
  this->m_hasHotkeyDigit = true;
  this->m_hotkeyDigit = digit;
}


void FileTextDocument::readFile()
{
  xassert(this->hasFilename());
  this->TextDocument::readFile(this->m_name);
  this->refreshModificationTime();
}


void FileTextDocument::writeFile()
{
  xassert(this->hasFilename());
  this->TextDocument::writeFile(this->m_name);
  this->noUnsavedChanges();
  this->refreshModificationTime();
}


bool FileTextDocument::getDiskModificationTime(int64_t &modTime) const
{
  bool ret = getFileModificationTime(this->m_name.c_str(), modTime);
  TRACE("modtime", "on-disk ts for " << this->m_name <<
                   " is " << modTime);
  return ret;
}


bool FileTextDocument::hasStaleModificationTime() const
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


void FileTextDocument::refreshModificationTime()
{
  TRACE("modtime", "refresh: old ts for " << this->m_name <<
                   " is " << this->m_lastFileTimestamp);

  if (!this->getDiskModificationTime(this->m_lastFileTimestamp)) {
    // We ignore the error because we only
    // call this after we have already successfully read the file's
    // contents, so an error here is quite unlikely.  Furthermore, this
    // API does not provide a reason.  If it does fail, the timestamp
    // will be set to 0, which likely will agree with a subsequent call
    // since that would probably fail too, so at least we won't be
    // repeatedly bothering the user with spurious errors.
  }

  TRACE("modtime", "refresh: new ts for " << this->m_name <<
                   " is " << this->m_lastFileTimestamp);
}


// EOF
