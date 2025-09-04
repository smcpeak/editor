// td-line-test.cc
// Tests for `td-line` module.

#include "td-line.h"                   // module under test
#include "unit-tests.h"                // decl for my entry point

#include "smbase/sm-test.h"            // EXPECT_EQ, op_eq
#include "smbase/xassert.h"            // xassert


class TextDocumentLineTester {
public:      // methods
  void test_equals()
  {
    TextDocumentLine tdl1, tdl2;
    xassert(op_eq(tdl1, tdl2));

    char arr1[] = "abc";
    tdl1.m_bytes = arr1;
    tdl1.m_length = 3;
    tdl1.selfCheck();
    xassert(!op_eq(tdl1, tdl2));
    xassert(tdl1.length() == 3);

    char arr2[] = "xabc";
    tdl2.m_bytes = arr2;
    tdl2.m_length = 3;
    tdl2.selfCheck();
    xassert(!op_eq(tdl1, tdl2));

    ++tdl2.m_bytes;
    tdl2.selfCheck();
    xassert(op_eq(tdl1, tdl2));
  }
};


// Called from unit-tests.cc.
void test_td_line(CmdlineArgsSpan args)
{
  TextDocumentLineTester().test_equals();
}


// EOF
