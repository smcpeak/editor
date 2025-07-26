// doc-type-detect-test.cc
// Tests for `doc-type-detect` module.

#include "doc-type-detect.h"           // module under test

#include "doc-name.h"                  // DocumentName

#include "smbase/sm-macros.h"          // OPEN_ANONYMOUS_NAMESPACE
#include "smbase/sm-test.h"            // EXPECT_EQ


OPEN_ANONYMOUS_NAMESPACE


DocumentName fileDocName(char const *fname)
{
  return DocumentName::fromFilename(HostName::asLocal(), fname);
}

DocumentName cmdDocName(char const *cmd)
{
  return DocumentName::fromNonFileResourceName(
    HostName::asLocal(), cmd, "some/dir");
}


void test_isDiffName()
{
  EXPECT_EQ(isDiffName(fileDocName("f")), false);
  EXPECT_EQ(isDiffName(fileDocName("foo.cc")), false);
  EXPECT_EQ(isDiffName(fileDocName("foo.diff")), true);
  EXPECT_EQ(isDiffName(fileDocName("foo.patch")), true);
  EXPECT_EQ(isDiffName(fileDocName("foo.patch.cc")), false);

  EXPECT_EQ(isDiffName(cmdDocName("differences")), false);
  EXPECT_EQ(isDiffName(cmdDocName("diff ere nces")), true);
  EXPECT_EQ(isDiffName(cmdDocName("git diff ere nces")), true);
  EXPECT_EQ(isDiffName(cmdDocName("gitdiff ere nces")), false);
}


CLOSE_ANONYMOUS_NAMESPACE


// Called from named-td-test.cc.
void test_doc_type_detect()
{
  test_isDiffName();
}


// EOF
