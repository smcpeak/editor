// python_hilite.h
// Python highlighter

#ifndef PYTHON_HILITE_H
#define PYTHON_HILITE_H

// editor
#include "inclexer.h"                  // IncLexer
#include "lex_hilite.h"                // LexHighlighter

// smbase
#include "smbase/sm-override.h"        // OVERRIDE

// lexer context class defined in python_hilite.yy.cc
class Python_FlexLexer;


// Incremental lexer for Python.
class Python_Lexer : public IncLexer {
private:     // data
  Python_FlexLexer *lexer;       // (owner)

public:      // funcs
  Python_Lexer();
  ~Python_Lexer();

  // IncLexer funcs
  virtual void beginScan(TextDocumentCore const *buffer, LineIndex line, LexerState state) OVERRIDE;
  virtual int getNextToken(TextCategory &code) OVERRIDE;
  virtual LexerState getState() const OVERRIDE;
};


// Highlighter for Python
class Python_Highlighter : public LexHighlighter {
private:     // data
  Python_Lexer theLexer;

public:      // funcs
  Python_Highlighter(TextDocumentCore const &buf)
    : LexHighlighter(buf, theLexer) {}

  // Highlighter funcs
  virtual string highlighterName() const OVERRIDE;
};


#endif // PYTHON_HILITE_H
