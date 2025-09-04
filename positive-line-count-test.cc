// positive-line-count-test.cc
// Tests for `positive-line-count` module.

#include "unit-tests.h"                // decl for my entry point
#include "positive-line-count.h"       // module under test

#include "line-count.h"                // LineCount
#include "line-difference.h"           // LineDifference
#include "line-index.h"                // LineIndex

#include "smbase/exc.h"                // smbase::XAssert
#include "smbase/gdvalue-parser.h"     // gdv::{GDValueParser, XGDValueError}
#include "smbase/gdvalue.h"            // gdv::GDValue
#include "smbase/sm-macros.h"          // OPEN_ANONYMOUS_NAMESPACE
#include "smbase/sm-test-order.h"      // EXPECT_STRICTLY_ORDERED, EXPECT_COMPARE
#include "smbase/sm-test.h"            // EXPECT_EQ
#include "smbase/stringb.h"            // stringb

using namespace gdv;
using namespace smbase;


OPEN_ANONYMOUS_NAMESPACE


void test_ctor()
{
  TEST_CASE("test_ctor");

  PositiveLineCount d1(1);
  EXPECT_EQ(d1.get(), 1);

  {
    PositiveLineCount d1Copy(d1);
    EXPECT_EQ(d1Copy.get(), 1);
  }

  PositiveLineCount d2(2);
  EXPECT_EQ(d2.get(), 2);

  {
    PositiveLineCount d2Copy(d2);
    EXPECT_EQ(d2Copy.get(), 2);
  }

  EXPECT_STRICTLY_ORDERED(PositiveLineCount, d1, d2);
}


void test_ctor_LineDifference()
{
  TEST_CASE("test_ctor_LineDifference");

  PositiveLineCount c(LineDifference(3));
  EXPECT_EQ(c.get(), 3);

  EXPECT_EXN_SUBSTR(PositiveLineCount(LineDifference(-1)),
    XAssert, "Value violates constraint for PositiveLineCount: -1.");
}


void test_ctor_LineCount()
{
  TEST_CASE("test_ctor_LineCount");

  PositiveLineCount c(LineCount(3));
  EXPECT_EQ(c.get(), 3);

  EXPECT_EXN_SUBSTR(PositiveLineCount(LineCount(0)),
    XAssert, "Value violates constraint for PositiveLineCount: 0.");
}


void test_toLineCount()
{
  PositiveLineCount plc(2);

  {
    LineCount lc;
    lc = plc;
    EXPECT_EQ(lc.get(), 2);
  }

  {
    LineCount lc(plc);
    EXPECT_EQ(lc.get(), 2);
  }
}


void test_toLineDifference()
{
  PositiveLineCount plc(2);

  {
    LineDifference ld;
    ld = plc;
    EXPECT_EQ(ld.get(), 2);
  }

  {
    LineDifference ld(plc);
    EXPECT_EQ(ld.get(), 2);
  }
}


void test_compareToLineIndex()
{
  PositiveLineCount plc(2);

  EXPECT_COMPARE(plc, LineIndex(1), +1);
  EXPECT_COMPARE(plc, LineIndex(2),  0);
  EXPECT_COMPARE(plc, LineIndex(3), -1);
}


void test_assignment()
{
  TEST_CASE("test_assignment");

  PositiveLineCount d1(5);
  PositiveLineCount d2(1);
  d2 = d1;
  EXPECT_EQ(d2.get(), 5);

  // Self-assignment, slightly obfuscated to avoid compiler warning.
  d2 = *&d2;
  EXPECT_EQ(d2.get(), 5);
}


void test_set_get()
{
  TEST_CASE("test_set_get");

  PositiveLineCount d(2);
  d.set(42);
  EXPECT_EQ(d.get(), 42);

  EXPECT_EXN_SUBSTR(d.set(-7),
    XAssert, "Value violates constraint for PositiveLineCount: -7.");
}


