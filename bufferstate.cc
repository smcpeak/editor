// bufferstate.cc
// code for bufferstate.h

#include "bufferstate.h"      // this module
#include "macros.h"           // CMEMB


// ----------------------- EditingState --------------------------
EditingState::EditingState()
  : cursorLine(0),
    cursorCol(0),
    selectLine(0),
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
  CMEMB(cursorLine);
  CMEMB(cursorCol);
  CMEMB(selectLine);
  CMEMB(selectCol);
  CMEMB(selectEnabled);
  CMEMB(selLowLine);
  CMEMB(selLowCol);
  CMEMB(selHighLine);
  CMEMB(selHighCol);
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


bool EditingState::cursorBeforeSelect() const
{
  if (cursorLine < selectLine) return true;
  if (cursorLine > selectLine) return false;
  return cursorCol < selectCol;
}


void EditingState::normalizeSelect()
{
  if (cursorBeforeSelect()) {
    selLowLine = cursorLine;
    selLowCol = cursorCol;
    selHighLine = selectLine;
    selHighCol = selectCol;
  }
  else {
    selLowLine = selectLine;
    selLowCol = selectCol;
    selHighLine = cursorLine;
    selHighCol = cursorCol;
  }
}


// ----------------------- BufferState ---------------------------
BufferState::BufferState()
  : Buffer(),
    filename(),
    changed(false),
    highlighter(NULL),
    savedState()
{}

BufferState::~BufferState()
{
  if (highlighter) {
    delete highlighter;
  }
}


void BufferState::clear()
{
  while (numLines() > 0) {
    deleteText(0,0, lineLength(0));
    deleteLine(0);
  }
  
  // always keep one empty line
  insertLine(0);
}
