// cursor.cc
// code for cursor.h

#include "cursor.h"      // this module
#include "buffer.h"      // Buffer
#include "textline.h"    // TextLine
#include "xassert.h"     // xassert


Cursor::Cursor(Buffer *buf)
  : buffer(buf),
    _line(0),
    _col(0)
{}


Cursor::Cursor(Cursor const &obj)
  : DMEMB(buffer),
    DMEMB(_line),
    DMEMB(_col)
{}
  

Cursor::~Cursor()
{}


Cursor& Cursor::operator = (Cursor const &obj)
{
  xassert(buffer == obj.buffer);
  _line = obj._line;
  _col = obj._col;
  return *this;
}



bool Cursor::operator == (Cursor const &obj) const
{                           
  // I permit equality checks even between cursors
  // of different buffers..
  return EMEMB(buffer) &&
         EMEMB(_line) &&
         EMEMB(_col);
}


bool Cursor::operator < (Cursor const &obj) const
{
  // but relationals only make sense for same buffer
  xassert(buffer == buffer);                        
  
  return _line < obj._line ||
         (_line == obj._line && _col < obj._col);
         
}


void Cursor::set(int newLine, int newCol)
{
  // automatic limiting is very useful for making the
  // cursor movement code simple
  if (newLine < 0) {
    newLine = 0;
  }
  if (newCol < 0) {
    newCol = 0;
  }

  _line=newLine;
  _col=newCol;
}


void Cursor::setToEnd()
{
  set(buffer->totLines(),
      buffer->lastLineC()->getLength());
}


bool Cursor::beyondEnd() const
{
  Cursor end(buffer);
  end.setToEnd();
  return *this > end;
}
