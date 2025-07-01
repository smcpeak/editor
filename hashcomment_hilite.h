// hashcomment_hilite.h
// Hash-comment file highlighter

#ifndef HASHCOMMENT_HILITE_H
#define HASHCOMMENT_HILITE_H

// editor
#include "inclexer.h"                  // IncLexer
#include "lex_hilite.h"                // LexHighlighter

// smbase
#include "smbase/sm-override.h"        // OVERRIDE

// lexer context class defined in hashcomment_hilite.yy.cc
class HashComment_FlexLexer;


// Incremental lexer for files using hash ('#') as the comment character.
class HashComment_Lexer : public IncLexer {
private:     // data
  HashComment_FlexLexer *lexer;       // (owner)

public:      // funcs
  HashComment_Lexer();
  ~HashComment_Lexer();

  // IncLexer funcs
  virtual void beginScan(TextDocumentCore const *buffer, int line, LexerState state) OVERRIDE;
  virtual int getNextToken(TextCategory &code) OVERRIDE;
  virtual LexerState getState() const OVERRIDE;
};


// Highlighter for files using '#' as comment character.
class HashComment_Highlighter : public LexHighlighter {
private:     // data
  HashComment_Lexer theLexer;

public:      // funcs
  HashComment_Highlighter(TextDocumentCore const &buf)
    : LexHighlighter(buf, theLexer) {}

  // Highlighter funcs
  virtual string highlighterName() const OVERRIDE;
};


#endif // HASHCOMMENT_HILITE_H
