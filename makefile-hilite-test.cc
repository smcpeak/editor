// makefile-hilite-test.cc
// Test code for makefile_hilite.{h,lex}.

#include "makefile_hilite.h"           // module to test

#include "td-editor.h"                 // TextDocumentAndEditor

#include "smbase/sm-test.h"            // USUAL_MAIN


void entry()
{
  TextDocumentAndEditor tde;
  Makefile_Highlighter hi(tde.getDocument()->getCore());
  testHighlighter(hi, tde, "test/highlight/mk1.mk");

  cout << "makefile_hilite works\n";
}

USUAL_MAIN


// EOF
