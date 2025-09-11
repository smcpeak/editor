// column-count-test.cc
// Tests for `column-count` module.

#include "unit-tests.h"                // decl for my entry point
#include "column-count.h"              // module under test

#include "column-difference.h"         // ColumnDifference
#include "column-index.h"              // ColumnIndex

#include "smbase/sm-macros.h"          // OPEN_ANONYMOUS_NAMESPACE
#include "smbase/sm-test.h"            // EXPECT_EQ

using namespace smbase;


OPEN_ANONYMOUS_NAMESPACE


void test_ctor()
{
  EXPECT_EQ(ColumnCount(ColumnDifference(3)).get(), 3);
}


void test_conversion()
{
  EXPECT_EQ(ColumnCount(4).operator ColumnDifference().get(), 4);
}


void test_addition()
{
  ColumnCount count = ColumnCount(2) + ColumnDifference(3);
  EXPECT_EQ(count.get(), 5);

  count += ColumnDifference(-4);
  EXPECT_EQ(count.get(), 1);

  ColumnIndex index = count + ColumnIndex(7);
  EXPECT_EQ(index.get(), 8);
}


void test_clampLower()
{
  ColumnCount c(3);

  c.clampLower(ColumnCount(2));
  EXPECT_EQ(c.get(), 3);

  c.clampLower(ColumnCount(8));
  EXPECT_EQ(c.get(), 8);
}


void test_subtract()
{
  ColumnDifference difference = - ColumnCount(4);
  EXPECT_EQ(difference.get(), -4);

  difference = ColumnCount(8) - ColumnCount(3);
  EXPECT_EQ(difference.get(), 5);

  difference = ColumnCount(8) - ColumnIndex(1);
  EXPECT_EQ(difference.get(), 7);

  ColumnCount count = ColumnCount(4) - ColumnDifference(2);
  EXPECT_EQ(count.get(), 2);

  count -= ColumnDifference(-10);
  EXPECT_EQ(count.get(), 12);
}


CLOSE_ANONYMOUS_NAMESPACE


// Called from unit-tests.cc.
void test_column_count(CmdlineArgsSpan args)
{
  test_ctor();
  test_conversion();
  test_addition();
  test_clampLower();
  test_subtract();
}


// EOF
