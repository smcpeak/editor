// file-td.cc
// code for file-td.h

#include "file-td.h"                   // this module

#include "macros.h"                    // CMEMB
#include "nonport.h"                   // getFileModificationTime
#include "trace.h"                     // TRACE

#include <qnamespace.h>                // Qt class/namespace


// Do not start with 0 because QVariant::toInt() returns 0 to
// indicate failure.
int FileTextDocument::nextWindowMenuId = 1;

int FileTextDocument::objectCount = 0;

FileTextDocument::FileTextDocument()
  : TextDocument(),
    hasHotkeyDigit(false),
    hotkeyDigit(0),
    filename(),
    isUntitled(true),
    lastFileTimestamp(0),
    title(),
    windowMenuId(nextWindowMenuId++),
    //changed(false),
    highlighter(NULL)
{
  FileTextDocument::objectCount++;
}

FileTextDocument::~FileTextDocument()
{
  FileTextDocument::objectCount--;
  if (highlighter) {
    delete highlighter;
  }
}


int FileTextDocument::getHotkeyDigit() const
{
  xassert(this->hasHotkey());
  return this->hotkeyDigit;
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
  this->hasHotkeyDigit = false;
  this->hotkeyDigit = 0;
}


void FileTextDocument::setHotkeyDigit(int digit)
{
  xassert(0 <= digit && digit <= 9);
  this->hasHotkeyDigit = true;
  this->hotkeyDigit = digit;
}


void FileTextDocument::readFile()
{
  xassert(!this->isUntitled);
  this->TextDocument::readFile(this->filename);
  this->refreshModificationTime();
}


void FileTextDocument::writeFile()
{
  xassert(!this->isUntitled);
  this->TextDocument::writeFile(this->filename);
  this->noUnsavedChanges();
  this->refreshModificationTime();
}


bool FileTextDocument::getDiskModificationTime(int64_t &modTime) const
{
  bool ret = getFileModificationTime(this->filename.c_str(), modTime);
  TRACE("modtime", "on-disk ts for " << this->filename <<
                   " is " << modTime);
  return ret;
}


bool FileTextDocument::hasStaleModificationTime() const
{
  if (this->isUntitled) {
    // The document is not actually associated with any file, the name
    // is just a placeholder.
    TRACE("modtime", "hasStale: returning false because isUntitled");
    return false;
  }

  int64_t diskTime;
  if (this->getDiskModificationTime(diskTime)) {
    bool ret = (diskTime != this->lastFileTimestamp);
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
  TRACE("modtime", "refresh: old ts for " << this->filename <<
                   " is " << this->lastFileTimestamp);

  if (!this->getDiskModificationTime(this->lastFileTimestamp)) {
    // We ignore the error because we only
    // call this after we have already successfully read the file's
    // contents, so an error here is quite unlikely.  Furthermore, this
    // API does not provide a reason.  If it does fail, the timestamp
    // will be set to 0, which likely will agree with a subsequent call
    // since that would probably fail too, so at least we won't be
    // repeatedly bothering the user with spurious errors.
  }

  TRACE("modtime", "refresh: new ts for " << this->filename <<
                   " is " << this->lastFileTimestamp);
}


// EOF
