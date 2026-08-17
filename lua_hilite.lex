/* lua_hilite.lex
 * lexer for highlighting Lua.
 * Spec: https://www.lua.org/manual/5.4/manual.html#3.1 */

%smflex 101

/* ----------------------- C definitions ---------------------- */
%{
#include "lua_hilite.h"                // Lua_Lexer class
#include "bufferlinesource.h"          // BufferLineSource
#include "textcategory.h"              // TC_XXX constants

// lexer context class
class Lua_FlexLexer : public lua_hilite_yyFlexLexer {
public:      // data
  BufferLineSource bufsrc;

protected:   // funcs
  virtual int yym_read_input(void *dest, int size) override;

public:      // funcs
  Lua_FlexLexer()
    : lua_hilite_yyFlexLexer(),
      bufsrc()
  {}

  ~Lua_FlexLexer() {}

  int yym_lex();

  void setState(LexerState state);
  LexerState getState() const;
};

%}


/* -------------------- flex options ------------------ */
/* don't use the default-echo rules */
%option nodefault

/* generate a c++ lexer */
%option c++

/* use the "fast" algorithm with no table compression */
%option full

/* utilize character equivalence classes */
%option ecs

/* and I will define the class */
%option yyclass="Lua_FlexLexer"

/* output file name */
%option outfile="lua_hilite.yy.cc"


/* Start conditions.  These conditions are used to encode lexer state
 * between lines, because the highlighter only sees one line at a time. */
%x SINGLE_STRING
%x DOUBLE_STRING
%x LONG_STRING
%x LONG_COMMENT
%x SHORT_COMMENT


