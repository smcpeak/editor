// makefile-hilite-test.cc
// Test code for makefile_hilite.{h,lex}.

#include "unit-tests.h"                // decl for my entry point

#include "makefile_hilite.h"           // module to test

#include "td-editor.h"                 // TextDocumentAndEditor


// Called from unit-tests.cc.
void test_makefile_hilite(CmdlineArgsSpan args)
{
  TextDocumentAndEditor tde;
  Makefile_Highlighter hi(tde.getDocument()->getCore());
  testHighlighter(hi, tde, "test/highlight/mk1.mk");
}


// EOF
