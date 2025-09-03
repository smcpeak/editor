// editor-strutil-test.cc
// Tests for 'editor-strutil' module.

#include "unit-tests.h"                // decl for my entry point

#include "editor-strutil.h"            // module under test

#include "byte-index.h"                // ByteIndex

#include "smbase/sm-macros.h"          // OPEN_ANONYMOUS_NAMESPACE
#include "smbase/sm-test.h"            // EXPECT_EQ

#include <string>                      // std::string


OPEN_ANONYMOUS_NAMESPACE


void expectCIA(std::string const &text, int byteOffset,
               std::string const &expect)
{
  std::string actual = cIdentifierAt(text, ByteIndex(byteOffset));
  EXPECT_EQ(actual, expect);
}


void test1()
{
  expectCIA("", 0, "");
  expectCIA("", 1, "");

  expectCIA(" ", 0, "");
  expectCIA(" ", 1, "");

  expectCIA("$", 0, "");
  expectCIA("$", 1, "");

  expectCIA("a", 0, "a");
  expectCIA("a", 1, "");

  expectCIA("abc", 0, "abc");
  expectCIA("abc", 1, "abc");
  expectCIA("abc", 2, "abc");
  expectCIA("abc", 3, "");

  expectCIA(" abc ", 0, "");
  expectCIA(" abc ", 1, "abc");
  expectCIA(" abc ", 2, "abc");
  expectCIA(" abc ", 3, "abc");
  expectCIA(" abc ", 4, "");

  expectCIA(" azAZ_09 ", 4, "azAZ_09");
  expectCIA("$azAZ_09-", 4, "azAZ_09");
}


CLOSE_ANONYMOUS_NAMESPACE


// Called from unit-tests.cc.
void test_editor_strutil(CmdlineArgsSpan args)
{
  test1();
}


// EOF
