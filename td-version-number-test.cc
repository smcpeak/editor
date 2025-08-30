// td-version-number-test.cc
// Tests for `td-version-number` module.

#include "unit-tests.h"                // decl for my entry point
#include "td-version-number.h"         // module under test

#include "smbase/exc.h"                // smbase::XAssert
#include "smbase/sm-macros.h"          // OPEN_ANONYMOUS_NAMESPACE
#include "smbase/sm-test.h"            // EXPECT_EQ
#include "smbase/xoverflow.h"          // smbase::XOverflow

#include <cstdint>                     // INT64_C

using namespace smbase;


OPEN_ANONYMOUS_NAMESPACE


void test_ctor()
{
  EXPECT_EQ(TD_VersionNumber(3).get(), 3);
  EXPECT_EQ(TD_VersionNumber(INT64_C(0x80000000)).get(),
                             INT64_C(0x80000000));

  EXPECT_EXN_SUBSTR(TD_VersionNumber(-1),
    XAssert, "Value violates constraint for TD_VersionNumber: -1.");
}


void test_preIncrement()
{
  TD_VersionNumber v(INT64_C(0x7FFFffffFFFFfffe));
  EXPECT_EQ(v.get(), INT64_C(0x7FFFffffFFFFfffe));

  preIncrementWithOverflowCheck(v);
  EXPECT_EQ(v.get(), INT64_C(0x7FFFffffFFFFffff));

  EXPECT_EXN_SUBSTR(preIncrementWithOverflowCheck(v),
    XOverflow, "+ 1 would overflow.");
}


CLOSE_ANONYMOUS_NAMESPACE


void test_td_version_number(CmdlineArgsSpan args)
{
  test_ctor();
  test_preIncrement();
}


// EOF
