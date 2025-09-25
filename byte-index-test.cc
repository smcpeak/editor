// byte-index-test.cc
// Tests for `byte-index` module.

#include "unit-tests.h"                // decl for my entry point
#include "byte-index.h"                // module under test

#include "byte-count.h"                // ByteCount
#include "byte-difference.h"           // ByteDifference

#include "smbase/sm-macros.h"          // OPEN_ANONYMOUS_NAMESPACE
#include "smbase/sm-test.h"            // EXPECT_EQ
#include "smbase/xassert.h"            // xassert

using namespace smbase;


OPEN_ANONYMOUS_NAMESPACE


void test_ctor()
{
  EXPECT_EQ(ByteIndex(std::ptrdiff_t(3)).get(), 3);
  EXPECT_EQ(ByteIndex(std::size_t(3)).get(), 3);
  EXPECT_EQ(ByteIndex(ByteCount(3)).get(), 3);
  EXPECT_EQ(ByteIndex(ByteDifference(3)).get(), 3);
}


void test_conversion()
{
  EXPECT_EQ(ByteIndex(4).operator ByteCount().get(), 4);
  EXPECT_EQ(ByteIndex(4).operator ByteDifference().get(), 4);
}


void test_compare()
{
  EXPECT_TRUE(ByteIndex(2) < ByteDifference(3));
  EXPECT_FALSE(ByteDifference(4) < ByteIndex(3));

  EXPECT_TRUE(ByteIndex(2) < ByteCount(3));
  EXPECT_FALSE(ByteCount(4) < ByteIndex(3));
}


void test_addition()
{
  EXPECT_EQ((ByteIndex(3) + ByteCount(1)).get(), 4);

  {
    ByteIndex c(3);
    EXPECT_EQ((c += ByteCount(4)).get(), 7);
    EXPECT_EQ(c.get(), 7);
  }

  EXPECT_EQ((ByteIndex(3) + ByteDifference(-1)).get(), 2);

  {
    ByteIndex c(3);
    EXPECT_EQ((c += ByteDifference(4)).get(), 7);
    EXPECT_EQ(c.get(), 7);
  }
}


void test_subtraction()
{
  EXPECT_EQ((-ByteIndex(3)).get(), -3);
  EXPECT_EQ((ByteIndex(3) - ByteIndex(2)).get(), 1);

  EXPECT_EQ((ByteIndex(3) - ByteDifference(-1)).get(), 4);

  ByteIndex c(3);
  EXPECT_EQ((c -= ByteDifference(2)).get(), 1);
  EXPECT_EQ(c.get(), 1);
}


void test_clampDecrease()
{
  ByteIndex i(10);
  i.clampDecrease(ByteDifference(2));
  EXPECT_EQ(i.get(), 8);

  i.clampDecrease(ByteDifference(2), ByteIndex(5));
  EXPECT_EQ(i.get(), 6);

  i.clampDecrease(ByteDifference(2), ByteIndex(5));
  EXPECT_EQ(i.get(), 5);

  i.clampDecrease(ByteDifference(-2), ByteIndex(5));
  EXPECT_EQ(i.get(), 7);

  i.clampDecrease(ByteDifference(20));
  EXPECT_EQ(i.get(), 0);
}


void test_at()
{
  xassert(at(std::string("abc"), ByteIndex(1)) == 'b');
}


void test_substr()
{
  EXPECT_EQ(substr(std::string("abcd"), ByteIndex(1), ByteCount(2)),
            std::string("bc"));
}


void test_toByteColumnNumber()
{
  EXPECT_EQ(ByteIndex(0).toByteColumnNumber(), 1);
  EXPECT_EQ(ByteIndex(1).toByteColumnNumber(), 2);
  EXPECT_EQ(ByteIndex(2).toByteColumnNumber(), 3);
}


CLOSE_ANONYMOUS_NAMESPACE


// Called from unit-tests.cc.
void test_byte_index(CmdlineArgsSpan args)
{
  test_ctor();
  test_conversion();
  test_compare();
  test_addition();
  test_subtraction();
  test_clampDecrease();
  test_at();
  test_substr();
  test_toByteColumnNumber();
}


// EOF
