// line-index-test.cc
// Tests for `line-index` module.

#include "unit-tests.h"                // decl for my entry point
#include "line-index.h"                // module under test

#include "smbase/gdvalue-parser.h"     // gdv::{GDValueParser, XGDValueError}
#include "smbase/gdvalue.h"            // gdv::GDValue
#include "smbase/sm-macros.h"          // OPEN_ANONYMOUS_NAMESPACE
#include "smbase/sm-pp-util.h"         // SM_PP_MAP
#include "smbase/sm-test-order.h"      // EXPECT_STRICTLY_ORDERED
#include "smbase/sm-test.h"            // EXPECT_EQ, EXPECT_EQ_GDV
#include "smbase/stringb.h"            // stringb

#include <cstddef>                     // std::size_t
#include <functional>                  // std::reference_wrapper
#include <limits>                      // std::numeric_limits

using namespace gdv;
using namespace smbase;


OPEN_ANONYMOUS_NAMESPACE


void test_ctor()
{
  TEST_CASE("test_ctor");

  LineIndex i0(0);
  i0.selfCheck();
  EXPECT_EQ(i0.isZero(), true);
  EXPECT_EQ(i0.isPositive(), false);
  EXPECT_EQ(i0.get(), 0);
  EXPECT_EQ(i0.getForNow(), 0);

  LineIndex i1(1);
  i1.selfCheck();
  EXPECT_EQ(i1.isZero(), false);
  EXPECT_EQ(i1.isPositive(), true);
  EXPECT_EQ(i1.get(), 1);
  EXPECT_EQ(i1.getForNow(), 1);

  LineIndex i2(2);
  i2.selfCheck();
  EXPECT_EQ(i2.isZero(), false);
  EXPECT_EQ(i2.isPositive(), true);
  EXPECT_EQ(i2.get(), 2);
  EXPECT_EQ(i2.getForNow(), 2);

  EXPECT_STRICTLY_ORDERED(LineIndex, i0, i1, i2);

  EXPECT_EXN_SUBSTR(LineIndex(-1),
    XAssert, "value >= 0");

  EXPECT_EQ(stringb(i0), "0");
  EXPECT_EQ(stringb(i1), "1");
  EXPECT_EQ(stringb(i2), "2");

  EXPECT_EQ(i0 < 0, false);
  EXPECT_EQ(i0 < 1, true);
  EXPECT_EQ(i0 <= 0, true);
  EXPECT_EQ(i0 <= 1, true);

  EXPECT_EQ(i1 < 0, false);
  EXPECT_EQ(i1 < 1, false);
  EXPECT_EQ(i1 < 2, true);
  EXPECT_EQ(i1 <= 0, false);
  EXPECT_EQ(i1 <= 1, true);
  EXPECT_EQ(i1 <= 2, true);

  EXPECT_EQ(i2 < 0, false);
  EXPECT_EQ(i2 < 1, false);
  EXPECT_EQ(i2 < 2, false);
  EXPECT_EQ(i2 <= 0, false);
  EXPECT_EQ(i2 <= 1, false);
  EXPECT_EQ(i2 <= 2, true);
}


void test_GDValue()
{
  TEST_CASE("test_GDValue");

  LineIndex i0(0);
  LineIndex i1(1);
  LineIndex i2(2);

  EXPECT_EQ_GDV(i0, 0);
  EXPECT_EQ_GDV(i1, 1);
  EXPECT_EQ_GDV(i2, 2);

  // Round-trip.
  EXPECT_EQ(LineIndex(GDValueParser(toGDValue(i0))), i0);
  EXPECT_EQ(LineIndex(GDValueParser(toGDValue(i1))), i1);
  EXPECT_EQ(LineIndex(GDValueParser(toGDValue(i2))), i2);

  EXPECT_EXN_SUBSTR(LineIndex(GDValueParser(GDValue(-1))),
    XGDValueError, "negative: -1.");
  EXPECT_EXN_SUBSTR(
    LineIndex(GDValueParser(GDValue(GDVInteger::fromDigits(
      "123456789012345678901234567890")))),
    XGDValueError, "too large: 123456789012345678901234567890.");
}


