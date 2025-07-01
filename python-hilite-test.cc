// python-hilite-test.cc
// Test code for python_hilite.{h,lex}.

#include "python_hilite.h"             // module to test

#include "td-editor.h"                 // TextDocumentAndEditor

#include "smbase/sm-test.h"            // USUAL_MAIN


void entry()
{
  TextDocumentAndEditor tde;
  Python_Highlighter hi(tde.getDocument()->getCore());
  testHighlighter(hi, tde, "test/highlight/python1.py");

  cout << "python_hilite works\n";
}

USUAL_MAIN


// EOF
