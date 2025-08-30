// positive-line-count.cc
// Code for `positive-line-count` module.

#include "positive-line-count.h"       // this module

#include "wrapped-integer.h"           // WrappedInteger method defns

#include "smbase/compare-util.h"       // smbase::compare, COMPARE_MEMBERS
#include "smbase/xassert.h"            // xassert

using namespace smbase;


// Explicitly instantiate the base class methods.
template class WrappedInteger<int, PositiveLineCount>;


// --------------------------- Binary tests ----------------------------
int PositiveLineCount::compareTo(LineDifference const &b) const
{
  return compare(m_value, b.get());
}


int PositiveLineCount::compareTo(LineCount const &b) const
{
  return compare(m_value, b.get());
}


// ----------------------------- Addition ------------------------------
PositiveLineCount PositiveLineCount::operator+(int delta) const
{
  return PositiveLineCount(get() + delta);
}


PositiveLineCount &PositiveLineCount::operator+=(int delta)
{
  set(get() + delta);
  return *this;
}


PositiveLineCount &PositiveLineCount::operator+=(LineCount delta)
{
  set(get() + delta.get());
  return *this;
}


// ----------------------- Subtraction/inversion -----------------------
LineDifference PositiveLineCount::operator-() const
{
  return LineDifference(-get());
}


LineDifference PositiveLineCount::operator-(PositiveLineCount delta) const
{
  return LineDifference(get() - delta.get());
}


LineDifference PositiveLineCount::operator-(int delta) const
{
  return LineDifference(get() - delta);
}


PositiveLineCount &PositiveLineCount::operator-=(LineDifference delta)
{
  set(get() - delta.get());
  return *this;
}


LineCount PositiveLineCount::pred() const
{
  return LineCount(get() - 1);
}


PositiveLineCount PositiveLineCount::predPLC() const
{
  return PositiveLineCount(get() - 1);
}


// --------------------------- Serialization ---------------------------
PositiveLineCount::PositiveLineCount(gdv::GDValueParser const &p)
  : Base(p)
{}


// EOF
