// td-line.cc
// Code for `td-line` module.

#include "td-line.h"                   // this module

#include "byte-count.h"                // memcmpBC, mkString

#include "smbase/gdvalue.h"            // gdv::GDValue
#include "smbase/xassert.h"            // xassertPrecondition

#include <cstring>                     // std::{memchr, memcmp}

using namespace gdv;


TextDocumentLine::TextDocumentLine(ByteCount length, char *bytes) :
  m_length(length.get()),
  m_bytes(bytes)
{
  xassertPrecondition( (length.isZero()) == (bytes == nullptr) );

  selfCheck();
}


void TextDocumentLine::selfCheck() const
{
  if (m_length) {
    xassert(m_bytes != nullptr);
    xassert(!std::memchr(m_bytes, '\n', m_length));
  }
  else {
    xassert(m_bytes == nullptr);
  }
}


bool TextDocumentLine::operator==(TextDocumentLine const &obj) const
{
  if (length() != obj.length()) {
    return false;
  }

  ByteCount len = length();
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
    return GDValue(mkString(m_bytes, length()));
  }
}


// EOF
