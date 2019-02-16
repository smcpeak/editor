// test-makefile-hilite.cc
// Test code for makefile_hilite.{h,lex}.

#include "makefile_hilite.h"           // module to test
#include "td-editor.h"                 // TextDocumentAndEditor
#include "test.h"                      // USUAL_MAIN


static void printLine(TextDocumentAndEditor &tde,
                      LexHighlighter &hi, int line)
{
  LineCategories categories(TC_NORMAL);
  hi.highlightTDE(&tde, line, categories);

  cout << "line " << line << ":\n"
       << "  text : " << tde.getWholeLineString(line) << "\n"
       << "  catgy: " << categories.asUnaryString() << "\n"
       << "  rle  : " << categories.asString() << "\n"
       ;
}

static void printLines(TextDocumentAndEditor &tde, LexHighlighter &hi)
{
  for (int i=0; i < tde.numLines(); i++) {
    printLine(tde, hi, i);
  }
}


void entry()
{
  TextDocumentAndEditor tde;
  tde.insertNulTermText(
    "# comment\n"
    "var := value  # comment\n"
    "-include somefile\n"
    "target: prereq\n"
    "\tshellcmd1\n"
    "# makefile comment\n"
    "\t# shell comment\n"
    "\n"
  );

  Makefile_Highlighter hi(tde.getDocument()->getCore());
  printLines(tde, hi);

  cout << "makefile_hilite works\n";
}

USUAL_MAIN


// EOF
