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
#include "style.h"       // LineStyle
#include "trace.h"       // traceAddSys

Buffer buf;

void printLine(C_Highlighter &hi, int line)
{
  LineStyle style(ST_NORMAL);
  hi.highlight(buf, line, style);

  cout << "line " << line << ":\n"
       << "  text : " << buf.getWholeLine(line) << "\n"
       << "  style: " << style.asUnaryString() << endl;
}

void printStyles(C_Highlighter &hi)
{
  // stop short so I have a waterline
  for (int i=0; i<buf.numLines()-1; i++) {
    printLine(hi, i);
  }
}

void insert(int line, int col, char const *text)
{
  buf.insertTextRange(line, col, text);
}

void del(int line, int col, int len)
{
  buf.deleteText(line, col, len);
}

// check that the incremental highlighter matches a batch highlighter
void check(C_Highlighter &hi)
{
  C_Highlighter batch(buf);    // batch because it has no initial info

  for (int i=0; i<buf.numLines(); i++) {
    LineStyle style1(ST_NORMAL);
    hi.highlight(buf, i, style1);
    string rendered1 = style1.asUnaryString();

    LineStyle style2(ST_NORMAL);
    hi.highlight(buf, i, style2);
    string rendered2 = style1.asUnaryString();

    // compare using rendered strings, instead of looking at
    // the run-length ranges, since it's ok if the incrementality
    // somehow gives rise to slightly different ranges (say, in one
    // version there are two adjacent ranges of same-style chars)

    if (rendered1 != rendered2) {
      cout << "check: mismatch at line " << i << ":\n"
           << "  line: " << buf.getWholeLine(i) << "\n"
           << "  inc.: " << rendered1 << "\n"
           << "  bat.: " << rendered2 << "\n"
           ;
    }
  }
}


void entry()
{
  traceAddSys("highlight");

  C_Highlighter hi(buf);

  int line=0, col=0;
  buf.insertTextRange(line, col,
    "hi there\n"
    "here is \"a string\" ok?\n"
    "and how about /*a comment*/ yo\n"
    "C++ comment: // I like C++\n"
    "back to normalcy\n"
  );
  printStyles(hi);
  check(hi);

  insert(2, 3, " what");
  printLine(hi, line);
  check(hi);

  insert(0, 3, "um, ");
  insert(2, 0, "derf ");
  insert(4, 5, "there ");
  printLine(hi, 1);
  printLine(hi, 2);
  printLine(hi, 4);
  check(hi);

  insert(0, 7, "/""*");
  printLine(hi, 4);
  printStyles(hi);
  check(hi);
  printStyles(hi);

  insert(0, 2, "\"");
  del(2, 35, 2);
  insert(4, 2, "Arg");
  printLine(hi, 4);
  check(hi);
  printStyles(hi);

  insert(0, 15, "\\");
  check(hi);
  printStyles(hi);

}

USUAL_MAIN

#endif //  TEST_C_HILITE
