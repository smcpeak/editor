// textmcoord-test.cc
// Tests for `textmcoord` module.

#include "unit-tests.h"                          // decl for my entry point
#include "textmcoord.h"                          // module under test

#include "smbase/gdvn-test-roundtrip.h"          // gdvnTestRoundtrip
#include "smbase/sm-macros.h"                    // OPEN_ANONYMOUS_NAMESPACE
#include "smbase/sm-test.h"                      // EXPECT_EQ

using namespace smbase;


OPEN_ANONYMOUS_NAMESPACE


void test_serialization()
{
  TextMCoordRange mcr(
    TextMCoord(LineIndex(1), ByteIndex(2)),
    TextMCoord(LineIndex(3), ByteIndex(4)));

  gdvnTestRoundtrip(mcr, "MCR(MC(1 2) MC(3 4))");
}


CLOSE_ANONYMOUS_NAMESPACE


// Called from unit-tests.cc.
void test_textmcoord(CmdlineArgsSpan args)
{
  test_serialization();
}


// EOF
