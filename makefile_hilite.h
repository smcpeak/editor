// makefile_hilite.h
// Makefile highlighter

#ifndef MAKEFILE_HILITE_H
#define MAKEFILE_HILITE_H

// editor
#include "inclexer.h"                  // IncLexer
#include "lex_hilite.h"                // LexHighlighter

// smbase
#include "smbase/sm-override.h"        // OVERRIDE

// lexer context class defined in make_hilite.yy.cc
class Makefile_FlexLexer;


// Incremental lexer for Makefiles.
class Makefile_Lexer : public IncLexer {
private:     // data
  Makefile_FlexLexer *lexer;       // (owner)

public:      // funcs
  Makefile_Lexer();
  ~Makefile_Lexer();

  // IncLexer funcs
  virtual void beginScan(TextDocumentCore const *buffer, LineIndex line, LexerState state) OVERRIDE;
  virtual int getNextToken(TextCategory &code) OVERRIDE;
  virtual LexerState getState() const OVERRIDE;
};


// Highlighter for Makefiles.
class Makefile_Highlighter : public LexHighlighter {
private:     // data
  Makefile_Lexer theLexer;

public:      // funcs
  Makefile_Highlighter(TextDocumentCore const &buf)
    : LexHighlighter(buf, theLexer) {}

  // Highlighter funcs
  virtual string highlighterName() const OVERRIDE;
};


#endif // MAKEFILE_HILITE_H
