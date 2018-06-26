// td-file.cc
// code for td-file.h

#include "td-file.h"                   // this module

#include "macros.h"                    // CMEMB
#include "nonport.h"                   // getFileModificationTime
#include "trace.h"                     // TRACE

#include <qnamespace.h>                // Qt class/namespace


// Do not start with 0 because QVariant::toInt() returns 0 to
// indicate failure.
int TextDocumentFile::nextWindowMenuId = 1;

int TextDocumentFile::objectCount = 0;

TextDocumentFile::TextDocumentFile()
  : TextDocument(),
    hasHotkeyDigit(false),
    hotkeyDigit(0),
    filename(),
    lastFileTimestamp(0),
    title(),
    windowMenuId(nextWindowMenuId++),
    //changed(false),
    highlighter(NULL)
{
  TextDocumentFile::objectCount++;
}

TextDocumentFile::~TextDocumentFile()
{
  TextDocumentFile::objectCount--;
  if (highlighter) {
    delete highlighter;
  }
}


int TextDocumentFile::getHotkeyDigit() const
{
  xassert(this->hasHotkey());
  return this->hotkeyDigit;
}


string TextDocumentFile::hotkeyDesc() const
{
  if (!this->hasHotkey()) {
    return "";
  }

  return stringc << "Alt+" << this->getHotkeyDigit();
}


void TextDocumentFile::clearHotkey()
{
  this->hasHotkeyDigit = false;
  this->hotkeyDigit = 0;
}


void TextDocumentFile::setHotkeyDigit(int digit)
{
  xassert(0 <= digit && digit <= 9);
  this->hasHotkeyDigit = true;
  this->hotkeyDigit = digit;
}


bool TextDocumentFile::getDiskModificationTime(int64_t &modTime) const
{
  bool ret = getFileModificationTime(this->filename.c_str(), modTime);
  TRACE("modtime", "on-disk ts for " << this->filename <<
                   " is " << modTime);
  return ret;
}


bool TextDocumentFile::hasStaleModificationTime() const
{
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
    // Either way, it should be safe to ignore the failure to get the
    // timestamp here and assume it is not stale.
    return false;
  }
}


void TextDocumentFile::refreshModificationTime()
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
