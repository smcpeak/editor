// python-hilite-test.cc
// Test code for python_hilite.{h,lex}.

#include "unit-tests.h"                // decl for my entry point

#include "python_hilite.h"             // module to test

#include "td-editor.h"                 // TextDocumentAndEditor


// Called from unit-tests.cc.
void test_python_hilite(CmdlineArgsSpan args)
{
  TextDocumentAndEditor tde;
  Python_Highlighter hi(tde.getDocument()->getCore());
  testHighlighter(hi, tde, "test/highlight/python1.py");
}


// EOF
