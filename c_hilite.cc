// c_hilite.cc
// code for c_hilite.h

#include "c_hilite.h"      // this module
#include "style.h"         // LineStyle
#include "buffer.h"        // BufferCore, Buffer


C_Highlighter::C_Highlighter()
{}

C_Highlighter::~C_Highlighter()
{}


void C_Highlighter::highlight(BufferCore const &buf, int line, LineStyle &style)
{
  // for now, re-scan entire buffer
  int prevState = 0;
  for (int i=0; i<line; i++) {
    lexer.beginScan(&buf, i, prevState);
    
    int len, code;
    while (lexer.getNextToken(len, code))
      {}
      
    prevState = lexer.getState();
  }
  
  // scan the line in question
  lexer.beginScan(&buf, line, prevState);
  
  // append each styled segment
  int len, code;
  while (lexer.getNextToken(len, code)) {
    style.append((Style)code, len);
  }

  // don't bother saving state for now since there's no place to put it
}


// ---------------------- test code -------------------------
#ifdef TEST_C_HILITE

#include "test.h"        // USUAL_MAIN

void entry()
{
  Buffer buf;
  int line=0, col=0;
  buf.insertTextRange(line, col,
    "hi there\n"
    "here is \"a string\" ok?\n"
    "and how about /*a comment*/ yo\n"
    "C++ comment: // I like C++\n"
    "back to normalcy\n"
  );
  
  C_Highlighter hi;
  for (int i=0; i<line; i++) {
    LineStyle style(ST_NORMAL);
    hi.highlight(buf, i, style);
    
    cout << "line " << i << ":\n"
         << "  text : " << buf.getTextRange(i, 0, i, buf.lineLength(i)) << "\n"
         << "  style: " << style.asString() << endl;
  }
}

USUAL_MAIN

#endif //  TEST_C_HILITE
