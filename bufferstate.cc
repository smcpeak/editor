// bufferstate.cc
// code for bufferstate.h

#include "bufferstate.h"      // this module
#include "macros.h"           // CMEMB


// ----------------------- EditingState --------------------------
EditingState::EditingState()
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

EditingState::~EditingState()
{}


void EditingState::copy(EditingState const &obj)
{
  CMEMB(selectLine);
  CMEMB(selectCol);
  CMEMB(selectEnabled);
  setFirstVisibleLC(obj.firstVisibleLine, obj.firstVisibleCol);
  CMEMB(hitText);
  CMEMB(hitTextFlags);
}


void EditingState::setFirstVisibleLC(int newFirstLine, int newFirstCol)
{
  // this is the one function allowed to change these
  const_cast<int&>(firstVisibleLine) = newFirstLine;
  const_cast<int&>(firstVisibleCol) = newFirstCol;
}


// ----------------------- BufferState ---------------------------
BufferState::BufferState()
  : Buffer(),
    filename(),
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
