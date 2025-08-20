// td-line-test.cc
// Tests for `td-line` module.

#include "td-line.h"                   // module under test
#include "unit-tests.h"                // decl for my entry point

#include "smbase/sm-macros.h"          // OPEN_ANONYMOUS_NAMESPACE
#include "smbase/sm-test.h"            // EXPECT_EQ, op_eq


OPEN_ANONYMOUS_NAMESPACE


void test_equals()
{
  TextDocumentLine tdl1, tdl2;
  xassert(op_eq(tdl1, tdl2));

  char arr1[] = "abc\n";
  tdl1.m_bytes = arr1;
  tdl1.m_length = 4;
  xassert(!op_eq(tdl1, tdl2));
  xassert(tdl1.lengthWithoutNL() == 3);

  char arr2[] = "xabc\n";
  tdl2.m_bytes = arr2;
  tdl2.m_length = 4;
  xassert(!op_eq(tdl1, tdl2));

  ++tdl2.m_bytes;
  xassert(op_eq(tdl1, tdl2));
}


CLOSE_ANONYMOUS_NAMESPACE


// Called from unit-tests.cc.
void test_td_line(CmdlineArgsSpan args)
{
  test_equals();
}


// EOF
