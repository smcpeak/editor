// line-count-test.cc
// Tests for `line-count` module.

#include "unit-tests.h"                // decl for my entry point
#include "line-count.h"                // module under test

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


void test_ctor()
{
  TEST_CASE("test_ctor");

  LineCount d0;
  EXPECT_EQ(d0.get(), 0);

  {
    LineCount d0b(0);
    EXPECT_EQ(d0b.get(), 0);
  }

  {
    LineCount d0Copy(d0);
    EXPECT_EQ(d0Copy.get(), 0);
  }

  LineCount d1(1);
  EXPECT_EQ(d1.get(), 1);

  {
    LineCount d1Copy(d1);
    EXPECT_EQ(d1Copy.get(), 1);
  }

  LineCount d2(2);
  EXPECT_EQ(d2.get(), 2);

  {
    LineCount d2Copy(d2);
    EXPECT_EQ(d2Copy.get(), 2);
  }

  EXPECT_STRICTLY_ORDERED(LineCount, d0, d1, d2);
}


void test_assignment()
{
  TEST_CASE("test_assignment");

  LineCount d1(5);
  LineCount d2;
  d2 = d1;
  EXPECT_EQ(d2.get(), 5);

  // Self-assignment, slightly obfuscated to avoid compiler warning.
  d2 = *&d2;
  EXPECT_EQ(d2.get(), 5);
}


void test_set_get()
{
  TEST_CASE("test_set_get");

  LineCount d;
  d.set(42);
  EXPECT_EQ(d.get(), 42);

  EXPECT_EXN_SUBSTR(d.set(-7),
    XAssert, "m_value >= 0");
}


void test_bool_conversion()
{
  TEST_CASE("test_bool_conversion");

  LineCount d0;
  EXPECT_FALSE(static_cast<bool>(d0));

  LineCount d1(1);
  EXPECT_TRUE(static_cast<bool>(d1));
}


void test_increment_decrement()
{
  TEST_CASE("test_increment_decrement");

  LineCount d(5);

  // Prefix ++
  LineCount &pref = ++d;
  EXPECT_EQ(d.get(), 6);
  EXPECT_EQ(pref.get(), 6);

  // Postfix ++
  LineCount old = d++;
  EXPECT_EQ(old.get(), 6);
  EXPECT_EQ(d.get(), 7);

  // Prefix --
  LineCount &pref2 = --d;
  EXPECT_EQ(d.get(), 6);
  EXPECT_EQ(pref2.get(), 6);

  // Postfix --
  LineCount old2 = d--;
  EXPECT_EQ(old2.get(), 6);
  EXPECT_EQ(d.get(), 5);
}


void test_arithmetic()
{
  TEST_CASE("test_arithmetic");

  LineCount d1(10);
  LineCount d2(3);

  EXPECT_EQ((d1 + d2).get(), 13);
  EXPECT_EQ((d1 + 5).get(), 15);

  LineCount d3(7);
  d3 += d2;
  EXPECT_EQ(d3.get(), 10);
  d3 += 2;
  EXPECT_EQ(d3.get(), 12);

  EXPECT_EQ((d1 - d2).get(), 7);
  EXPECT_EQ((d1 - 4).get(), 6);

  LineCount d4(20);
  d4 -= d2;
  EXPECT_EQ(d4.get(), 17);
  d4 -= LineCount(10);
  EXPECT_EQ(d4.get(), 7);
}


void test_comparisons()
{
  TEST_CASE("test_comparisons");

  LineCount d1(5);
  LineCount d2(7);
  LineCount d3(5);

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

  LineCount d0(0);
  LineCount d1(1);
  LineCount d2(2);

  EXPECT_EQ((+d0).get(), 0);
  EXPECT_EQ((+d1).get(), 1);
  EXPECT_EQ((+d2).get(), 2);

  // Negation yields `LineDifference`, which can be negative.
  EXPECT_EQ((-d0).get(), 0);
  EXPECT_EQ((-d1).get(), -1);
  EXPECT_EQ((-d2).get(), -2);
}


void test_nzpred()
{
  TEST_CASE("test_nzpred");

  LineCount d(2);
  EXPECT_EQ(d.get(), 2);

  d = d.nzpred();
  EXPECT_EQ(d.get(), 1);

  d = d.nzpred();
  EXPECT_EQ(d.get(), 0);

  EXPECT_EXN_SUBSTR(d.nzpred(),
    XAssert, "isPositive");
}


void test_gdv()
{
  LineCount c(2);
  GDValue v(c);
  EXPECT_EQ(v, GDValue(2));

  LineCount d{GDValueParser(v)};
  EXPECT_EQ(d, c);

  EXPECT_EXN_SUBSTR(LineCount(GDValueParser(GDValue(-2))),
    XGDValueError, "negative: -2");
  EXPECT_EXN_SUBSTR(LineCount(GDValueParser(GDValue("abc"))),
    XGDValueError, "Expected integer, not string.");
  EXPECT_EXN_SUBSTR(
    LineCount(GDValueParser(GDValue(
      GDVInteger::fromDigits("123456789012345678901234567890")))),
    XGDValueError, "out of range: 123456789012345678901234567890.");
}


void test_write()
{
  EXPECT_EQ(stringb(LineCount(34)), "34");
}


CLOSE_ANONYMOUS_NAMESPACE


// Called from unit-tests.cc.
void test_line_count(CmdlineArgsSpan args)
{
  test_ctor();
  test_assignment();
  test_set_get();
  test_bool_conversion();
  test_increment_decrement();
  test_arithmetic();
  test_comparisons();
  test_unary();
  test_nzpred();
  test_gdv();
  test_write();
}


// EOF
