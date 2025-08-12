// named-td-editor-test.cc
// Tests for `named-td-editor` module.

#include "named-td-editor.h"           // module under test

#include "doc-name.h"                  // DocumentName
#include "named-td.h"                  // NamedTextDocument

#include "smbase/sm-macros.h"          // OPEN_ANONYMOUS_NAMESPACE
#include "smbase/sm-test.h"            // EXPECT_EQ


OPEN_ANONYMOUS_NAMESPACE


void test_applyCommandSubstitutions()
{
  NamedTextDocument doc;
  NamedTextDocumentEditor ntde(&doc);

  // Initially it has no file name.
  EXPECT_EQ(ntde.applyCommandSubstitutions("$f"),
    "''");

  doc.setDocumentName(DocumentName::fromFilename(
    HostName::asLocal(), "tmp.h"));
  EXPECT_EQ(ntde.applyCommandSubstitutions("$f"),
    "tmp.h");
  EXPECT_EQ(ntde.applyCommandSubstitutions("abc $f def $f hij"),
    "abc tmp.h def tmp.h hij");

  // This isn't necessariliy ideal, but it is the current behavior.
  EXPECT_EQ(ntde.applyCommandSubstitutions("$$f"),
    "$tmp.h");

  doc.setDocumentName(DocumentName::fromFilename(
    HostName::asLocal(), "d1/d2/foo.txt"));
  EXPECT_EQ(ntde.applyCommandSubstitutions("$f"),
    "foo.txt");
}


CLOSE_ANONYMOUS_NAMESPACE


void test_named_td_editor()
{
  test_applyCommandSubstitutions();
}


// EOF
