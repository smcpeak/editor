// bufferstate.cc
// code for bufferstate.h

#include "bufferstate.h"      // this module
#include "macros.h"           // CMEMB

#include <qnamespace.h>       // Qt class/namespace


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


// EOF
