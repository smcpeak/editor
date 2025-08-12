// named-td-editor-test.cc
// Tests for `named-td-editor` module.

#include "unit-tests.h"                // decl for mye entry point

#include "named-td-editor.h"           // module under test

#include "doc-name.h"                  // DocumentName
#include "named-td.h"                  // NamedTextDocument

#include "smbase/sm-macros.h"          // OPEN_ANONYMOUS_NAMESPACE
#include "smbase/sm-test.h"            // EXPECT_EQ


OPEN_ANONYMOUS_NAMESPACE


void test_applyCommandSubstitutions()
{
  NamedTextDocument doc;
  doc.appendString(
R"(zero
one
4b45a58a (Scott McPeak 2024-05-25 05:23:07 -0700 40)   // Open, throwing XSysError on failure.
three
)");

  NamedTextDocumentEditor ntde(&doc);

  // Initially it has no file name.
  EXPECT_EQ(ntde.applyCommandSubstitutions("$f"),
    "\"\"");

  doc.setDocumentName(DocumentName::fromFilename(
    HostName::asLocal(), "tmp.h"));
  EXPECT_EQ(ntde.applyCommandSubstitutions("$f"),
    "tmp.h");
  EXPECT_EQ(ntde.applyCommandSubstitutions("abc $f def $f hij"),
    "abc tmp.h def tmp.h hij");

  // This isn't necessarily ideal, but it is the current behavior.
  EXPECT_EQ(ntde.applyCommandSubstitutions("$$f"),
    "$tmp.h");

  doc.setDocumentName(DocumentName::fromFilename(
    HostName::asLocal(), "d1/d2/foo.txt"));
  EXPECT_EQ(ntde.applyCommandSubstitutions("$f"),
    "foo.txt");


  ntde.setCursor(TextLCoord(0,0));
  EXPECT_EQ(ntde.applyCommandSubstitutions("$w"), "zero");

  ntde.setCursor(TextLCoord(1,1));
  EXPECT_EQ(ntde.applyCommandSubstitutions("$w"), "ne");

  ntde.setCursor(TextLCoord(2,0));
  EXPECT_EQ(ntde.applyCommandSubstitutions("$w"), "4b45a58a");

  ntde.setCursor(TextLCoord(1,10));
  EXPECT_EQ(ntde.applyCommandSubstitutions("$w"), "\"\"");

  ntde.setCursor(TextLCoord(10,1));
  EXPECT_EQ(ntde.applyCommandSubstitutions("$w"), "\"\"");
}


CLOSE_ANONYMOUS_NAMESPACE


// Called from unit-tests.cc.
void test_named_td_editor(CmdlineArgsSpan)
{
  test_applyCommandSubstitutions();
}


// EOF
