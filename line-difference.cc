// line-difference.cc
// Code for `line-difference` module.

#include "line-difference.h"           // this module

#include "wrapped-integer.h"           // WrappedInteger method defns

#include "smbase/stringb.h"            // stringb


// Explicitly instantiate all of the class template methods.  The header
// file for this module deliberately does not expose them, so they won't
// be implicitly instantiated.
template class WrappedInteger<int, LineDifference>;


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


std::string toString(LineDifference obj)
{
  return stringb(obj);
}


// EOF
