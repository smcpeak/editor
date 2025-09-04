// byte-count.cc
// Code for `byte-count` module.

#include "byte-count.h"                // this module

#include "byte-difference.h"           // ByteDifference
#include "wrapped-integer.h"           // WrappedInteger method defns

#include "smbase/compare-util.h"       // smbase::compare
#include "smbase/overflow.h"           // convertNumber, {add,subtract}WithOverflowCheck

#include <cstring>                     // std::{memchr, memcmp, memcpy, strlen}
#include <string>                      // std::string

using namespace smbase;


// Instantiate the base class.
template class WrappedInteger<int, ByteCount>;


// ---------------------------- Conversion -----------------------------
ByteCount::ByteCount(std::ptrdiff_t size)
  : Base(convertNumber<int>(size))
{}


ByteCount::ByteCount(std::size_t size)
  : Base(convertNumber<int>(size))
{}


ByteCount::ByteCount(ByteDifference delta)
  : Base(delta.get())
{}


ByteCount::operator ByteDifference() const
{
  return ByteDifference(get());
}


// ----------------------------- Addition ------------------------------
ByteCount ByteCount::operator+(ByteDifference delta) const
{
  return ByteCount(addWithOverflowCheck(get(), delta.get()));
}


ByteCount &ByteCount::operator+=(ByteDifference delta)
{
  set(addWithOverflowCheck(get(), delta.get()));
  return *this;
}


// ----------------------- Subtraction/inversion -----------------------
ByteDifference ByteCount::operator-() const
{
  // This cannot overflow because `get()` is non-negative.
  return ByteDifference(-get());
}


ByteDifference ByteCount::operator-(ByteCount count) const
{
  return ByteDifference(subtractWithOverflowCheck(get(), count.get()));
}


ByteCount ByteCount::operator-(ByteDifference delta) const
{
  return ByteCount(subtractWithOverflowCheck(get(), delta.get()));
}


ByteCount &ByteCount::operator-=(ByteDifference delta)
{
  set(subtractWithOverflowCheck(get(), delta.get()));
  return *this;
}


// ------------------------------ strings ------------------------------
ByteCount strlenBC(char const *str)
{
  return ByteCount(std::strlen(str));
}


void const *memchrBC(void const *p, char c, ByteCount length)
{
  return std::memchr(p, c, length.get());
}


int memcmpBC(void const *a, void const *b, ByteCount length)
{
  return std::memcmp(a, b, length.get());
}


void memcpyBC(char *dest, char const *src, ByteCount length)
{
  std::memcpy(dest, src, length.get());
}


ByteCount sizeBC(std::string const &str)
{
  return ByteCount(str.size());
}


std::string stringBC(char const *text, ByteCount length)
{
  return std::string(text, length.get());
}


// EOF