void test_iterate()
{
  TEST_CASE("test_iterate");

  int expect = 0;
  for (LineIndex i; i < 3; ++i) {
    EXPECT_EQ(i.get(), expect);
    ++expect;
  }

  expect = 0;
  for (LineIndex i(0); i < 3; ++i) {
    EXPECT_EQ(i.get(), expect);
    ++expect;
  }
}


void test_inc_dec()
{
  TEST_CASE("test_inc_dec");

  LineIndex i(0);
  EXPECT_EQ(i.get(), 0);

  ++i;
  EXPECT_EQ(i.get(), 1);

  ++i;
  EXPECT_EQ(i.get(), 2);

  --i;
  EXPECT_EQ(i.get(), 1);

  --i;
  EXPECT_EQ(i.get(), 0);
}


void test_add()
{
  TEST_CASE("test_add");

  EXPECT_EQ(LineIndex(1) + 2, LineIndex(3));
  EXPECT_EQ(LineIndex(1) + (-1), LineIndex(0));

  LineIndex i(0);
  EXPECT_EQ(i.get(), 0);

  i += 3;
  EXPECT_EQ(i.get(), 3);

  i += 2;
  EXPECT_EQ(i.get(), 5);

  i += (-4);
  EXPECT_EQ(i.get(), 1);
}


void test_tryIncrease()
{
  TEST_CASE("test_tryIncrease");

  LineIndex i(0);
  EXPECT_EQ(i.get(), 0);

  xassert(!i.tryIncrease(-1));
  EXPECT_EQ(i.get(), 0);

  xassert(i.tryIncrease(2));
  EXPECT_EQ(i.get(), 2);

  xassert(i.tryIncrease(-1));
  EXPECT_EQ(i.get(), 1);

  xassert(i.tryIncrease(3));
  EXPECT_EQ(i.get(), 4);

  xassert(!i.tryIncrease(-5));
  EXPECT_EQ(i.get(), 4);

  xassert(!i.tryIncrease(std::numeric_limits<int>::max()));
  EXPECT_EQ(i.get(), 4);
}


void test_clampIncrease()
{
  TEST_CASE("test_clampIncrease");

  LineIndex i(0);
  EXPECT_EQ(i.get(), 0);

  EXPECT_EQ(i.clampIncreased(-1).get(), 0);
  i.clampIncrease(-1);
  EXPECT_EQ(i.get(), 0);

  EXPECT_EQ(i.clampIncreased(2).get(), 2);
  i.clampIncrease(2);
  EXPECT_EQ(i.get(), 2);

  EXPECT_EQ(i.clampIncreased(-1).get(), 1);
  i.clampIncrease(-1);
  EXPECT_EQ(i.get(), 1);

  EXPECT_EQ(i.clampIncreased(3).get(), 4);
  i.clampIncrease(3);
  EXPECT_EQ(i.get(), 4);

  EXPECT_EQ(i.clampIncreased(-5).get(), 0);
  i.clampIncrease(-5);
  EXPECT_EQ(i.get(), 0);
}


void test_subtract()
{
  TEST_CASE("test_subtract");

  LineIndex i0(0);
  LineIndex i1(1);
  LineIndex i2(2);

  EXPECT_EQ(i2 - i2, 0);
  EXPECT_EQ(i2 - i1, 1);
  EXPECT_EQ(i2 - i0, 2);

  EXPECT_EQ(i1 - i2, -1);
}


void test_succ_pred()
{
  TEST_CASE("test_succ_pred");

  LineIndex i0(0);
  LineIndex i1(1);
  LineIndex i2(2);

  EXPECT_EQ(i0.succ().get(), 1);
  EXPECT_EQ(i1.succ().get(), 2);
  EXPECT_EQ(i2.succ().get(), 3);

  EXPECT_EQ(i0.pred().get(), 0);
  EXPECT_EQ(i1.pred().get(), 0);
  EXPECT_EQ(i2.pred().get(), 1);
}


CLOSE_ANONYMOUS_NAMESPACE


// Called from unit-tests.cc.
void test_line_index(CmdlineArgsSpan args)
{
  test_ctor();
  test_GDValue();
  test_iterate();
  test_inc_dec();
  test_add();
  test_tryIncrease();
  test_clampIncrease();
  test_subtract();
  test_succ_pred();
}


// EOF
