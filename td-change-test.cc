// td-change-test.cc
// Tests for `td-change` module.

#include "td-change.h"                 // module under test
#include "unit-tests.h"                // decl for my entry point

#include "line-index.h"                // LineIndex
#include "positive-line-count.h"       // PositiveLineCount
#include "td-core.h"                   // TextDocumentCore

#include "smbase/sm-macros.h"          // OPEN_ANONYMOUS_NAMESPACE
#include "smbase/sm-test.h"            // EXPECT_EQ


OPEN_ANONYMOUS_NAMESPACE


void test_1()
{
  TextDocumentCore doc;
  EXPECT_EQ(doc.getWholeFileString(), "");

  {
    TDC_TotalChange change(PositiveLineCount(4), "zero\none\ntwo\n");
    change.applyToDoc(doc);
    EXPECT_EQ(doc.getWholeFileString(), "zero\none\ntwo\n");
    EXPECT_EQ(doc.numLines(), 4);
  }

  {
    TDC_InsertLine change(LineIndex(1), {});
    change.applyToDoc(doc);
    EXPECT_EQ(doc.getWholeFileString(), "zero\n\none\ntwo\n");
  }

  {
    TDC_DeleteLine change(LineIndex(1), {});
    change.applyToDoc(doc);
    EXPECT_EQ(doc.getWholeFileString(), "zero\none\ntwo\n");
  }

  {
    TDC_InsertText change(TextMCoord(LineIndex(1), 1), "XYZ");
    change.applyToDoc(doc);
    EXPECT_EQ(doc.getWholeFileString(), "zero\noXYZne\ntwo\n");
  }

  {
    TDC_DeleteText change(TextMCoord(LineIndex(1), 2), 3);
    change.applyToDoc(doc);
    EXPECT_EQ(doc.getWholeFileString(), "zero\noXe\ntwo\n");
  }
}


CLOSE_ANONYMOUS_NAMESPACE


// Called from unit-tests.cc.
void test_td_change(CmdlineArgsSpan args)
{
  test_1();
}


// EOF
