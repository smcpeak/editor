// line-difference.cc
// Code for `line-difference` module.

#include "line-difference.h"           // this module

#include "smbase/compare-util.h"       // smbase::compare
#include "smbase/gdvalue.h"            // gdv::GDValue
#include "smbase/gdvalue-parser.h"     // gdv::GDValueParser
#include "smbase/stringb.h"            // stringb

#include <iostream>                    // std::ostream
#include <optional>                    // std::optional

using namespace gdv;
using namespace smbase;


int LineDifference::compareTo(LineDifference const &b) const
{
  return compare(m_value, b.m_value);
}


int LineDifference::compareTo(int const &b) const
{
  return compare(m_value, b);
}


LineDifference &LineDifference::operator++()
{
  ++m_value;
  return *this;
}

LineDifference LineDifference::operator++(int)
{
  LineDifference ret(*this);
  ++m_value;
  return ret;
}


LineDifference &LineDifference::operator--()
{
  --m_value;
  return *this;
}

LineDifference LineDifference::operator--(int)
{
  LineDifference ret(*this);
  --m_value;
  return ret;
}


LineDifference LineDifference::operator+(LineDifference delta) const
{
  return LineDifference(m_value + delta.get());
}

LineDifference LineDifference::operator+(int delta) const
{
  return LineDifference(m_value + delta);
}


LineDifference &LineDifference::operator+=(LineDifference delta)
{
  m_value += delta.get();
  return *this;
}

LineDifference &LineDifference::operator+=(int delta)
{
  m_value += delta;
  return *this;
}


LineDifference LineDifference::operator-(LineDifference delta) const
{
  return LineDifference(m_value - delta.get());
}

LineDifference LineDifference::operator-(int delta) const
{
  return LineDifference(m_value - delta);
}


LineDifference &LineDifference::operator-=(LineDifference delta)
{
  m_value -= delta.get();
  return *this;
}

LineDifference &LineDifference::operator-=(int delta)
{
  m_value -= delta;
  return *this;
}


LineDifference LineDifference::operator+() const
{
  return *this;
}

LineDifference LineDifference::operator-() const
{
  return LineDifference(-m_value);
}


void LineDifference::clampLower(int value)
{
  if (m_value < value) {
    m_value = value;
  }
}


LineDifference::operator gdv::GDValue() const
{
  return GDValue(m_value);
}

LineDifference::LineDifference(gdv::GDValueParser const &p)
{
  p.checkIsInteger();

  GDVInteger v = p.integerGet();

  if (std::optional<int> i = v.getAsOpt<int>()) {
    m_value = *i;
  }
  else {
    p.throwError(stringb("LineDifference value out of range: " << v << "."));
  }
}


void LineDifference::write(std::ostream &os) const
{
  os << m_value;
}


std::string toString(LineDifference const &obj)
{
  return stringb(obj);
}


// EOF
