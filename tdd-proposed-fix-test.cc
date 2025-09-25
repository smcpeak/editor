// tdd-proposed-fix-test.cc
// Tests for `tdd-proposed-fix` module.

#include "unit-tests.h"                // decl for my entry point
#include "tdd-proposed-fix.h"          // module under test

#include "smbase/gdvalue-parser.h"     // gdv::GDValueParser
#include "smbase/gdvalue.h"            // gdv::GDValue
#include "smbase/sm-macros.h"          // OPEN_ANONYMOUS_NAMESPACE
#include "smbase/sm-test.h"            // EXPECT_EQ

using namespace gdv;
using namespace smbase;


OPEN_ANONYMOUS_NAMESPACE


void test_numFiles_numEdits()
{
  TDD_ProposedFix pfix(GDValueParser(fromGDVN(R"(
    TDD_ProposedFix[
      title: "something"
      changesForFile: {
        "file1": [
          TDD_TextEdit[
            range: MCR(MC(1 2) MC(3 4))
            newText: "t1"
          ]
        ]
        "file2": [
          TDD_TextEdit[
            range: MCR(MC(1 2) MC(3 4))
            newText: "t2"
          ]
          TDD_TextEdit[
            range: MCR(MC(11 12) MC(13 14))
            newText: "t3"
          ]
        ]
      }
    ]
  )")));

  EXPECT_EQ(pfix.numFiles(), 2);
  EXPECT_EQ(pfix.numEdits(), 3);
}


CLOSE_ANONYMOUS_NAMESPACE


// Called from unit-tests.cc.
void test_tdd_proposed_fix(CmdlineArgsSpan args)
{
  test_numFiles_numEdits();
}


// EOF
