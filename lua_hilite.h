// lua_hilite.h
// Lua highlighter

#ifndef LUA_HILITE_H
#define LUA_HILITE_H

// editor
#include "inclexer.h"                  // IncLexer
#include "lex_hilite.h"                // LexHighlighter

// smbase
#include "smbase/sm-override.h"        // OVERRIDE

// lexer context class defined in lua_hilite.yy.cc
class Lua_FlexLexer;


// Incremental lexer for Lua.
class Lua_Lexer : public IncLexer {
private:     // data
  Lua_FlexLexer *lexer;       // (owner)

public:      // funcs
  Lua_Lexer();
  ~Lua_Lexer();

  // IncLexer funcs
  virtual void beginScan(TextDocumentCore const *buffer, LineIndex line, LexerState state) OVERRIDE;
  virtual int getNextToken(TextCategoryAOA &code) OVERRIDE;
  virtual LexerState getState() const OVERRIDE;
};


// Highlighter for Lua
class Lua_Highlighter : public LexHighlighter {
private:     // data
  Lua_Lexer theLexer;

public:      // funcs
  Lua_Highlighter(TextDocumentCore const &buf)
    : LexHighlighter(buf, theLexer) {}

  // Highlighter funcs
  virtual string highlighterName() const OVERRIDE;
};


#endif // LUA_HILITE_H
