// test-makefile-hilite.cc
// Test code for makefile_hilite.{h,lex}.

#include "makefile_hilite.h"           // module to test
#include "td-editor.h"                 // TextDocumentAndEditor
#include "test.h"                      // USUAL_MAIN


void entry()
{
  TextDocumentAndEditor tde;
  tde.insertNulTermText(
    "# comment\n"
    "var := value  # comment\n"
    "-include somefile\n"
    "# comment that \\\n"
    "  spans two lines\n"
    "target: prereq\n"
    "\tshellcmd1\n"
    "# makefile comment\n"
    "\t# shell comment\n"
    "\n"
  );

  Makefile_Highlighter hi(tde.getDocument()->getCore());
  printHighlightedLines(tde.getDocument()->getCore(), hi);

  cout << "makefile_hilite works\n";
}

USUAL_MAIN


// EOF
