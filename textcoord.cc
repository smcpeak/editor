// textcoord.cc
// code for textcoord.h

#include "textcoord.h"                 // this module

#include "sm-swap.h"                   // swap


// ------------------------ TextCoord -----------------------------
TextCoord& TextCoord::operator= (TextCoord const &obj)
{
  CMEMB(line);
  CMEMB(column);
  return *this;
}


bool TextCoord::operator== (TextCoord const &obj) const
{
  return EMEMB(line) && EMEMB(column);
}


bool TextCoord::operator< (TextCoord const &obj) const
{
  if (this->line < obj.line) {
    return true;
  }
  else if (this->line == obj.line) {
    return this->column < obj.column;
  }
  else {
    return false;
  }
}


void TextCoord::insert(ostream &os) const
{
  os << this->line << ':' << this->column;
}

void TextCoord::insert(stringBuilder &sb) const
{
  sb << this->line << ':' << this->column;
}


// ------------------------ TextCoordRange -----------------------------
TextCoordRange& TextCoordRange::operator= (TextCoordRange const &obj)
{
  CMEMB(start);
  CMEMB(end);
  return *this;
}


bool TextCoordRange::operator== (TextCoordRange const &obj) const
{
  return EMEMB(start) && EMEMB(end);
}


void TextCoordRange::swapEnds()
{
  swap(start, end);
}


void TextCoordRange::insert(ostream &os) const
{
  os << this->start << '-' << this->end;
}

void TextCoordRange::insert(stringBuilder &sb) const
{
  sb << this->start << '-' << this->end;
}


// EOF
