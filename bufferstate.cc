// bufferstate.cc
// code for bufferstate.h

#include "bufferstate.h"      // this module
#include "macros.h"           // CMEMB

#include <qnamespace.h>       // Qt class/namespace


// ----------------------- EditingState --------------------------
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
/*static*/ int BufferState::nextWindowMenuId = 1;

BufferState::BufferState()
  : Buffer(),
    filename(),
    title(),
    windowMenuId(nextWindowMenuId++),
    hotkeyDigit(0),
    //changed(false),
    highlighter(NULL),
    savedState()
{}

BufferState::~BufferState()
{
  if (highlighter) {
    delete highlighter;
  }
}


string BufferState::hotkeyDesc()
{
  if (this->hotkeyDigit == 0) {
    return "";
  }

  return stringc << "Alt+" << this->hotkeyDigit;
}


// EOF
