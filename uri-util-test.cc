// uri-util-test.cc
// Tests for `uri-util` module.

#include "uri-util.h"                  // module under test

#include "smbase/exc.h"                // smbase::XFormat
#include "smbase/sm-macros.h"          // OPEN_ANONYMOUS_NAMESPACE
#include "smbase/sm-test.h"            // EXPECT_EQ

using namespace smbase;


OPEN_ANONYMOUS_NAMESPACE


void roundTripFileToURL(char const *file, char const *expectURI)
{
  std::string actualURI = makeFileURI(file);
  EXPECT_EQ(actualURI, expectURI);

  std::string decodedFile = getFileURIPath(actualURI);
  EXPECT_EQ(decodedFile, file);
}


void test_makeFileURI()
{
  roundTripFileToURL("/a/b/c",
              "file:///a/b/c");

  roundTripFileToURL("c:/users/user/foo.h",
             "file:///c:/users/user/foo.h");
}


void test_getFileURIPath()
{
  // The valid cases are tested as part of round-trip above, so here I
  // just focus on error cases.

  EXPECT_EXN_SUBSTR(getFileURIPath("http://example.com"),
    XFormat, "URI does not begin with \"file://\".");

  EXPECT_EXN_SUBSTR(getFileURIPath("file:///a/b/c%41"),
    XFormat, "URI uses percent encoding but I can't handle that.");

  EXPECT_EXN_SUBSTR(getFileURIPath("file:///a/b/c?q=4"),
    XFormat, "URI has a query part but I can't handle that.");

  EXPECT_EXN_SUBSTR(getFileURIPath("user@file:///a/b/c"),
    XFormat, "URI has a user name part but I can't handle that.");
}


CLOSE_ANONYMOUS_NAMESPACE


// Called from unit-tests.cc.
void test_uri_util()
{
  test_makeFileURI();
  test_getFileURIPath();
}


// EOF
