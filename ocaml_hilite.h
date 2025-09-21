// ocaml_hilite.h
// OCaml highlighter

#ifndef OCAML_HILITE_H
#define OCAML_HILITE_H

// editor
#include "inclexer.h"                  // IncLexer
#include "lex_hilite.h"                // LexHighlighter

// smbase
#include "smbase/sm-override.h"        // OVERRIDE

// lexer context class defined in ocaml_hilite.yy.cc
class OCaml_FlexLexer;


// Incremental lexer for OCaml.
class OCaml_Lexer : public IncLexer {
private:     // data
  OCaml_FlexLexer *lexer;       // (owner)

public:      // funcs
  OCaml_Lexer();
  ~OCaml_Lexer();

  // IncLexer funcs
  virtual void beginScan(TextDocumentCore const *buffer, LineIndex line, LexerState state) OVERRIDE;
  virtual int getNextToken(TextCategoryAOA &code) OVERRIDE;
  virtual LexerState getState() const OVERRIDE;
};


// Highlighter for OCaml
class OCaml_Highlighter : public LexHighlighter {
private:     // data
  OCaml_Lexer theLexer;

public:      // funcs
  OCaml_Highlighter(TextDocumentCore const &buf)
    : LexHighlighter(buf, theLexer) {}

  // Highlighter funcs
  virtual string highlighterName() const OVERRIDE;
};


#endif // OCAML_HILITE_H
