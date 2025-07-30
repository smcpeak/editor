// textlcoord.cc
// code for textlcoord.h

#include "textlcoord.h"                // this module

#include "smbase/sm-swap.h"            // swap


// ------------------------ TextLCoord -----------------------------
TextLCoord& TextLCoord::operator= (TextLCoord const &obj)
{
  CMEMB(m_line);
  CMEMB(m_column);
  return *this;
}


bool TextLCoord::operator== (TextLCoord const &obj) const
{
  return EMEMB(m_line) && EMEMB(m_column);
}


bool TextLCoord::operator< (TextLCoord const &obj) const
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


void TextLCoord::insert(std::ostream &os) const
{
  os << this->m_line << ':' << this->m_column;
}


// ------------------------ TextLCoordRange -----------------------------
TextLCoordRange& TextLCoordRange::operator= (TextLCoordRange const &obj)
{
  CMEMB(m_start);
  CMEMB(m_end);
  return *this;
}


bool TextLCoordRange::operator== (TextLCoordRange const &obj) const
{
  return EMEMB(m_start) && EMEMB(m_end);
}


void TextLCoordRange::swapEnds()
{
  swap(m_start, m_end);
}


void TextLCoordRange::insert(std::ostream &os) const
{
  os << this->m_start << '-' << this->m_end;
}


// EOF
