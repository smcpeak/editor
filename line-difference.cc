// line-difference.cc
// Code for `line-difference` module.

#include "line-difference.h"           // this module

#include "clampable-wrapped-integer.h" // ClampableInteger method defns
#include "wrapped-integer.h"           // WrappedInteger method defns

#include "smbase/stringb.h"            // stringb


// Explicitly instantiate all of the class template methods.  The header
// file for this module deliberately does not expose them, so they won't
// be implicitly instantiated.
template class WrappedInteger<int, LineDifference>;
template class ClampableInteger<int, LineDifference>;


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


std::string toString(LineDifference obj)
{
  return stringb(obj);
}


// EOF
