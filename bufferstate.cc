// bufferstate.cc
// code for bufferstate.h

#include "bufferstate.h"      // this module


BufferState::BufferState()
  : Buffer(),
    filename(),
    highlighter(NULL)
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