// doc-type-detect-test.cc
// Tests for `doc-type-detect` module.

#include "unit-tests.h"                // decl for my entry point

#include "doc-type-detect.h"           // module under test

#include "doc-name.h"                  // DocumentName

#include "smbase/sm-macros.h"          // OPEN_ANONYMOUS_NAMESPACE
#include "smbase/sm-test.h"            // EXPECT_EQ, TEST_CASE


OPEN_ANONYMOUS_NAMESPACE


DocumentName fileDocName(char const *fname)
{
  return DocumentName::fromLocalFilename(fname);
}

DocumentName cmdDocName(char const *cmd)
{
  return DocumentName::fromNonFileResourceName(
    HostName::asLocal(), cmd, "some/dir");
}


void testOne_detectDocumentType(
  DocumentName const &docName,
  DocumentType expect)
{
  EXPECT_EQ(detectDocumentType(docName), expect);
}

void testOne_detectFileType(
  char const *fname,
  DocumentType expect)
{
  testOne_detectDocumentType(fileDocName(fname), expect);
}


void test_detectDocumentType()
{
  testOne_detectFileType("f", KDT_UNKNOWN);
  testOne_detectFileType("foo.cc", KDT_C);
  testOne_detectFileType("foo.diff", KDT_DIFF);
  testOne_detectFileType("foo.patch", KDT_DIFF);
  testOne_detectFileType("foo.patch.cc", KDT_C);

  testOne_detectFileType("some-test.ev", KDT_C);

  testOne_detectDocumentType(cmdDocName("differences"), KDT_UNKNOWN);
  testOne_detectDocumentType(cmdDocName("diff ere nces"), KDT_DIFF);
  testOne_detectDocumentType(cmdDocName("git diff ere nces"), KDT_DIFF);
  testOne_detectDocumentType(cmdDocName("gitdiff ere nces"), KDT_UNKNOWN);

  testOne_detectFileType("iostream", KDT_C);
  testOne_detectFileType("string", KDT_C);
  testOne_detectFileType("iostream_other", KDT_UNKNOWN);
  testOne_detectFileType("other_iostream", KDT_UNKNOWN);

  testOne_detectFileType("ostream.tcc", KDT_C);
  testOne_detectFileType("something.json", KDT_C);
  testOne_detectFileType("else.gdvn", KDT_C);
}


CLOSE_ANONYMOUS_NAMESPACE


// Called from unit-tests.cc.
void test_doc_type_detect(CmdlineArgsSpan args)
{
  TEST_CASE("test_doc_type_detect");

  test_detectDocumentType();
}


// EOF
