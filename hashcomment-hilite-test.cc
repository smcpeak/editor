// hashcomment-hilite-test.cc
// Test code for hashcomment_hilite.{h,lex}.

#include "hashcomment_hilite.h"        // module to test

#include "td-editor.h"                 // TextDocumentAndEditor

#include "smbase/sm-test.h"            // USUAL_MAIN


void entry()
{
  TextDocumentAndEditor tde;
  HashComment_Highlighter hi(tde.getDocument()->getCore());
  testHighlighter(hi, tde, "test/highlight/hashcomment1.sh");

  cout << "hashcomment_hilite works\n";
}

USUAL_MAIN


// EOF
