// textmcoord.cc
// code for textmcoord.h

#include "textmcoord.h"                // this module

#include "smbase/gdvalue.h"            // gdv::GDValue
#include "smbase/sm-swap.h"            // swap


using namespace gdv;


// ------------------------ TextMCoord -----------------------------
TextMCoord& TextMCoord::operator= (TextMCoord const &obj)
{
  CMEMB(m_line);
  CMEMB(m_byteIndex);
  return *this;
}


bool TextMCoord::operator== (TextMCoord const &obj) const
{
  return EMEMB(m_line) && EMEMB(m_byteIndex);
}


bool TextMCoord::operator< (TextMCoord const &obj) const
{
  if (this->m_line < obj.m_line) {
    return true;
  }
  else if (this->m_line == obj.m_line) {
    return this->m_byteIndex < obj.m_byteIndex;
  }
  else {
    return false;
  }
}


void TextMCoord::insert(ostream &os) const
{
  os << this->m_line << ':' << this->m_byteIndex;
}

void TextMCoord::insert(stringBuilder &sb) const
{
  sb << this->m_line << ':' << this->m_byteIndex;
}


TextMCoord::operator GDValue() const
{
  return GDVTaggedTuple("MC"_sym, {
    toGDValue(m_line),
    toGDValue(m_byteIndex)
  });
}


// ------------------------ TextMCoordRange -----------------------------
TextMCoordRange& TextMCoordRange::operator= (TextMCoordRange const &obj)
{
  CMEMB(m_start);
  CMEMB(m_end);
  return *this;
}


bool TextMCoordRange::operator== (TextMCoordRange const &obj) const
{
  return EMEMB(m_start) && EMEMB(m_end);
}


void TextMCoordRange::swapEnds()
{
  swap(m_start, m_end);
}


void TextMCoordRange::insert(ostream &os) const
{
  os << this->m_start << '-' << this->m_end;
}

void TextMCoordRange::insert(stringBuilder &sb) const
{
  sb << this->m_start << '-' << this->m_end;
}


TextMCoordRange::operator GDValue() const
{
  return GDVTaggedTuple("MCR"_sym, {
    toGDValue(m_start),
    toGDValue(m_end)
  });
}


// EOF
