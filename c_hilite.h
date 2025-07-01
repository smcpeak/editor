// c_hilite.h
// C/C++ highlighter

#ifndef C_HILITE_H
#define C_HILITE_H

// editor
#include "inclexer.h"                  // IncLexer
#include "lex_hilite.h"                // LexHighlighter

// smbase
#include "smbase/sm-override.h"        // OVERRIDE

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
  virtual void beginScan(TextDocumentCore const *buffer, int line, LexerState state) OVERRIDE;
  virtual int getNextToken(TextCategory &code) OVERRIDE;
  virtual LexerState getState() const OVERRIDE;
};


// highlighter for C++
class C_Highlighter : public LexHighlighter {
private:     // data
  C_Lexer theLexer;

public:      // funcs
  C_Highlighter(TextDocumentCore const &buf)
    : LexHighlighter(buf, theLexer) {}

  // Highlighter funcs
  virtual string highlighterName() const OVERRIDE;
};


#endif // C_HILITE_H
