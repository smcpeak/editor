// line-difference-test.cc
// Tests for `line-difference` module.

#include "unit-tests.h"                // decl for my entry point
#include "line-difference.h"           // module under test

#include "smbase/sm-macros.h"          // OPEN_ANONYMOUS_NAMESPACE
#include "smbase/sm-test-order.h"      // EXPECT_STRICTLY_ORDERED
#include "smbase/sm-test.h"            // EXPECT_EQ


OPEN_ANONYMOUS_NAMESPACE


void test_ctor()
{
  TEST_CASE("test_ctor");

  LineDifference d0;
  EXPECT_EQ(d0.get(), 0);

  {
    LineDifference d0b(0);
    EXPECT_EQ(d0b.get(), 0);
  }

  {
    LineDifference d0Copy(d0);
    EXPECT_EQ(d0Copy.get(), 0);
  }

  LineDifference d1(1);
  EXPECT_EQ(d1.get(), 1);

  {
    LineDifference d1Copy(d1);
    EXPECT_EQ(d1Copy.get(), 1);
  }

  LineDifference d2(2);
  EXPECT_EQ(d2.get(), 2);

  {
    LineDifference d2Copy(d2);
    EXPECT_EQ(d2Copy.get(), 2);
  }

  EXPECT_STRICTLY_ORDERED(LineDifference, d0, d1, d2);
}


void test_assignment()
{
  TEST_CASE("test_assignment");

  LineDifference d1(5);
  LineDifference d2;
  d2 = d1;
  EXPECT_EQ(d2.get(), 5);

  // Self-assignment, slightly obfuscated to avoid compiler warning.
  d2 = *&d2;
  EXPECT_EQ(d2.get(), 5);
}


void test_set_get()
{
  TEST_CASE("test_set_get");

  LineDifference d;
  d.set(42);
  EXPECT_EQ(d.get(), 42);

  d.set(-7);
  EXPECT_EQ(d.get(), -7);
}


void test_bool_conversion()
{
  TEST_CASE("test_bool_conversion");

  LineDifference d0;
  EXPECT_FALSE(static_cast<bool>(d0));

  LineDifference d1(1);
  EXPECT_TRUE(static_cast<bool>(d1));

  LineDifference dneg(-3);
  EXPECT_TRUE(static_cast<bool>(dneg));
}


void test_increment_decrement()
{
  TEST_CASE("test_increment_decrement");

  LineDifference d(5);

  // Prefix ++
  LineDifference &pref = ++d;
  EXPECT_EQ(d.get(), 6);
  EXPECT_EQ(pref.get(), 6);

  // Postfix ++
  LineDifference old = d++;
  EXPECT_EQ(old.get(), 6);
  EXPECT_EQ(d.get(), 7);

  // Prefix --
  LineDifference &pref2 = --d;
  EXPECT_EQ(d.get(), 6);
  EXPECT_EQ(pref2.get(), 6);

  // Postfix --
  LineDifference old2 = d--;
  EXPECT_EQ(old2.get(), 6);
  EXPECT_EQ(d.get(), 5);
}


void test_arithmetic()
{
  TEST_CASE("test_arithmetic");

  LineDifference d1(10);
  LineDifference d2(3);

  EXPECT_EQ((d1 + d2).get(), 13);
  EXPECT_EQ((d1 + 5).get(), 15);

  LineDifference d3(7);
  d3 += d2;
  EXPECT_EQ(d3.get(), 10);
  d3 += 2;
  EXPECT_EQ(d3.get(), 12);

  EXPECT_EQ((d1 - d2).get(), 7);
  EXPECT_EQ((d1 - 4).get(), 6);

  LineDifference d4(20);
  d4 -= d2;
  EXPECT_EQ(d4.get(), 17);
  d4 -= 10;
  EXPECT_EQ(d4.get(), 7);
}


void test_comparisons()
{
  TEST_CASE("test_comparisons");

  LineDifference d1(5);
  LineDifference d2(7);
  LineDifference d3(5);

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

  LineDifference dn1(-1);
  LineDifference d0(0);
  LineDifference d1(1);
  LineDifference d2(2);

  EXPECT_EQ((+dn1).get(), -1);
  EXPECT_EQ((+d0).get(), 0);
  EXPECT_EQ((+d1).get(), 1);
  EXPECT_EQ((+d2).get(), 2);

  EXPECT_EQ((-dn1).get(), 1);
  EXPECT_EQ((-d0).get(), 0);
  EXPECT_EQ((-d1).get(), -1);
  EXPECT_EQ((-d2).get(), -2);
}


void test_clampLower()
{
  TEST_CASE("test_clampLower");

  LineDifference d(3);

  d.clampLower(LineDifference(2));
  EXPECT_EQ(d.get(), 3);   // unchanged

  d.clampLower(LineDifference(3));
  EXPECT_EQ(d.get(), 3);   // unchanged

  d.clampLower(LineDifference(5));
  EXPECT_EQ(d.get(), 5);   // clamped up
}


CLOSE_ANONYMOUS_NAMESPACE


// Called from unit-tests.cc.
void test_line_difference(CmdlineArgsSpan args)
{
  test_ctor();
  test_assignment();
  test_set_get();
  test_bool_conversion();
  test_increment_decrement();
  test_arithmetic();
  test_comparisons();
  test_unary();
  test_clampLower();
}


// EOF
