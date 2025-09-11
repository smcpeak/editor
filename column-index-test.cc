// column-index-test.cc
// Tests for `column-index` module.

#include "unit-tests.h"                // decl for my entry point
#include "column-index.h"              // module under test

#include "column-count.h"              // ColumnCount
#include "column-difference.h"         // ColumnDifference

#include "smbase/sm-macros.h"          // OPEN_ANONYMOUS_NAMESPACE
#include "smbase/sm-test.h"            // EXPECT_EQ

using namespace smbase;


OPEN_ANONYMOUS_NAMESPACE


void test_ctor()
{
  EXPECT_EQ(ColumnIndex(ColumnDifference(5)).get(), 5);
  EXPECT_EQ(ColumnIndex(ColumnCount(54)).get(), 54);
}


void test_conversion()
{
  EXPECT_EQ(ColumnIndex(3).operator ColumnDifference().get(), 3);
  EXPECT_EQ(ColumnIndex(32).operator ColumnCount().get(), 32);
}


void test_toColumnNumber()
{
  EXPECT_EQ(ColumnIndex(0).toColumnNumber(), 1);
  EXPECT_EQ(ColumnIndex(10).toColumnNumber(), 11);
}


void test_addition()
{
  ColumnIndex index = ColumnIndex(3) + ColumnDifference(-1);
  EXPECT_EQ(index.get(), 2);

  index += ColumnDifference(11);
  EXPECT_EQ(index.get(), 13);
}


void test_clampIncrease()
{
  ColumnIndex index(3);

  index.clampIncrease(ColumnDifference(5), ColumnIndex(2));
  EXPECT_EQ(index.get(), 8);

  index.clampIncrease(ColumnDifference(-4), ColumnIndex(2));
  EXPECT_EQ(index.get(), 4);

  index.clampIncrease(ColumnDifference(-4), ColumnIndex(2));
  EXPECT_EQ(index.get(), 2);

  index.clampIncrease(ColumnDifference(-4));
  EXPECT_EQ(index.get(), 0);
}


void test_subtract()
{
  ColumnDifference difference = - ColumnIndex(4);
  EXPECT_EQ(difference.get(), -4);

  difference = ColumnIndex(8) - ColumnIndex(50);
  EXPECT_EQ(difference.get(), -42);

  difference = ColumnIndex(8) - ColumnCount(22);
  EXPECT_EQ(difference.get(), -14);

  ColumnIndex index = ColumnIndex(9) - ColumnDifference(7);
  EXPECT_EQ(index.get(), 2);

  index += ColumnDifference(13);
  EXPECT_EQ(index.get(), 15);
}


void test_roundUpToMultipleOf()
{
  auto one = [](int i, int m, int e) -> void {
    EXPECT_EQ(
      ColumnIndex(i).roundUpToMultipleOf(ColumnCount(m)).get(),
      e);
  };

  one(0, 10, 0);
  one(1, 10, 10);
  one(9, 10, 10);
  one(10, 10, 10);
  one(11, 10, 20);
}


CLOSE_ANONYMOUS_NAMESPACE


// Called from unit-tests.cc.
void test_column_index(CmdlineArgsSpan args)
{
  test_ctor();
  test_conversion();
  test_toColumnNumber();
  test_addition();
  test_clampIncrease();
  test_subtract();
  test_roundUpToMultipleOf();
}


// EOF
