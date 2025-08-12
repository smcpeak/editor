// uri-util-test.cc
// Tests for `uri-util` module.

#include "unit-tests.h"                // decl for my entry point

#include "uri-util.h"                  // module under test

#include "smbase/exc.h"                // smbase::XFormat
#include "smbase/gdvalue.h"            // gdv::GDValue (for TEST_CASE_EXPRS)
#include "smbase/ordered-map.h"        // smbase::OrderedMap (for TEST_CASE_EXPRS)
#include "smbase/sm-macros.h"          // OPEN_ANONYMOUS_NAMESPACE
#include "smbase/sm-platform.h"        // PLATFORM_IS_WINDOWS
#include "smbase/sm-test.h"            // EXPECT_EQ, TEST_CASE_EXPRS

using namespace gdv;
using namespace smbase;


OPEN_ANONYMOUS_NAMESPACE


void roundTripFileToURL(char const *file, char const *expectURI)
{
  TEST_CASE_EXPRS("roundTripFileToURL", file);

  std::string actualURI = makeFileURI(file);
  EXPECT_EQ(actualURI, expectURI);

  std::string decodedFile = getFileURIPath(actualURI);
  EXPECT_EQ(decodedFile, file);
}


void test_makeFileURI()
{
  roundTripFileToURL("/a/b/c",
              "file:///a/b/c");

  if (PLATFORM_IS_WINDOWS) {
    roundTripFileToURL("c:/users/user/foo.h",
               "file:///c:/users/user/foo.h");
  }
  else {
    // On POSIX, "c:/..." is not considered absolute, so the current dir
    // gets prepended.  Skip the test for now.
  }

  roundTripFileToURL("/a/b/c++",
              "file:///a/b/c%2B%2B");
}


void test_getFileURIPath()
{
  // The valid cases are tested as part of round-trip above, so here I
  // just focus on error cases.

  EXPECT_EXN_SUBSTR(getFileURIPath("http://example.com"),
    XFormat, "URI does not begin with \"file://\".");

  EXPECT_EXN_SUBSTR(getFileURIPath("file:///a/b/c?q=4"),
    XFormat, "URI has a query part but I can't handle that.");

  EXPECT_EXN_SUBSTR(getFileURIPath("user@file:///a/b/c"),
    XFormat, "URI has a user name part but I can't handle that.");
}


CLOSE_ANONYMOUS_NAMESPACE


// Called from unit-tests.cc.
void test_uri_util(CmdlineArgsSpan args)
{
  test_makeFileURI();
  test_getFileURIPath();
}


// EOF
