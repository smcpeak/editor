// td-line.cc
// Code for `td-line` module.

#include "td-line.h"                   // this module

#include "byte-count.h"                // memcmpBC, mkString

#include "smbase/gdvalue.h"            // gdv::GDValue

#include <cstring>                     // std::memcmp

using namespace gdv;


bool TextDocumentLine::operator==(TextDocumentLine const &obj) const
{
  if (lengthWithoutNL() != obj.lengthWithoutNL()) {
    return false;
  }

  ByteCount len = lengthWithoutNL();
  if (len) {
    return 0==memcmpBC(m_bytes, obj.m_bytes, len);
  }
  else {
    return true;
  }
}


TextDocumentLine::operator gdv::GDValue() const
{
  if (isEmpty()) {
    return GDValue(GDVString());
  }
  else {
    return GDValue(mkString(m_bytes, lengthWithoutNL()));
  }
}


// EOF
