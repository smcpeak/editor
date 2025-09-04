// byte-index.cc
// Code for `byte-index` module.

#include "byte-index.h"                // this module

#include "byte-count.h"                // ByteCount
#include "byte-difference.h"           // ByteDifference
#include "wrapped-integer.h"           // WrappedInteger method defns

#include "smbase/compare-util.h"       // smbase::compare
#include "smbase/overflow.h"           // convertNumber, {add,subtract}WithOverflowCheck

#include <string>                      // std::string

using namespace smbase;


// Instantiate base class methods.
template class WrappedInteger<int, ByteIndex>;


// ---------------------------- Conversion -----------------------------
ByteIndex::ByteIndex(std::ptrdiff_t size)
  : Base(convertNumber<int>(size))
{}


ByteIndex::ByteIndex(std::size_t size)
  : Base(convertNumber<int>(size))
{}


ByteIndex::ByteIndex(ByteCount count)
  : Base(count.get())
{}


ByteIndex::ByteIndex(ByteDifference delta)
  : Base(delta.get())
{}


ByteIndex::operator ByteCount() const
{
  return ByteCount(get());
}


ByteIndex::operator ByteDifference() const
{
  return ByteDifference(get());
}


// ----------------------------- Addition ------------------------------
ByteIndex ByteIndex::operator+(ByteDifference delta) const
{
  return ByteIndex(addWithOverflowCheck(get(), delta.get()));
}


ByteIndex &ByteIndex::operator+=(ByteDifference delta)
{
  set(addWithOverflowCheck(get(), delta.get()));
  return *this;
}


// ----------------------- Subtraction/inversion -----------------------
ByteDifference ByteIndex::operator-() const
{
  // This cannot overflow because `get()` is non-negative.
  return ByteDifference(-get());
}


ByteDifference ByteIndex::operator-(ByteIndex count) const
{
  return ByteDifference(subtractWithOverflowCheck(get(), count.get()));
}


ByteIndex ByteIndex::operator-(ByteDifference delta) const
{
  return ByteIndex(subtractWithOverflowCheck(get(), delta.get()));
}


ByteIndex &ByteIndex::operator-=(ByteDifference delta)
{
  set(subtractWithOverflowCheck(get(), delta.get()));
  return *this;
}


void ByteIndex::clampDecrease(
  ByteDifference delta, ByteIndex lowerLimit)
{
  int newValue = subtractWithOverflowCheck(get(), delta.get());
  if (newValue >= lowerLimit.get()) {
    set(newValue);
  }
  else {
    set(lowerLimit.get());
  }
}


// ------------------------- string functions --------------------------
char const &at(std::string const &str, ByteIndex index)
{
  return str.at(index.get());
}


std::string substr(
  std::string const &str, ByteIndex start, ByteCount count)
{
  return str.substr(start.get(), count.get());
}


// EOF
