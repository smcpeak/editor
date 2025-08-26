// td-test.cc
// Tests for `td` module.

// The bulk of the testing is done in `td-editor-test`, but there are
// some things to test specifically here too.

#include "td.h"                        // module under test
#include "unit-tests.h"                // decl for my entry point

#include "range-text-repl.h"           // RangeTextReplacement

#include "smbase/gdvalue.h"            // gdv::GDValue for TEST_CASE_EXPRS
#include "smbase/sm-macros.h"          // OPEN_ANONYMOUS_NAMESPACE
#include "smbase/sm-test.h"            // EXPECT_EQ

using namespace gdv;


OPEN_ANONYMOUS_NAMESPACE


void replaceRange(
  TextDocument &doc,
  int startLine,
  int startByteIndex,
  int endLine,
  int endByteIndex,
  char const *text)
{
  doc.replaceMultilineRange(
    TextMCoordRange(
      TextMCoord(LineIndex(startLine), startByteIndex),
      TextMCoord(LineIndex(endLine), endByteIndex)),
    text);
}


void testOne_replaceMultilineRange(
  TextDocument &doc,
  int startLine,
  int startByteIndex,
  int endLine,
  int endByteIndex,
  char const *text,
  char const *expect)
{
  TEST_CASE_EXPRS("testOne_replaceMultilineRange",
    startLine,
    startByteIndex,
    endLine,
    endByteIndex,
    text);

  replaceRange(
    doc,
    startLine,
    startByteIndex,
    endLine,
    endByteIndex,
    text);
  EXPECT_EQ(doc.getWholeFileString(), expect);
}


// Copy+paste from `td-core-test.cc`.
void test_replaceMultilineRange()
{
  TextDocument doc;
  EXPECT_EQ(doc.getWholeFileString(), "");
  EXPECT_EQ(doc.historyLength(), 0);

  testOne_replaceMultilineRange(doc, 0,0,0,0, "zero\none\n",
    "zero\n"
    "one\n");
  EXPECT_EQ(doc.historyLength(), 1);

  testOne_replaceMultilineRange(doc, 2,0,2,0, "two\nthree\n",
    "zero\n"
    "one\n"
    "two\n"
    "three\n");
  EXPECT_EQ(doc.historyLength(), 2);

  testOne_replaceMultilineRange(doc, 1,1,2,2, "XXXX\nYYYY",
    "zero\n"
    "oXXXX\n"
    "YYYYo\n"
    "three\n");
  EXPECT_EQ(doc.historyLength(), 3);

  testOne_replaceMultilineRange(doc, 0,4,3,0, "",
    "zerothree\n");
  EXPECT_EQ(doc.historyLength(), 4);

  testOne_replaceMultilineRange(doc, 0,9,1,0, "",
    "zerothree");
  EXPECT_EQ(doc.historyLength(), 5);

  testOne_replaceMultilineRange(doc, 0,2,0,3, "0\n1\n2\n3",
    "ze0\n"
    "1\n"
    "2\n"
    "3othree");
  EXPECT_EQ(doc.historyLength(), 6);
}


void test_applyRangeTextReplacement()
{
  TextDocument doc;
  EXPECT_EQ(doc.getWholeFileString(), "");

  {
    RangeTextReplacement repl(std::nullopt, "zero\none\ntwo\n");
    doc.applyRangeTextReplacement(repl);
    EXPECT_EQ(doc.getWholeFileString(), "zero\none\ntwo\n");
  }

  {
    RangeTextReplacement repl(
      TextMCoordRange(
        TextMCoord(LineIndex(1), 2),
        TextMCoord(LineIndex(2), 1)),
      "ABC");
    doc.applyRangeTextReplacement(repl);
    EXPECT_EQ(doc.getWholeFileString(), "zero\nonABCwo\n");
  }

  {
    RangeTextReplacement repl(std::nullopt, "zxc");
    doc.applyRangeTextReplacement(repl);
    EXPECT_EQ(doc.getWholeFileString(), "zxc");
  }
}


CLOSE_ANONYMOUS_NAMESPACE


// Called from unit-tests.cc.
void test_td(CmdlineArgsSpan args)
{
  test_replaceMultilineRange();
  test_applyRangeTextReplacement();
}


// EOF
