// ocaml-hilite-test.cc
// Test code for ocaml_hilite.{h,lex}.

#include "unit-tests.h"                // decl for my entry point

#include "ocaml_hilite.h"              // module to test

#include "td-editor.h"                 // TextDocumentAndEditor



// Called from unit-tests.cc.
void test_ocaml_hilite(CmdlineArgsSpan args)
{
  TextDocumentAndEditor tde;
  OCaml_Highlighter hi(tde.getDocument()->getCore());
  testHighlighter(hi, tde, "test/highlight/ocaml1.ml");
}


// EOF
