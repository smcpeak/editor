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

using namespace gdv;
using namespace smbase;


OPEN_ANONYMOUS_NAMESPACE


void test_ctor()
{
  LineIndex i0(0);
  EXPECT_EQ(i0.get(), 0);

  LineIndex i1(1);
  EXPECT_EQ(i1.get(), 1);

  LineIndex i2(2);
  EXPECT_EQ(i2.get(), 2);

  EXPECT_STRICTLY_ORDERED(LineIndex, i0, i1, i2);

  EXPECT_EXN_SUBSTR(LineIndex(-1),
    XAssert, "value >= 0");

  EXPECT_EQ(stringb(i0), "0");
  EXPECT_EQ(stringb(i1), "1");
  EXPECT_EQ(stringb(i2), "2");
}


void test_GDValue()
{
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


CLOSE_ANONYMOUS_NAMESPACE


// Called from unit-tests.cc.
void test_line_index(CmdlineArgsSpan args)
{
  test_ctor();
  test_GDValue();
}


// EOF
