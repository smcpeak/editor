// range-text-repl-test.cc
// Tests for `range-text-repl` module.

#include "range-text-repl.h"           // module under test
#include "unit-tests.h"                // decl for my entry point

#include "smbase/gdvalue.h"            // needed by EXPECT_EQ_GDVSER
#include "smbase/sm-macros.h"          // OPEN_ANONYMOUS_NAMESPACE
#include "smbase/sm-test.h"            // EXPECT_EQ, EXPECT_EQ_GDVSER

#include <optional>                    // std::make_optional
#include <string>                      // std::string

using namespace gdv;


OPEN_ANONYMOUS_NAMESPACE


// Make a coordinate range vaguely based on `n`.
TextMCoordRange makeMCR(int n)
{
  return TextMCoordRange(
    TextMCoord(LineIndex(n), n),
    TextMCoord(LineIndex(n+1), n+1));
}


void test_ConstructWithLvalueString()
{
  TextMCoordRange range = makeMCR(5);
  std::string text = "hello";

  RangeTextReplacement r(std::make_optional(range), text);

  xassert(r.m_range.has_value());
  EXPECT_EQ_GDVSER(*r.m_range, range);
  EXPECT_EQ(r.m_text, "hello");
}


void test_ConstructWithRvalueString()
{
  TextMCoordRange range = makeMCR(10);
  RangeTextReplacement r(std::make_optional(range), std::string("world"));

  xassert(r.m_range.has_value());
  EXPECT_EQ_GDVSER(*r.m_range, range);
  EXPECT_EQ(r.m_text, "world");
}


void test_ConstructWithNoRange()
{
  RangeTextReplacement r(std::nullopt, "replace all");

  xassert(!r.m_range.has_value());
  EXPECT_EQ(r.m_text, "replace all");
}


void test_MoveConstructorTransfersOwnership()
{
  TextMCoordRange range = makeMCR(2);
  RangeTextReplacement original(std::make_optional(makeMCR(2)), "abc");
  RangeTextReplacement moved(std::move(original));

  xassert(moved.m_range.has_value());
  EXPECT_EQ_GDVSER(*moved.m_range, range);
  EXPECT_EQ(moved.m_text, "abc");

  // I expect the original string to have been emptied.
  xassert(original.m_text.empty());
}

void test_MoveAssignmentTransfersOwnership()
{
  RangeTextReplacement a(std::make_optional(makeMCR(4)), "first");
  RangeTextReplacement b(std::make_optional(makeMCR(10)), "second");

  b = std::move(a);

  xassert(b.m_range.has_value());
  EXPECT_EQ_GDVSER(*b.m_range, makeMCR(4));
  EXPECT_EQ(b.m_text, "first");

  xassert(a.m_text.empty());
}


CLOSE_ANONYMOUS_NAMESPACE


// Called from unit-tests.cc.
void test_range_text_repl(CmdlineArgsSpan args)
{
  test_ConstructWithLvalueString();
  test_ConstructWithRvalueString();
  test_ConstructWithNoRange();
  test_MoveConstructorTransfersOwnership();
  test_MoveAssignmentTransfersOwnership();
}


// EOF
