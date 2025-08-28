// line-difference.cc
// Code for `line-difference` module.

#include "line-difference.h"           // this module

#include "wrapped-integer.h"           // WrappedInteger method defns

#include "smbase/compare-util.h"       // smbase::compare
#include "smbase/gdvalue.h"            // gdv::GDValue
#include "smbase/gdvalue-parser.h"     // gdv::GDValueParser
#include "smbase/stringb.h"            // stringb

#include <iostream>                    // std::ostream
#include <optional>                    // std::optional

using namespace gdv;
using namespace smbase;


// Explicitly instantiate all of the class template methods.  The header
// file for this module deliberately does not expose them, so they won't
// be implicitly instantiated.
template class WrappedInteger<LineDifference>;


LineDifference LineDifference::operator+(int delta) const
{
  return LineDifference(m_value + delta);
}


LineDifference &LineDifference::operator+=(int delta)
{
  m_value += delta;
  return *this;
}


LineDifference LineDifference::operator-(int delta) const
{
  return LineDifference(m_value - delta);
}


LineDifference &LineDifference::operator-=(int delta)
{
  m_value -= delta;
  return *this;
}


void LineDifference::clampLower(int value)
{
  if (m_value < value) {
    m_value = value;
  }
}


std::string toString(LineDifference const &obj)
{
  return stringb(obj);
}


// EOF
