// td-line.cc
// Code for `td-line` module.

#include "td-line.h"                   // this module

#include "smbase/gdvalue.h"            // gdv::GDValue

using namespace gdv;


TextDocumentLine::operator gdv::GDValue() const
{
  if (isEmpty()) {
    return GDValue(GDVString());
  }
  else {
    return GDValue(GDVString(m_bytes, lengthWithoutNL()));
  }
}


// EOF
