// lsp-version-number-test.cc
// Tests for `lsp-version-number` module.

#include "unit-tests.h"                // decl for my entry point
#include "lsp-version-number.h"        // module under test

#include "td-version-number.h"         // TD_VersionNumber

#include "smbase/sm-integer.h"         // smbase::Integer
#include "smbase/sm-macros.h"          // OPEN_ANONYMOUS_NAMESPACE
#include "smbase/sm-test-order.h"      // EXPECT_COMPARE
#include "smbase/sm-test.h"            // EXPECT_EQ
#include "smbase/xoverflow.h"          // smbase::XNumericConversion

#include <cstdint>                     // std::int32_t, etc.

using namespace smbase;


OPEN_ANONYMOUS_NAMESPACE


void test_ctor()
{
  EXPECT_EQ(LSP_VersionNumber(3).get(), 3);
  EXPECT_EQ(LSP_VersionNumber(std::uint32_t(4)).get(), 4);
  EXPECT_EQ(LSP_VersionNumber(std::int64_t(5)).get(), 5);
  EXPECT_EQ(LSP_VersionNumber(std::uint64_t(6)).get(), 6);
  EXPECT_EQ(LSP_VersionNumber(Integer(7)).get(), 7);

  // Does not compile due to deleted ctor.
  //LSP_VersionNumber(std::int16_t(8));

  EXPECT_EXN_SUBSTR(LSP_VersionNumber(UINT32_C(0x80000000)),
    XNumericConversion, "cannot be represented");
  EXPECT_EXN_SUBSTR(LSP_VersionNumber(-1),
    XAssert, "Value violates constraint for LSP_VersionNumber: -1.");
}


void test_TDVN()
{
  LSP_VersionNumber n(34);
  TD_VersionNumber n2 = n.toTD_VersionNumber();
  LSP_VersionNumber n3 = LSP_VersionNumber::fromTDVN(n2);
  EXPECT_EQ(n3.get(), 34);

  EXPECT_COMPARE(n, n2, 0);
  EXPECT_COMPARE(n3, n2, 0);

  TD_VersionNumber tdbig(UINT64_C(0x80000000));
  EXPECT_EXN_SUBSTR(LSP_VersionNumber::fromTDVN(tdbig),
    XNumericConversion, "cannot be represented");

  // We can compare to it even though we cannot convert it.
  EXPECT_COMPARE(n, tdbig, -1);
}


CLOSE_ANONYMOUS_NAMESPACE


void test_lsp_version_number(CmdlineArgsSpan args)
{
  test_ctor();
  test_TDVN();
}


// EOF
