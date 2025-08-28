// wrapped-integer-test.cc
// Tests for `wrapped-integer` module.

#include "unit-tests.h"                // decl for my entry point
#include "wrapped-integer.h"           // module under test

#include "smbase/exc.h"                // smbase::XAssert
#include "smbase/gdvalue-parser.h"     // gdv::{GDValueParser, XGDValueError}
#include "smbase/gdvalue.h"            // gdv::GDValue
#include "smbase/sm-macros.h"          // OPEN_ANONYMOUS_NAMESPACE
#include "smbase/sm-test-order.h"      // EXPECT_STRICTLY_ORDERED
#include "smbase/sm-test.h"            // EXPECT_EQ
#include "smbase/stringb.h"            // stringb

using namespace gdv;
using namespace smbase;


OPEN_ANONYMOUS_NAMESPACE


// Wrapped integer that is never negative.
class NonNegativeInteger : public WrappedInteger<NonNegativeInteger> {
public:      // types
  using Base = WrappedInteger<NonNegativeInteger>;
  friend Base;

protected:   // methods
  static bool isValid(int value)
    { return value >= 0; }

  static char const *getTypeName()
    { return "NonNegativeInteger"; }

public:      // methods
  // Inherit ctors.
  using Base::Base;
};


void test_ctor()
{
  TEST_CASE("test_ctor");

  NonNegativeInteger d0;
  EXPECT_EQ(d0.get(), 0);

  {
    NonNegativeInteger d0b(0);
    EXPECT_EQ(d0b.get(), 0);
  }

  {
    NonNegativeInteger d0Copy(d0);
    EXPECT_EQ(d0Copy.get(), 0);
  }

  NonNegativeInteger d1(1);
  EXPECT_EQ(d1.get(), 1);

  {
    NonNegativeInteger d1Copy(d1);
    EXPECT_EQ(d1Copy.get(), 1);
  }

  NonNegativeInteger d2(2);
  EXPECT_EQ(d2.get(), 2);

  {
    NonNegativeInteger d2Copy(d2);
    EXPECT_EQ(d2Copy.get(), 2);
  }

  EXPECT_STRICTLY_ORDERED(NonNegativeInteger, d0, d1, d2);
}


void test_assignment()
{
  TEST_CASE("test_assignment");

  NonNegativeInteger d1(5);
  NonNegativeInteger d2;
  d2 = d1;
  EXPECT_EQ(d2.get(), 5);

  // Self-assignment, slightly obfuscated to avoid compiler warning.
  d2 = *&d2;
  EXPECT_EQ(d2.get(), 5);
}


void test_set_get()
{
  TEST_CASE("test_set_get");

  NonNegativeInteger d;
  d.set(42);
  EXPECT_EQ(d.get(), 42);

  EXPECT_EXN_SUBSTR(d.set(-7),
    XAssert, "Value violates constraint for NonNegativeInteger: -7.");
}


void test_bool_conversion()
{
  TEST_CASE("test_bool_conversion");

  NonNegativeInteger d0;
  EXPECT_FALSE(static_cast<bool>(d0));

  NonNegativeInteger d1(1);
  EXPECT_TRUE(static_cast<bool>(d1));
}


void test_increment_decrement()
{
  TEST_CASE("test_increment_decrement");

  NonNegativeInteger d(5);

  // Prefix ++
  NonNegativeInteger &pref = ++d;
  EXPECT_EQ(d.get(), 6);
  EXPECT_EQ(pref.get(), 6);

  // Postfix ++
  NonNegativeInteger old = d++;
  EXPECT_EQ(old.get(), 6);
  EXPECT_EQ(d.get(), 7);

  // Prefix --
  NonNegativeInteger &pref2 = --d;
  EXPECT_EQ(d.get(), 6);
  EXPECT_EQ(pref2.get(), 6);

  // Postfix --
  NonNegativeInteger old2 = d--;
  EXPECT_EQ(old2.get(), 6);
  EXPECT_EQ(d.get(), 5);
}


void test_arithmetic()
{
  TEST_CASE("test_arithmetic");

  NonNegativeInteger d1(10);
  NonNegativeInteger d2(3);

  EXPECT_EQ((d1 + d2).get(), 13);

  NonNegativeInteger d3(7);
  d3 += d2;
  EXPECT_EQ(d3.get(), 10);

  EXPECT_EQ((d1 - d2).get(), 7);

  NonNegativeInteger d4(20);
  d4 -= d2;
  EXPECT_EQ(d4.get(), 17);
  d4 -= NonNegativeInteger(10);
  EXPECT_EQ(d4.get(), 7);
}


void test_comparisons()
{
  TEST_CASE("test_comparisons");

  NonNegativeInteger d1(5);
  NonNegativeInteger d2(7);
  NonNegativeInteger d3(5);

  EXPECT_TRUE(d1 == d3);
  EXPECT_FALSE(d1 == d2);

  EXPECT_TRUE(d1 != d2);
  EXPECT_FALSE(d1 != d3);

  EXPECT_TRUE(d1 < d2);
  EXPECT_TRUE(d2 > d1);
  EXPECT_TRUE(d1 <= d3);
  EXPECT_TRUE(d1 >= d3);

  // With ints
  EXPECT_TRUE(d1 == 5);
  EXPECT_TRUE(d1 != 6);
  EXPECT_TRUE(d1 < 6);
  EXPECT_TRUE(d1 <= 5);
  EXPECT_TRUE(d1 > 4);
  EXPECT_TRUE(d1 >= 5);

  expectCompare(LABELED(d1), LABELED(4), +1);
  expectCompare(LABELED(d1), LABELED(5), 0);
  expectCompare(LABELED(d1), LABELED(6), -1);
}


void test_unary()
{
  TEST_CASE("test_unary");

  NonNegativeInteger d0(0);
  NonNegativeInteger d1(1);
  NonNegativeInteger d2(2);

  EXPECT_EQ((+d0).get(), 0);
  EXPECT_EQ((+d1).get(), 1);
  EXPECT_EQ((+d2).get(), 2);

  EXPECT_EQ((-d0).get(), 0);
  EXPECT_EXN_SUBSTR(-d1,
    XAssert, "Value violates constraint for NonNegativeInteger: -1.");
}


void test_gdv()
{
  NonNegativeInteger c(2);
  GDValue v(c);
  EXPECT_EQ(v, GDValue(2));

  NonNegativeInteger d{GDValueParser(v)};
  EXPECT_EQ(d, c);

  EXPECT_EXN_SUBSTR(NonNegativeInteger(GDValueParser(GDValue(-2))),
    XGDValueError, "Invalid NonNegativeInteger: -2");
  EXPECT_EXN_SUBSTR(NonNegativeInteger(GDValueParser(GDValue("abc"))),
    XGDValueError, "Expected integer, not string.");
  EXPECT_EXN_SUBSTR(
    NonNegativeInteger(GDValueParser(GDValue(
      GDVInteger::fromDigits("123456789012345678901234567890")))),
    XGDValueError, "Out of range for NonNegativeInteger: 123456789012345678901234567890.");
}


void test_write()
{
  EXPECT_EQ(stringb(NonNegativeInteger(34)), "34");
}


CLOSE_ANONYMOUS_NAMESPACE


// Called from unit-tests.cc.
void test_wrapped_integer(CmdlineArgsSpan args)
{
  test_ctor();
  test_assignment();
  test_set_get();
  test_bool_conversion();
  test_increment_decrement();
  test_arithmetic();
  test_comparisons();
  test_unary();
  test_gdv();
  test_write();
}


// EOF
