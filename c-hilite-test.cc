// c-hilite-test.cc
// Tests for `c_hilite` and `comment` modules.

#include "unit-tests.h"                // decl for my entry point

#include "c_hilite.h"                  // module to test
#include "comment.h"                   // another module to test


LexHighlighter * /*owner*/ makeC_Highlighter(TextDocumentCore const &buf)
{
  return new C_Highlighter(buf);
}

LexHighlighter * /*owner*/ makeCommentHighlighter(TextDocumentCore const &buf)
{
  return new CommentHighlighter(buf);
}


// Called from unit-tests.cc.
void test_c_hilite(CmdlineArgsSpan args)
{
  exerciseHighlighter(&makeC_Highlighter);
  exerciseHighlighter(&makeCommentHighlighter);

  TextDocumentAndEditor tde;
  C_Highlighter hi(tde.getDocument()->getCore());
  testHighlighter(hi, tde, "test/highlight/c1.c");
  testHighlighter(hi, tde, "test/highlight/c-strlit-eof1.c");
  testHighlighter(hi, tde, "test/highlight/c-strlit-eof2.c");
  testHighlighter(hi, tde, "test/highlight/c-strlit-backsl-eof1.c");
  testHighlighter(hi, tde, "test/highlight/c-strlit-backsl-eof2.c");
  testHighlighter(hi, tde, "test/highlight/c-c-comment-eof1.c");
  testHighlighter(hi, tde, "test/highlight/c-c-comment-eof2.c");
  testHighlighter(hi, tde, "test/highlight/c-c-comment-eof3.c");
  testHighlighter(hi, tde, "test/highlight/c-c-comment-eof4.c");
  testHighlighter(hi, tde, "test/highlight/c-cpp-comment-eof1.c");
  testHighlighter(hi, tde, "test/highlight/c-cpp-comment-eof2.c");
  testHighlighter(hi, tde, "test/highlight/c-cpp-comment-eof3.c");
  testHighlighter(hi, tde, "test/highlight/c-cpp-comment-eof4.c");
  testHighlighter(hi, tde, "test/highlight/c-fesvr-syscall.cc");
  testHighlighter(hi, tde, "test/highlight/odd-stars.c");
}


// EOF
