// ocaml-hilite-test.cc
// Test code for ocaml_hilite.{h,lex}.

#include "ocaml_hilite.h"              // module to test

#include "td-editor.h"                 // TextDocumentAndEditor

#include "smbase/sm-test.h"            // USUAL_MAIN


void entry()
{
  TextDocumentAndEditor tde;
  OCaml_Highlighter hi(tde.getDocument()->getCore());
  testHighlighter(hi, tde, "test/highlight/ocaml1.ml");

  cout << "ocaml_hilite works\n";
}

USUAL_MAIN


// EOF
