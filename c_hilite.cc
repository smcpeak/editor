// c_hilite.cc
// code for c_hilite.h

#include "c_hilite.h"      // this module


C_Highlighter::C_Highlighter(BufferCore const &buf)
  : LexHighlighter(buf, theLexer),
    theLexer()
{}

C_Highlighter::~C_Highlighter()
{}


// ---------------------- test code -------------------------
#ifdef TEST_C_HILITE

#include "test.h"        // USUAL_MAIN
#include "trace.h"       // traceAddSys

LexHighlighter * /*owner*/ makeC_Highlighter(BufferCore const &buf)
{
  return new C_Highlighter(buf);
}
     

void entry()
{
  traceAddSys("highlight");

  exerciseHighlighter(&makeC_Highlighter);

  cout << "c_hilite works\n";
}

USUAL_MAIN

#endif //  TEST_C_HILITE
