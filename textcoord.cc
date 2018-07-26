// textcoord.cc
// code for textcoord.h

#include "textcoord.h"                 // this module

#include "sm-swap.h"                   // swap


// ------------------------ TextCoord -----------------------------
TextCoord& TextCoord::operator= (TextCoord const &obj)
{
  CMEMB(m_line);
  CMEMB(m_column);
  return *this;
}


bool TextCoord::operator== (TextCoord const &obj) const
{
  return EMEMB(m_line) && EMEMB(m_column);
}


bool TextCoord::operator< (TextCoord const &obj) const
{
  if (this->m_line < obj.m_line) {
    return true;
  }
  else if (this->m_line == obj.m_line) {
    return this->m_column < obj.m_column;
  }
  else {
    return false;
  }
}


void TextCoord::insert(ostream &os) const
{
  os << this->m_line << ':' << this->m_column;
}

void TextCoord::insert(stringBuilder &sb) const
{
  sb << this->m_line << ':' << this->m_column;
}


// ------------------------ TextCoordRange -----------------------------
TextCoordRange& TextCoordRange::operator= (TextCoordRange const &obj)
{
  CMEMB(m_start);
  CMEMB(m_end);
  return *this;
}


bool TextCoordRange::operator== (TextCoordRange const &obj) const
{
  return EMEMB(m_start) && EMEMB(m_end);
}


void TextCoordRange::swapEnds()
{
  swap(m_start, m_end);
}


void TextCoordRange::insert(ostream &os) const
{
  os << this->m_start << '-' << this->m_end;
}

void TextCoordRange::insert(stringBuilder &sb) const
{
  sb << this->m_start << '-' << this->m_end;
}


// EOF
