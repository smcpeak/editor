// editor-strutil-test.cc
// Test code for 'editor-strutil' module.

#include "editor-strutil.h"            // this module

// smbase
#include "smbase/sm-iostream.h"        // cout
#include "smbase/sm-test.h"            // USUAL_TEST_MAIN


static void expectCIA(string const &text, int byteOffset,
                      string const &expect)
{
  string actual = cIdentifierAt(text, byteOffset);
  EXPECT_EQ(actual, expect);
}


static void test1()
{
  expectCIA("", -1, "");
  expectCIA("", 0, "");
  expectCIA("", 1, "");

  expectCIA(" ", -1, "");
  expectCIA(" ", 0, "");
  expectCIA(" ", 1, "");

  expectCIA("$", -1, "");
  expectCIA("$", 0, "");
  expectCIA("$", 1, "");

  expectCIA("a", -1, "");
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


static void entry()
{
  test1();

  cout << "editor-strutil-test ok" << endl;
}

USUAL_TEST_MAIN


// EOF