/* ------------------- definitions -------------------- */
NL              "\n"
BACKSL          "\\"
QUOTE           [\"]
TICK            [\']
ANY             .|{NL}

DIGIT           [0-9]
HEXDIGIT        [0-9a-fA-F]
LETTER          [A-Za-z_]

IDENT           {LETTER}({LETTER}|{DIGIT})*

DECIMAL         {DIGIT}+(\.{DIGIT}*)?([eE][+-]?{DIGIT}+)?
HEXNUM          0[xX]{HEXDIGIT}+(\.{HEXDIGIT}*)?([pP][+-]?{DIGIT}+)?
NUMBER          {DECIMAL}|{HEXNUM}

/* Long bracket delimiters.  NOTE: This is only approximate, as it
 * treats all long brackets equivalently, ignoring their level. */
OPENING_LONG_BRACKET     \[=*\[
CLOSING_LONG_BRACKET     \]=*\]


/* ------------- token definition rules --------------- */
%%


  /* ---------- Comments ---------- */

  /* Long comment start. */
"--"{OPENING_LONG_BRACKET} {
  YY_SET_START_CONDITION(LONG_COMMENT);
  return TC_COMMENT;
}

<LONG_COMMENT>{
  {CLOSING_LONG_BRACKET} {
    YY_SET_START_CONDITION(INITIAL);
    return TC_COMMENT;
  }

  {ANY} {
    return TC_COMMENT;
  }
}

  /* Single-line comment.  This is not all one rule because then the
   * "longest match" mechanism of Flex would cause it to take precedence
   * over the long comment rule above. */
"--" {
  YY_SET_START_CONDITION(SHORT_COMMENT);
  return TC_COMMENT;
}

<SHORT_COMMENT>{
  {NL} {
    YY_SET_START_CONDITION(INITIAL);
    return TC_COMMENT;
  }

  {ANY} {
    return TC_COMMENT;
  }
}


  /* ---------- Keywords ---------- */

"and"|"break"|"do"|"else"|"elseif"|"end" |
"for"|"function"|"goto"|"if"             |
"in"|"local"|"not"|"or"                  |
"repeat"|"return"|"then"                 |
"until"|"while"                          {
  return TC_KEYWORD;
}

  /* Special literals */
"nil"|"true"|"false" {
  return TC_SPECIAL;
}

  /* Identifier */
{IDENT} {
  return TC_NORMAL;
}


  /* ---------- Strings ---------- */

  /* Long string */
{OPENING_LONG_BRACKET} {
  YY_SET_START_CONDITION(LONG_STRING);
  return TC_STRING;
}

<LONG_STRING>{
  {CLOSING_LONG_BRACKET} {
    YY_SET_START_CONDITION(INITIAL);
    return TC_STRING;
  }

  {ANY} {
    return TC_STRING;
  }
}

  /* Single-quoted string */
{TICK} {
  YY_SET_START_CONDITION(SINGLE_STRING);
  return TC_STRING;
}

<SINGLE_STRING>{
  {TICK} {
    YY_SET_START_CONDITION(INITIAL);
    return TC_STRING;
  }

  {BACKSL}{ANY} {
    return TC_STRING;
  }

  /* Non-special characters. */
  [^\\\n\']+ {
    return TC_STRING;
  }

  /* This will trigger for a backlash at EOF, a newline, or a single
   * tick character.  The first two are syntax errors that, at least for
   * now, the highlighter does not diagnose. */
  {ANY} {
    YY_SET_START_CONDITION(INITIAL);
    return TC_STRING;
  }
}

  /* Double-quoted string */
{QUOTE} {
  YY_SET_START_CONDITION(DOUBLE_STRING);
  return TC_STRING;
}

<DOUBLE_STRING>{
  {QUOTE} {
    YY_SET_START_CONDITION(INITIAL);
    return TC_STRING;
  }

  {BACKSL}{ANY} |
  [^\\\n\"]+ {
    return TC_STRING;
  }

  {ANY} {
    YY_SET_START_CONDITION(INITIAL);
    return TC_STRING;
  }
}


  /* ---------- Numbers ---------- */
{NUMBER} {
  return TC_NUMBER;
}


  /* ---------- Operators ---------- */
"+"|"-"|"*"|"/"|"%"|"^"|"#"   |
"&"|"~"|"|"|"<<"|">>"|"//"    |
"=="|"~="|"<="|">="|"<"|">"   |
"="|"("|")"|"{"|"}"           |
"["|"]"|"::"|";"|":"|","      |
"."|".."|"..."                {
  return TC_OPERATOR;
}


  /* ---------- Whitespace ---------- */
[ \t\n\f\v\r]+ {
  return TC_NORMAL;
}


  /* ---------- Errors ---------- */
. {
  return TC_ERROR;
}


%%


// --------------------------- Lua_FlexLexer ---------------------------
int Lua_FlexLexer::yym_read_input(void *dest, int size)
{
  return bufsrc.fillBuffer(dest, size);
}


void Lua_FlexLexer::setState(LexerState state)
{
  yym_set_start_condition((int)state);
}


LexerState Lua_FlexLexer::getState() const
{
  return static_cast<LexerState>(yym_get_start_condition());
}


// ----------------------------- Lua_Lexer -----------------------------
Lua_Lexer::Lua_Lexer()
  : lexer(new Lua_FlexLexer)
{}

Lua_Lexer::~Lua_Lexer()
{
  delete lexer;
}


void Lua_Lexer::beginScan(TextDocumentCore const *buffer, LineIndex line, LexerState state)
{
  lexer->bufsrc.beginScan(buffer, line);
  lexer->setState(state);
}


int Lua_Lexer::getNextToken(TextCategoryAOA &code)
{
  int result = lexer->yym_lex();

  if (result == 0) {
    // end of line
    switch ((int)lexer->getState()) {
      case LONG_STRING:
        code = TC_STRING;
        break;

      case LONG_COMMENT:
        code = TC_COMMENT;
        break;

      default:
        code = TC_NORMAL;
        break;
    }
    return 0;
  }
  else {
    code = (TextCategory)result;
    return lexer->yym_leng();
  }
}


LexerState Lua_Lexer::getState() const
{
  return lexer->getState();
}


// -------------------------- Lua_Highlighter --------------------------
string Lua_Highlighter::highlighterName() const
{
  return "Lua";
}


// EOF
