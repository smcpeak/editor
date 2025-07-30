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


void testOne_detectDocumentType(
  DocumentName const &docName,
  KnownDocumentType expect)
{
  EXPECT_EQ(detectDocumentType(docName), expect);
}


void test_detectDocumentType()
{
  testOne_detectDocumentType(fileDocName("f"), KDT_UNKNOWN);
  testOne_detectDocumentType(fileDocName("foo.cc"), KDT_C);
  testOne_detectDocumentType(fileDocName("foo.diff"), KDT_DIFF);
  testOne_detectDocumentType(fileDocName("foo.patch"), KDT_DIFF);
  testOne_detectDocumentType(fileDocName("foo.patch.cc"), KDT_C);

  testOne_detectDocumentType(cmdDocName("differences"), KDT_UNKNOWN);
  testOne_detectDocumentType(cmdDocName("diff ere nces"), KDT_DIFF);
  testOne_detectDocumentType(cmdDocName("git diff ere nces"), KDT_DIFF);
  testOne_detectDocumentType(cmdDocName("gitdiff ere nces"), KDT_UNKNOWN);
}


CLOSE_ANONYMOUS_NAMESPACE


// Called from named-td-test.cc.
void test_doc_type_detect()
{
  test_detectDocumentType();
}


// EOF
