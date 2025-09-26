// textmcoord-test.cc
// Tests for `textmcoord` module.

#include "unit-tests.h"                          // decl for my entry point
#include "textmcoord.h"                          // module under test

#include "smbase/gdvn-test-roundtrip.h"          // gdvnTestRoundtrip
#include "smbase/sm-macros.h"                    // OPEN_ANONYMOUS_NAMESPACE
#include "smbase/sm-test.h"                      // EXPECT_EQ

using namespace smbase;


OPEN_ANONYMOUS_NAMESPACE


TextMCoord tmc(int li, int bi)
{
  return TextMCoord(LineIndex(li), ByteIndex(bi));
}


void test_serialization()
{
  TextMCoordRange mcr(tmc(1, 2), tmc(3, 4));

  gdvnTestRoundtrip(mcr, "MCR(MC(1 2) MC(3 4))");
}


void test_toLineColNumberString()
{
  EXPECT_EQ(tmc(1, 2).toLineColNumberString(), "2:3");
}


void test_rangeContains_orAtCollapsed()
{
  EXPECT_FALSE(rangeContains_orAtCollapsed(2, 4, 1));
  EXPECT_TRUE(rangeContains_orAtCollapsed(2, 4, 2));
  EXPECT_TRUE(rangeContains_orAtCollapsed(2, 4, 3));
  EXPECT_FALSE(rangeContains_orAtCollapsed(2, 4, 4));

  EXPECT_FALSE(rangeContains_orAtCollapsed(2, 2, 1));
  EXPECT_TRUE(rangeContains_orAtCollapsed(2, 2, 2));
  EXPECT_FALSE(rangeContains_orAtCollapsed(2, 2, 3));
}


void test_contains_orAtCollapsed()
{
  {
    TextMCoordRange mcr(tmc(1, 2), tmc(3, 4));
    EXPECT_FALSE(mcr.contains_orAtCollapsed(tmc(1, 1)));
    EXPECT_TRUE(mcr.contains_orAtCollapsed(tmc(1, 2)));
    EXPECT_TRUE(mcr.contains_orAtCollapsed(tmc(2, 2)));
    EXPECT_TRUE(mcr.contains_orAtCollapsed(tmc(3, 2)));
    EXPECT_TRUE(mcr.contains_orAtCollapsed(tmc(3, 3)));
    EXPECT_FALSE(mcr.contains_orAtCollapsed(tmc(3, 4)));
  }

  {
    TextMCoordRange mcr(tmc(1, 2), tmc(1, 2));
    EXPECT_FALSE(mcr.contains_orAtCollapsed(tmc(1, 1)));
    EXPECT_TRUE(mcr.contains_orAtCollapsed(tmc(1, 2)));
    EXPECT_FALSE(mcr.contains_orAtCollapsed(tmc(1, 3)));
  }
}


CLOSE_ANONYMOUS_NAMESPACE


// Called from unit-tests.cc.
void test_textmcoord(CmdlineArgsSpan args)
{
  test_serialization();
  test_toLineColNumberString();
  test_rangeContains_orAtCollapsed();
  test_contains_orAtCollapsed();
}


// EOF
