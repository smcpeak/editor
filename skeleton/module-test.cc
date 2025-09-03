// {module}-test.cc
// Tests for `{module}` module.

#include "unit-tests.h"                // decl for my entry point
#include "{module}.h"                  // module under test

#include "smbase/sm-macros.h"          // OPEN_ANONYMOUS_NAMESPACE
#include "smbase/sm-test.h"            // EXPECT_EQ

using namespace smbase;


OPEN_ANONYMOUS_NAMESPACE


void test_basics()
{
  // ...
}


CLOSE_ANONYMOUS_NAMESPACE


// Called from unit-tests.cc.
void test_{module_with_underscores}(CmdlineArgsSpan args)
{
  test_basics();
}


// EOF
