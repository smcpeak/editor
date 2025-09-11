// wrapped-integer-test.cc
// Tests for `wrapped-integer` module.

#include "unit-tests.h"                // decl for my entry point

#include "addable-wrapped-integer.h"   // module under test
#include "clampable-wrapped-integer.h" // module under test
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


// Unconstrained wrapped integer for use as the difference type in the
// `ClampableInteger` specialization.
class IntegerDifference
  : public WrappedInteger<int, IntegerDifference> {

public:      // types
  using Base = WrappedInteger<int, IntegerDifference>;
  friend Base;

protected:   // methods
  static bool isValid(int value)
    { return true; }

  static char const *getTypeName()
    { return "IntegerDifference"; }

public:      // methods
  // Inherit ctors.
  using Base::Base;
};


// Wrapped integer that is never negative.
class NonNegativeInteger
  : public WrappedInteger<int, NonNegativeInteger>,
    public AddableWrappedInteger<int, NonNegativeInteger, IntegerDifference>,
    public ClampableInteger<int, NonNegativeInteger, IntegerDifference> {

public:      // types
  using Base = WrappedInteger<int, NonNegativeInteger>;
  friend Base;

  using Addable = AddableWrappedInteger<int, NonNegativeInteger, IntegerDifference>;

protected:   // methods
  static bool isValid(int value)
    { return value >= 0; }

  static char const *getTypeName()
    { return "NonNegativeInteger"; }

public:      // methods
  // Inherit ctors.
  using Base::Base;

  operator IntegerDifference() const
    { return IntegerDifference(get()); }

  using Base::operator+;
  using Base::operator+=;
  using Addable::operator+;
  using Addable::operator+=;
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


void test_clampLower()
{
  NonNegativeInteger c(3);

  c.clampLower(NonNegativeInteger(2));
  EXPECT_EQ(c.get(), 3);

  c.clampLower(NonNegativeInteger(3));
  EXPECT_EQ(c.get(), 3);

  c.clampLower(NonNegativeInteger(8));
  EXPECT_EQ(c.get(), 8);
}


void test_clampIncrease()
{
  TEST_CASE("test_clampIncrease");

  using Difference = IntegerDifference;

  NonNegativeInteger i(0);
  EXPECT_EQ(i.get(), 0);

  EXPECT_EQ(i.clampIncreased(Difference(-1)).get(), 0);
  i.clampIncrease(Difference(-1));
  EXPECT_EQ(i.get(), 0);

  EXPECT_EQ(i.clampIncreased(Difference(2)).get(), 2);
  i.clampIncrease(Difference(2));
  EXPECT_EQ(i.get(), 2);

  EXPECT_EQ(i.clampIncreased(Difference(-1)).get(), 1);
  i.clampIncrease(Difference(-1));
  EXPECT_EQ(i.get(), 1);

  EXPECT_EQ(i.clampIncreased(Difference(3)).get(), 4);
  i.clampIncrease(Difference(3));
  EXPECT_EQ(i.get(), 4);

  EXPECT_EQ(i.clampIncreased(Difference(-5)).get(), 0);
  i.clampIncrease(Difference(-5));
  EXPECT_EQ(i.get(), 0);

  EXPECT_EQ(i.clampIncreased(Difference(10), NonNegativeInteger(5)).get(), 10);
  i.clampIncrease(Difference(10), NonNegativeInteger(5));
  EXPECT_EQ(i.get(), 10);

  EXPECT_EQ(i.clampIncreased(Difference(1), NonNegativeInteger(20)).get(), 20);
  i.clampIncrease(Difference(1), NonNegativeInteger(20));
  EXPECT_EQ(i.get(), 20);

  EXPECT_EQ(i.clampIncreased(Difference(-1), NonNegativeInteger(3)).get(), 19);
  i.clampIncrease(Difference(-1), NonNegativeInteger(3));
  EXPECT_EQ(i.get(), 19);

  EXPECT_EQ(i.clampIncreased(Difference(-100), NonNegativeInteger(3)).get(), 3);
  i.clampIncrease(Difference(-100), NonNegativeInteger(3));
  EXPECT_EQ(i.get(), 3);
}


void test_addOther()
{
  EXPECT_EQ(NonNegativeInteger(3) + IntegerDifference(2), 5);
  EXPECT_EQ(NonNegativeInteger(3) + IntegerDifference(-2), 1);

  EXPECT_EXN_SUBSTR(NonNegativeInteger(3) + IntegerDifference(-5),
    XAssert,
    "Value violates constraint for NonNegativeInteger: -2.");

  NonNegativeInteger nni(5);
  nni += IntegerDifference(7);
  EXPECT_EQ(nni, 12);

  nni += IntegerDifference(-1);
  EXPECT_EQ(nni, 11);
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
  test_clampLower();
  test_clampIncrease();
  test_addOther();
}


// EOF
