// line-number-test.cc
// Tests for `line-number` module.

#include "line-number.h"               // module under test

#include "line-difference.h"           // LineDifference
#include "line-index.h"                // LineIndex

#include "smbase/sm-macros.h"          // OPEN_ANONYMOUS_NAMESPACE
#include "smbase/sm-test.h"            // EXPECT_EQ


OPEN_ANONYMOUS_NAMESPACE


void test_toLineIndex()
{
  LineNumber n(1);
  EXPECT_EQ(n.toLineIndex().get(), 0);

  n.set(2);
  EXPECT_EQ(n.toLineIndex().get(), 1);
}


void test_subtract()
{
  LineNumber n1(1);
  LineNumber n2(2);

  LineDifference d = n1 - n2;
  EXPECT_EQ(d.get(), -1);

  EXPECT_EQ((n1 - n1).get(), 0);
  EXPECT_EQ((n2 - n1).get(), 1);

  LineNumber n3 = n2 - d;
  EXPECT_EQ(n3.get(), 3);

  n3 -= d;
  EXPECT_EQ(n3.get(), 4);
}


CLOSE_ANONYMOUS_NAMESPACE


void test_line_number()
{
  test_toLineIndex();
  test_subtract();
}


// EOF
