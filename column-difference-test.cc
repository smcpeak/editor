// column-difference-test.cc
// Tests for `column-difference` module.

#include "unit-tests.h"                // decl for my entry point
#include "column-difference.h"         // module under test

#include "smbase/sm-macros.h"          // OPEN_ANONYMOUS_NAMESPACE
#include "smbase/sm-test.h"            // EXPECT_EQ

using namespace smbase;


OPEN_ANONYMOUS_NAMESPACE


void test_clampLower()
{
  ColumnDifference d(3);
  d.clampLower(ColumnDifference(2));
  EXPECT_EQ(d.get(), 3);

  d.clampLower(ColumnDifference(4));
  EXPECT_EQ(d.get(), 4);
}


void test_multiply()
{
  EXPECT_EQ(3 * ColumnDifference(4), 12);
}


void test_toString()
{
  EXPECT_EQ(toString(ColumnDifference(123)), "123");
}


CLOSE_ANONYMOUS_NAMESPACE


// Called from unit-tests.cc.
void test_column_difference(CmdlineArgsSpan args)
{
  test_clampLower();
  test_multiply();
  test_toString();
}


// EOF
