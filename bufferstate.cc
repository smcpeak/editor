// bufferstate.cc
// code for bufferstate.h

#include "bufferstate.h"               // this module

#include "macros.h"                    // CMEMB
#include "nonport.h"                   // getFileModificationTime

#include <qnamespace.h>                // Qt class/namespace


// ----------------------- SavedEditingState --------------------------
SavedEditingState::SavedEditingState()
  : selectLine(0),
    selectCol(0),
    selectEnabled(false),
    // selLow, etc. not valid until normalizeSelect()
    firstVisibleLine(0),
    firstVisibleCol(0),
    lastVisibleLine(0),
    lastVisibleCol(0),
    hitText(),                      // empty string, no hit highlighting
    hitTextFlags(Buffer::FS_NONE)
{}

SavedEditingState::~SavedEditingState()
{}


void SavedEditingState::copySavedEditingState(SavedEditingState const &obj)
{
  CMEMB(selectLine);
  CMEMB(selectCol);
  CMEMB(selectEnabled);
  setFirstVisibleLC(obj.firstVisibleLine, obj.firstVisibleCol);
  CMEMB(hitText);
  CMEMB(hitTextFlags);
}


void SavedEditingState::setFirstVisibleLC(int newFirstLine, int newFirstCol)
{
  // this is the one function allowed to change these
  const_cast<int&>(firstVisibleLine) = newFirstLine;
  const_cast<int&>(firstVisibleCol) = newFirstCol;
}


// ----------------------- BufferState ---------------------------
// Do not start with 0 because QVariant::toInt() returns 0 to
// indicate failure.
int BufferState::nextWindowMenuId = 1;

int BufferState::objectCount = 0;

BufferState::BufferState()
  : Buffer(),
    hasHotkeyDigit(false),
    hotkeyDigit(0),
    filename(),
    lastFileTimestamp(0),
    title(),
    windowMenuId(nextWindowMenuId++),
    //changed(false),
    highlighter(NULL),
    savedState()
{
  BufferState::objectCount++;
}

BufferState::~BufferState()
{
  BufferState::objectCount--;
  if (highlighter) {
    delete highlighter;
  }
}


int BufferState::getHotkeyDigit() const
{
  xassert(this->hasHotkey());
  return this->hotkeyDigit;
}


string BufferState::hotkeyDesc() const
{
  if (!this->hasHotkey()) {
    return "";
  }

  return stringc << "Alt+" << this->getHotkeyDigit();
}


void BufferState::clearHotkey()
{
  this->hasHotkeyDigit = false;
  this->hotkeyDigit = 0;
}


void BufferState::setHotkeyDigit(int digit)
{
  xassert(0 <= digit && digit <= 9);
  this->hasHotkeyDigit = true;
  this->hotkeyDigit = digit;
}


bool BufferState::getDiskModificationTime(int64_t &modTime)
{
  return getFileModificationTime(this->filename.c_str(), modTime);
}


void BufferState::refreshModificationTime()
{
  // We ignore the return when calling this function because we only
  // call this after we have already successfully read the file's
  // contents, so an error here is quite unlikely.  Furthermore, this
  // API does not provide a reason.  If it does fail, the timestamp
  // will be set to 0, which likely will agree with a subsequent call
  // since that would probably fail too, so at least we won't be
  // repeatedly bothering the user with spurious errors.
  (void)this->getDiskModificationTime(this->lastFileTimestamp);
}


// EOF
