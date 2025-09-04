// byte-count-test.cc
// Tests for `byte-count` module.

#include "unit-tests.h"                // decls for my entry point
#include "byte-count.h"                // module under test

#include "byte-difference.h"           // ByteDifference
#include "smbase/sm-macros.h"          // OPEN_ANONYMOUS_NAMESPACE
#include "smbase/sm-test.h"            // EXPECT_EQ

#include <cstddef>                     // std::{ptrdiff_t, size_t}


OPEN_ANONYMOUS_NAMESPACE


void test_ctor()
{
  EXPECT_EQ(ByteCount(std::ptrdiff_t(3)).get(), 3);
  EXPECT_EQ(ByteCount(std::size_t(3)).get(), 3);
  EXPECT_EQ(ByteCount(ByteDifference(3)).get(), 3);
}


void test_conversion()
{
  EXPECT_EQ(ByteCount(4).operator ByteDifference().get(), 4);
}


void test_compare()
{
  EXPECT_TRUE(ByteCount(2) < ByteDifference(3));
  EXPECT_FALSE(ByteDifference(4) < ByteCount(3));
}


void test_addition()
{
  EXPECT_EQ((ByteCount(3) + ByteDifference(-1)).get(), 2);

  ByteCount c(3);
  EXPECT_EQ((c += ByteDifference(4)).get(), 7);
  EXPECT_EQ(c.get(), 7);
}


void test_subtraction()
{
  EXPECT_EQ((-ByteCount(3)).get(), -3);
  EXPECT_EQ((ByteCount(3) - ByteCount(2)).get(), 1);

  EXPECT_EQ((ByteCount(3) - ByteDifference(-1)).get(), 4);

  ByteCount c(3);
  EXPECT_EQ((c -= ByteDifference(2)).get(), 1);
  EXPECT_EQ(c.get(), 1);
}


void test_strlenBC()
{
  EXPECT_EQ(strlenBC("abc").get(), 3);
}


void test_memchrBC()
{
  char arr[4] = "abc";
  xassert(memchrBC(arr, 'a', ByteCount(3)) == arr+0);
  xassert(memchrBC(arr, 'b', ByteCount(3)) == arr+1);
  xassert(memchrBC(arr, 'c', ByteCount(3)) == arr+2);
  xassert(memchrBC(arr, 'c', ByteCount(2)) == nullptr);
  xassert(memchrBC(arr, '\0', ByteCount(3)) == nullptr);
  xassert(memchrBC(arr, '\0', ByteCount(4)) == arr+3);
}


void test_memcpyBC()
{
  char arr[5] = "abcd";
  memcpyBC(arr, "ABC", ByteCount(2));
  EXPECT_EQ(std::string(arr), "ABcd");
}


void test_memcmpBC()
{
  char arr1[4] = "abc";
  char arr2[4] = "def";

  EXPECT_TRUE(memcmpBC(arr1, arr2, ByteCount(3)) < 0);
  EXPECT_TRUE(memcmpBC(arr2, arr1, ByteCount(3)) > 0);
  EXPECT_TRUE(memcmpBC(arr1, arr1, ByteCount(3)) == 0);
  EXPECT_TRUE(memcmpBC(arr1, arr2, ByteCount(0)) == 0);
}


void test_sizeBC()
{
  EXPECT_EQ(sizeBC(std::string("abcd")).get(), 4);
}


void test_stringBC()
{
  EXPECT_EQ(stringBC("foobar", ByteCount(3)), "foo");
}


CLOSE_ANONYMOUS_NAMESPACE


// Called from unit-tests.cc.
void test_byte_count(CmdlineArgsSpan args)
{
  test_ctor();
  test_conversion();
  test_compare();
  test_addition();
  test_subtraction();
  test_strlenBC();
  test_memchrBC();
  test_memcpyBC();
  test_memcmpBC();
  test_sizeBC();
  test_stringBC();
}


// EOF
