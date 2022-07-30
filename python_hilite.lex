/* python_hilite.lex
 * lexer for highlighting Python.
 * https://docs.python.org/3/reference/lexical_analysis.html */

%smflex 101

/* ----------------------- C definitions ---------------------- */
%{

#include "python_hilite.h"             // Python_Lexer class
#include "bufferlinesource.h"          // BufferLineSource
#include "textcategory.h"              // TC_XXX constants

// lexer context class
class Python_FlexLexer : public python_hilite_yyFlexLexer {
public:      // data
  BufferLineSource bufsrc;

protected:   // funcs
  virtual int yym_read_input(void *dest, int size) override;

public:      // funcs
  Python_FlexLexer()
    : python_hilite_yyFlexLexer(),
      bufsrc()
  {}

  ~Python_FlexLexer() {}

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
%option yyclass="Python_FlexLexer"

/* output file name */
%option outfile="python_hilite.yy.cc"


/* start conditions */
/* these conditions are used to encode lexer state between lines,
   because the highlighter only sees one line at a time */
%x SINGLE_STRING
%x DOUBLE_STRING
%x TRIPLE_SINGLE_STRING
%x TRIPLE_DOUBLE_STRING


/* ------------------- definitions -------------------- */
/* Particular characters. */
NL                           "\n"
SPACE                        [ ]
TAB                          "\t"
BACKSL                       "\\"
QUOTE                        [\"]
TICK                         [\']
ANY                          .|{NL}

/* Uppercase or lowercase English letter. */
LETTER                       [A-Za-z]

/* Elements of string literals. */
STRINGPREFIX                 [ruRUfF]|[fF][rR]|[rR][fF]
ANY_BUT_BS_NL_TICK           [^\\\n\']
ANY_BUT_BS_NL_QUOTE          [^\\\n\"]
STRINGESCAPESEQ              {BACKSL}{ANY}
BYTESPREFIX                  [bB]|[bB][rR]|[rR][bB]

/* I do not distinguish between string and bytes literals. */
SB_PREFIX                    {STRINGPREFIX}|{BYTESPREFIX}

/* Elements of integer literals. */
NONZERODIGIT                 [1-9]
DIGIT                        [0-9]
BINDIGIT                     [0-1]
OCTDIGIT                     [0-7]
HEXDIGIT                     [0-9a-fA-F]
DECINTEGER                   ({NONZERODIGIT}("_"?{DIGIT})*)|("0"+("_"?"0")*)
BININTEGER                   "0"[bB]("_"?{BINDIGIT})+
OCTINTEGER                   "0"[oO]("_"?{OCTDIGIT})+
HEXINTEGER                   "0"[xX]("_"?{HEXDIGIT})+
INTEGER                      {DECINTEGER}|{BININTEGER}|{OCTINTEGER}|{HEXINTEGER}

/* Elements of floating point literals. */
DIGITPART                    {DIGIT}("_"?{DIGIT})*
FRACTION                     "."{DIGITPART}
EXPONENT                     [eE][+-]?{DIGITPART}
POINTFLOAT                   {DIGITPART}?({FRACTION}|{DIGITPART}".")
EXPONENTFLOAT                ({DIGITPART}|{POINTFLOAT}){EXPONENT}
FLOATNUMBER                  {POINTFLOAT}|{EXPONENTFLOAT}


/* ------------- token definition rules --------------- */
%%

  /* Comment.  Backslash does *not* continue it. */
"#".* {
  return TC_COMMENT;
}

  /* A backslash at end of line continues the line, but that is not
   * relevant for highlighting.  I'll highlight the backslash itself
   * like a keyword. */
{BACKSL}{NL} {
  return TC_KEYWORD;
}

  /* Identifier-like keywords except for "True", "False", and "None". */
"and"              |
"as"               |
"assert"           |
"async"            |
"await"            |
"break"            |
"class"            |
"continue"         |
"def"              |
"del"              |
"elif"             |
"else"             |
"except"           |
"finally"          |
"for"              |
"from"             |
"global"           |
"if"               |
"import"           |
"in"               |
"is"               |
"lambda"           |
"nonlocal"         |
"not"              |
"or"               |
"pass"             |
"raise"            |
"return"           |
"try"              |
"while"            |
"with"             |
"yield"            {
  return TC_KEYWORD;
}

  /* Special values. */
"False"            |
"None"             |
"True"             {
  return TC_SPECIAL;
}

  /* Treat soft keywords like regular keywords */
"match"            |
"case"             |
"_"                {
  return TC_KEYWORD;
}

  /* Identifier. */
({LETTER}|"_")({LETTER}|[0-9_])* {
  return TC_NORMAL;
}

  /* Triple-single-quoted string literal. */
{SB_PREFIX}?{TICK}{TICK}{TICK} {
  YY_SET_START_CONDITION(TRIPLE_SINGLE_STRING);
  return TC_STRING;
}

<TRIPLE_SINGLE_STRING>{
  {TICK}{TICK}{TICK} {
    YY_SET_START_CONDITION(INITIAL);
    return TC_STRING;
  }

  ({ANY_BUT_BS_NL_TICK}|{STRINGESCAPESEQ})+ {
    return TC_STRING;
  }

  /* Unterminated string. */
  {ANY} {
    return TC_STRING;
  }
}

  /* Triple-double-quoted string literal. */
{SB_PREFIX}?{QUOTE}{QUOTE}{QUOTE} {
  YY_SET_START_CONDITION(TRIPLE_DOUBLE_STRING);
  return TC_STRING;
}

<TRIPLE_DOUBLE_STRING>{
  {QUOTE}{QUOTE}{QUOTE} {
    YY_SET_START_CONDITION(INITIAL);
    return TC_STRING;
  }

  ({ANY_BUT_BS_NL_QUOTE}|{STRINGESCAPESEQ})+ {
    return TC_STRING;
  }

  /* Unterminated string. */
  {ANY} {
    return TC_STRING;
  }
}

  /* Single-quoted string literal. */
  /* TODO: It would be cool to highlight replacement expressions inside
   * f-strings. */
{SB_PREFIX}?{TICK} {
  YY_SET_START_CONDITION(SINGLE_STRING);
  return TC_STRING;
}

<SINGLE_STRING>{
  {TICK} {
    YY_SET_START_CONDITION(INITIAL);
    return TC_STRING;
  }

  ({ANY_BUT_BS_NL_TICK}|{STRINGESCAPESEQ})+ {
    return TC_STRING;
  }

  /* Unterminated string. */
  {ANY} {
    YY_SET_START_CONDITION(INITIAL);
    return TC_STRING;
  }
}

  /* Double-quoted string literal. */
{SB_PREFIX}?{QUOTE} {
  YY_SET_START_CONDITION(DOUBLE_STRING);
  return TC_STRING;
}

<DOUBLE_STRING>{
  {QUOTE} {
    YY_SET_START_CONDITION(INITIAL);
    return TC_STRING;
  }

  ({ANY_BUT_BS_NL_QUOTE}|{STRINGESCAPESEQ})+ {
    return TC_STRING;
  }

  /* Unterminated string. */
  {ANY} {
    YY_SET_START_CONDITION(INITIAL);
    return TC_STRING;
  }
}

  /* Integer literal. */
{INTEGER} {
  return TC_NUMBER;
}

  /* Floating point literal. */
{FLOATNUMBER} {
  return TC_NUMBER;
}

  /* Imaginaary literal. */
({FLOATNUMBER}|{DIGITPART})[jJ] {
  return TC_NUMBER;
}

  /* Operators. */
"+"                |
"-"                |
"*"                |
"**"               |
"/"                |
"//"               |
"%"                |
"@"                |
"<<"               |
">>"               |
"&"                |
"|"                |
"^"                |
"~"                |
":="               |
"<"                |
">"                |
"<="               |
">="               |
"=="               |
"!="               {
  return TC_OPERATOR;
}

  /* Delimiters. */
  /* Oddly, the spec has "@" here too.  I omit it. */
"("                |
")"                |
"["                |
"]"                |
"{"                |
"}"                |
","                |
":"                |
"."                |
";"                |
"="                |
"->"               |
"+="               |
"-="               |
"*="               |
"/="               |
"//="              |
"%="               |
"@="               |
"&="               |
"|="               |
"^="               |
">>="              |
"<<="              |
"**="              {
  return TC_OPERATOR;
}

  /* whitespace */
[ \t\n\f\v\r]+ {
  return TC_NORMAL;
}

  /* anything else */
. {
  return TC_ERROR;
}


%%
// third section: extra C++ code


// ----------------------- Python_FlexLexer ---------------------
int Python_FlexLexer::yym_read_input(void *dest, int size)
{
  return bufsrc.fillBuffer(dest, size);
}


void Python_FlexLexer::setState(LexerState state)
{
  yym_set_start_condition((int)state);
}

LexerState Python_FlexLexer::getState() const
{
  int sc = yym_get_start_condition();
  return static_cast<LexerState>(sc);
}


// ------------------------- Python_Lexer -----------------------
Python_Lexer::Python_Lexer()
  : lexer(new Python_FlexLexer)
{}

Python_Lexer::~Python_Lexer()
{
  delete lexer;
}


void Python_Lexer::beginScan(TextDocumentCore const *buffer, int line, LexerState state)
{
  lexer->bufsrc.beginScan(buffer, line);
  lexer->setState(state);
}


int Python_Lexer::getNextToken(TextCategory &code)
{
  int result = lexer->yym_lex();

  if (result == 0) {
    // end of line
    switch ((int)lexer->getState()) {
      case TRIPLE_SINGLE_STRING:
      case TRIPLE_DOUBLE_STRING:
        code = TC_STRING;
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


LexerState Python_Lexer::getState() const
{
  return lexer->getState();
}


// ---------------------- Python_Highlighter --------------------
string Python_Highlighter::highlighterName() const
{
  return "Python";
}


// EOF
