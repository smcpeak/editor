// position.cc
// code for position.h

#include "position.h"    // this module
#include "buffer.h"      // Buffer
#include "textline.h"    // TextLine
#include "xassert.h"     // xassert


Position::Position(Buffer *buf)
  : buffer(buf),
    _line(0),
    _col(0)
{}


Position::Position(Position const &obj)
  : DMEMB(buffer),
    DMEMB(_line),
    DMEMB(_col)
{}
  

Position::~Position()
{}


Position& Position::operator = (Position const &obj)
{
  xassert(buffer == obj.buffer);
  _line = obj._line;
  _col = obj._col;
  return *this;
}



bool Position::operator == (Position const &obj) const
{                           
  // I permit equality checks even between positions
  // of different buffers..
  return EMEMB(buffer) &&
         EMEMB(_line) &&
         EMEMB(_col);
}


bool Position::operator < (Position const &obj) const
{
  // but relationals only make sense for same buffer
  xassert(buffer == buffer);                        
  
  return _line < obj._line ||
         (_line == obj._line && _col < obj._col);
         
}


void Position::set(int newLine, int newCol)
{
  // automatic limiting is very useful for making the
  // position movement code simple
  if (newLine < 0) {
    newLine = 0;
  }
  if (newCol < 0) {
    newCol = 0;
  }

  _line=newLine;
  _col=newCol;
}


void Position::setToEnd()
{
  set(buffer->totLines(),
      buffer->lastLineC()->getLength());
}


bool Position::beyondEnd() const
{
  Position end(buffer);
  end.setToEnd();
  return *this > end;
}
