// hashcomment-hilite-test.cc
// Test code for hashcomment_hilite.{h,lex}.

#include "unit-tests.h"                // decl for my entry point

#include "hashcomment_hilite.h"        // module to test

#include "td-editor.h"                 // TextDocumentAndEditor


// Called from unit-tests.cc.
void test_hashcomment_hilite(CmdlineArgsSpan args)
{
  TextDocumentAndEditor tde;
  HashComment_Highlighter hi(tde.getDocument()->getCore());
  testHighlighter(hi, tde, "test/highlight/hashcomment1.sh");
}


// EOF
