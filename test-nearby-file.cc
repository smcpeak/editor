// test-nearby-file.cc
// Test code for 'nearby-file' module.

#include "nearby-file.h"               // module to test

#include "sm-file-util.h"              // TestSMFileUtil
#include "sm-iostream.h"               // cout, etc.
#include "test.h"                      // USUAL_TEST_MAIN


static void expectIGNF(
  SMFileUtil &sfu,
  ArrayStack<string> const &candidatePrefixes,
  string const &haystack,
  int charOffset,
  string const &expect)
{
  string actual = innerGetNearbyFilename(sfu,
    candidatePrefixes, haystack, charOffset);
  EXPECT_EQ(actual, expect);
}


static void test1()
{
  TestSMFileUtil sfu;
  sfu.m_existingPaths.add("/home/foo.txt");
  sfu.m_existingPaths.add("/home/user/foo.txt");
  sfu.m_existingPaths.add("/home/user/bar.txt");

  ArrayStack<string> prefixes;

  // No prefixes.
  expectIGNF(sfu, prefixes, "anything", 0, "");

  prefixes.push("/home");
  expectIGNF(sfu, prefixes, "foo.txt", 0, "/home/foo.txt");
  expectIGNF(sfu, prefixes, "foo.txt", 6, "/home/foo.txt");
  expectIGNF(sfu, prefixes, "foo.txt", 7, "/home/foo.txt");

  // Empty input line.
  expectIGNF(sfu, prefixes, "", 0, "");

  // Offset out of bounds.
  expectIGNF(sfu, prefixes, "foo.txt", -1, "");
  expectIGNF(sfu, prefixes, "foo.txt", 8, "");

  // No absolute search path yet, but this is the result when nothing
  // found and the start string is absolute, so it's hard to see the
  // effect...
  expectIGNF(sfu, prefixes, "/home/foo.txt", 3, "/home/foo.txt");

  // Now it will work.
  prefixes.push("");
  expectIGNF(sfu, prefixes, "/home/foo.txt", 3, "/home/foo.txt");

  // Prefix priority.
  expectIGNF(sfu, prefixes, "bar.txt", 0, "/home/bar.txt");  // not found
  prefixes.push("/home/user");
  expectIGNF(sfu, prefixes, "foo.txt", 0, "/home/foo.txt");  // still
  expectIGNF(sfu, prefixes, "bar.txt", 0, "/home/user/bar.txt");   // now found

  // Range of file name characters.  None exist, that's fine.
  expectIGNF(sfu, prefixes, "ab cd ef", 3,
                         "/home/cd");
  expectIGNF(sfu, prefixes, "ab cd ef", 4,
                         "/home/cd");
  expectIGNF(sfu, prefixes, "ab cd ef", 5,
                         "/home/cd");

  // Test inclusion.
  expectIGNF(sfu, prefixes, "ab cAZaz90_d ef", 7,
                         "/home/cAZaz90_d");
  expectIGNF(sfu, prefixes, "ab z/\\-_.cAZaz90_d ef", 7, "");
  expectIGNF(sfu, prefixes, "ab z/\\-_.cAZaz90_d ef", 10,
                         "/home/z/\\-_.cAZaz90_d");

  // Test exclusion.
  expectIGNF(sfu, prefixes, "ab \"cd\" ef", 5,
                           "/home/cd");
  expectIGNF(sfu, prefixes, "ab <cd> ef", 5,
                          "/home/cd");
  expectIGNF(sfu, prefixes, "ab 'cd' ef", 5,
                          "/home/cd");

  // Test that we ignore starting on "//".
  expectIGNF(sfu, prefixes, "// blah", 0, "");
  expectIGNF(sfu, prefixes, "//blah", 0, "");
  expectIGNF(sfu, prefixes, "/blah", 0, "/blah");

  // Ignore trailing punctuation.
  expectIGNF(sfu, prefixes, "foo.txt.", 0, "/home/foo.txt");
  expectIGNF(sfu, prefixes, "foo.txt.", 6, "/home/foo.txt");
}


static void entry()
{
  test1();

  cout << "test-nearby-file ok" << endl;
}

USUAL_TEST_MAIN

// EOF
