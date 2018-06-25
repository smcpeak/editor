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

  cout << "c_hilite works\n";
}

USUAL_MAIN
