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


TextLine *Position::getBufLine() const 
{ 
  return buffer->getLine(line()); 
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


bool Position::beyondEnd() const
{
  Position end(buffer);
  end.setToEnd();
  return *this > end;
}


void Position::setToEnd()
{
  set(buffer->totLines(),
      buffer->lastLineC()->getLength());
}


bool Position::beyondLineEnd() const
{
  return col() > buffer->getLineC(line())->getLength();
}

void Position::setToLineEnd()
{
  _col = buffer->getLineC(line())->getLength();
}


bool Position::inText() const
{
  return !beyondEnd() && !beyondLineEnd();
}

void Position::clampToText()
{              
  if (beyondEnd()) {
    setToEnd();
  }
  else if (beyondLineEnd()) {
    setToLineEnd();
  }
}


void Position::moveLeftWrap()
{
  if (col() == 0) {
    // move to end of previous line
    if (line() == 0) {
      // nothing to do
    }
    else {
      set(line()-1, 0);
      setToLineEnd();
    }
  }
  else {
    // move one char left in current line
    move(0,-1);
  }
}

void Position::moveRightWrap()
{
  if (col() >= getBufLine()->getLength()) {
    // move to beginning of next line
    set(line()+1, 0);
  }
  else {
    // move one char right in current line
    move(0,+1);
  }
}
