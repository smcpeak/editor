// doc-type-detect-test.cc
// Tests for `doc-type-detect` module.

#include "unit-tests.h"                // decl for my entry point

#include "doc-type-detect.h"           // module under test

#include "doc-name.h"                  // DocumentName
#include "doc-type.h"                  // DocumentType

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
  testOne_detectFileType("f", DocumentType::DT_UNKNOWN);
  testOne_detectFileType("foo.cc", DocumentType::DT_C);
  testOne_detectFileType("foo.diff", DocumentType::DT_DIFF);
  testOne_detectFileType("foo.patch", DocumentType::DT_DIFF);
  testOne_detectFileType("foo.patch.cc", DocumentType::DT_C);

  testOne_detectFileType("some-test.ev", DocumentType::DT_C);

  testOne_detectDocumentType(cmdDocName("differences"), DocumentType::DT_UNKNOWN);
  testOne_detectDocumentType(cmdDocName("diff ere nces"), DocumentType::DT_DIFF);
  testOne_detectDocumentType(cmdDocName("git diff ere nces"), DocumentType::DT_DIFF);
  testOne_detectDocumentType(cmdDocName("gitdiff ere nces"), DocumentType::DT_UNKNOWN);

  testOne_detectFileType("iostream", DocumentType::DT_C);
  testOne_detectFileType("string", DocumentType::DT_C);
  testOne_detectFileType("iostream_other", DocumentType::DT_UNKNOWN);
  testOne_detectFileType("other_iostream", DocumentType::DT_UNKNOWN);

  testOne_detectFileType("ostream.tcc", DocumentType::DT_C);
  testOne_detectFileType("something.json", DocumentType::DT_C);
  testOne_detectFileType("else.gdvn", DocumentType::DT_C);
}


CLOSE_ANONYMOUS_NAMESPACE


// Called from unit-tests.cc.
void test_doc_type_detect(CmdlineArgsSpan args)
{
  TEST_CASE("test_doc_type_detect");

  test_detectDocumentType();
}


// EOF
