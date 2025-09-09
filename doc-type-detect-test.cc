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
  testOne_detectFileType("f", DocumentType::KDT_UNKNOWN);
  testOne_detectFileType("foo.cc", DocumentType::KDT_C);
  testOne_detectFileType("foo.diff", DocumentType::KDT_DIFF);
  testOne_detectFileType("foo.patch", DocumentType::KDT_DIFF);
  testOne_detectFileType("foo.patch.cc", DocumentType::KDT_C);

  testOne_detectFileType("some-test.ev", DocumentType::KDT_C);

  testOne_detectDocumentType(cmdDocName("differences"), DocumentType::KDT_UNKNOWN);
  testOne_detectDocumentType(cmdDocName("diff ere nces"), DocumentType::KDT_DIFF);
  testOne_detectDocumentType(cmdDocName("git diff ere nces"), DocumentType::KDT_DIFF);
  testOne_detectDocumentType(cmdDocName("gitdiff ere nces"), DocumentType::KDT_UNKNOWN);

  testOne_detectFileType("iostream", DocumentType::KDT_C);
  testOne_detectFileType("string", DocumentType::KDT_C);
  testOne_detectFileType("iostream_other", DocumentType::KDT_UNKNOWN);
  testOne_detectFileType("other_iostream", DocumentType::KDT_UNKNOWN);

  testOne_detectFileType("ostream.tcc", DocumentType::KDT_C);
  testOne_detectFileType("something.json", DocumentType::KDT_C);
  testOne_detectFileType("else.gdvn", DocumentType::KDT_C);
}


CLOSE_ANONYMOUS_NAMESPACE


// Called from unit-tests.cc.
void test_doc_type_detect(CmdlineArgsSpan args)
{
  TEST_CASE("test_doc_type_detect");

  test_detectDocumentType();
}


// EOF
