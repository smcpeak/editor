// c_hilite.cc
// test code for c_hilite.h

#include "c_hilite.h"      // module to test
#include "comment.h"       // another module to test
#include "test.h"          // USUAL_MAIN
#include "trace.h"         // traceAddSys

LexHighlighter * /*owner*/ makeC_Highlighter(TextDocumentCore const &buf)
{
  return new C_Highlighter(buf);
}

LexHighlighter * /*owner*/ makeCommentHighlighter(TextDocumentCore const &buf)
{
  return new CommentHighlighter(buf);
}


void entry()
{
  traceAddSys("highlight");

  exerciseHighlighter(&makeC_Highlighter);
  exerciseHighlighter(&makeCommentHighlighter);

  TextDocumentAndEditor tde;
  C_Highlighter hi(tde.getDocument()->getCore());
  testHighlighter(hi, tde, "test/highlight/c1.c");
  testHighlighter(hi, tde, "test/highlight/c-strlit-eof1.c");
  testHighlighter(hi, tde, "test/highlight/c-strlit-eof2.c");
  testHighlighter(hi, tde, "test/highlight/c-strlit-backsl-eof1.c");
  testHighlighter(hi, tde, "test/highlight/c-strlit-backsl-eof2.c");

  cout << "c_hilite works\n";
}

USUAL_MAIN
