// c_hilite.h
// C/C++ highlighter

#ifndef C_HILITE_H
#define C_HILITE_H

#include "inclexer.h"      // IncLexer
#include "lex_hilite.h"    // LexHighlighter

// lexer context class defined in c_hilite.yy.cc
class C_FlexLexer;


// incremental lexer for C++
class C_Lexer : public IncLexer {
private:     // data
  C_FlexLexer *lexer;       // (owner)

public:      // funcs
  C_Lexer();
  ~C_Lexer();

  // IncLexer funcs
  virtual void beginScan(TextDocumentCore const *buffer, int line, LexerState state) override;
  virtual int getNextToken(TextCategory &code) override;
  virtual LexerState getState() const override;
};


// highlighter for C++
class C_Highlighter : public LexHighlighter {
private:     // data
  C_Lexer theLexer;

public:      // funcs
  C_Highlighter(TextDocumentCore const &buf)
    : LexHighlighter(buf, theLexer) {}
};


#endif // C_HILITE_H