void test_increment_decrement()
{
  TEST_CASE("test_increment_decrement");

  PositiveLineCount d(5);

  // Prefix ++
  PositiveLineCount &pref = ++d;
  EXPECT_EQ(d.get(), 6);
  EXPECT_EQ(pref.get(), 6);

  // Postfix ++
  PositiveLineCount old = d++;
  EXPECT_EQ(old.get(), 6);
  EXPECT_EQ(d.get(), 7);

  // Prefix --
  PositiveLineCount &pref2 = --d;
  EXPECT_EQ(d.get(), 6);
  EXPECT_EQ(pref2.get(), 6);

  // Postfix --
  PositiveLineCount old2 = d--;
  EXPECT_EQ(old2.get(), 6);
  EXPECT_EQ(d.get(), 5);
}


void test_arithmetic()
{
  TEST_CASE("test_arithmetic");

  PositiveLineCount d1(10);
  PositiveLineCount d2(3);

  EXPECT_EQ((d1 + d2).get(), 13);
  EXPECT_EQ((d1 + LineDifference(5)).get(), 15);

  PositiveLineCount d3(7);
  d3 += d2;
  EXPECT_EQ(d3.get(), 10);
  d3 += LineDifference(2);
  EXPECT_EQ(d3.get(), 12);

  EXPECT_EQ((d1 - d2).get(), 7);
  EXPECT_EQ((d1 - LineDifference(4)).get(), 6);

  PositiveLineCount d4(20);
  d4 -= d2;
  EXPECT_EQ(d4.get(), 17);
  d4 -= PositiveLineCount(10);
  EXPECT_EQ(d4.get(), 7);
}


void test_opPlusEqLineCount()
{
  PositiveLineCount plc(1);
  LineCount lc(2);

  plc += lc;
  EXPECT_EQ(plc.get(), 3);
}


void test_comparisons()
{
  TEST_CASE("test_comparisons");

  PositiveLineCount d1(5);
  PositiveLineCount d2(7);
  PositiveLineCount d3(5);

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

  PositiveLineCount d1(1);
  PositiveLineCount d2(2);

  EXPECT_EQ((+d1).get(), 1);
  EXPECT_EQ((+d2).get(), 2);

  // Negation yields `LineDifference`, which can be negative.
  EXPECT_EQ((-d1).get(), -1);
  EXPECT_EQ((-d2).get(), -2);
}


void test_pred()
{
  PositiveLineCount c(1);
  LineCount lc = c.pred();
  EXPECT_EQ(lc.get(), 0);
}


void test_predPLC()
{
  TEST_CASE("test_predPLC");

  PositiveLineCount d(2);
  EXPECT_EQ(d.get(), 2);

  d = d.predPLC();
  EXPECT_EQ(d.get(), 1);

  EXPECT_EXN_SUBSTR(d.predPLC(),
    XAssert, "Value violates constraint for PositiveLineCount: 0.");
}


void test_gdv()
{
  PositiveLineCount c(2);
  GDValue v(c);
  EXPECT_EQ(v, GDValue(2));

  PositiveLineCount d{GDValueParser(v)};
  EXPECT_EQ(d, c);

  EXPECT_EXN_SUBSTR(PositiveLineCount(GDValueParser(GDValue(-2))),
    XGDValueError, "Invalid PositiveLineCount: -2.");
  EXPECT_EXN_SUBSTR(PositiveLineCount(GDValueParser(GDValue("abc"))),
    XGDValueError, "Expected integer, not string.");
  EXPECT_EXN_SUBSTR(
    PositiveLineCount(GDValueParser(GDValue(
      GDVInteger::fromDigits("123456789012345678901234567890")))),
    XGDValueError, "Out of range for PositiveLineCount: 123456789012345678901234567890.");
}


void test_write()
{
  EXPECT_EQ(stringb(PositiveLineCount(34)), "34");
}


CLOSE_ANONYMOUS_NAMESPACE


// Called from unit-tests.cc.
void test_positive_line_count(CmdlineArgsSpan args)
{
  test_ctor();
  test_ctor_LineDifference();
  test_ctor_LineCount();
  test_toLineDifference();
  test_compareToLineIndex();
  test_toLineCount();
  test_assignment();
  test_set_get();
  test_increment_decrement();
  test_arithmetic();
  test_opPlusEqLineCount();
  test_comparisons();
  test_unary();
  test_pred();
  test_predPLC();
  test_gdv();
  test_write();
}


// EOF
