/* ocaml_hilite.lex
   lexer for highlighting OCaml. */

%smflex 101

/* ----------------------- C definitions ---------------------- */
%{

#include "ocaml_hilite.h"              // OCaml_Lexer class
#include "bufferlinesource.h"          // BufferLineSource
#include "textcategory.h"              // TC_XXX constants

// lexer context class
class OCaml_FlexLexer : public ocaml_hilite_yyFlexLexer {
public:      // data
  BufferLineSource bufsrc;

  // Comment nesting depth.  0 means we are not in a comment.
  int m_commentNesting;

protected:   // funcs
  virtual int yym_read_input(void *dest, int size) override;

public:      // funcs
  OCaml_FlexLexer()
    : ocaml_hilite_yyFlexLexer(),
      bufsrc(),
      m_commentNesting(0)
  {}

  ~OCaml_FlexLexer() {}

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
%option yyclass="OCaml_FlexLexer"

/* output file name */
%option outfile="ocaml_hilite.yy.cc"


/* start conditions */
/* these conditions are used to encode lexer state between lines,
   because the highlighter only sees one line at a time */
%x STRING
%x COMMENT


/* ------------------- definitions -------------------- */
/* Particular characters. */
NL                           "\n"
SPACE                        [ ]
TAB                          "\t"
BACKSL                       "\\"
QUOTE                        [\"]
TICK                         [\']

/* Uppercase or lowercase English letter. */
LETTER                       [A-Za-z]

/* Character other than tick or backslash. */
REGULAR_CHAR                 [^\'\\]

/* Escape sequence that can appear in a character or string literal. */
MNEMONIC_ESCAPE_SEQUENCE     {BACKSL}[\\\"\'ntbr ]
DEC_ESCAPE_SEQUENCE          {BACKSL}[0-9][0-9][0-9]
HEX_ESCAPE_SEQUENCE          {BACKSL}"x"[0-9A-Fa-f][0-9A-Fa-f]
OCT_ESCAPE_SEQUENCE          {BACKSL}"o"[0-3][0-7][0-7]
ESCAPE_SEQUENCE              ({MNEMONIC_ESCAPE_SEQUENCE}|{DEC_ESCAPE_SEQUENCE}|{HEX_ESCAPE_SEQUENCE}|{OCT_ESCAPE_SEQUENCE})

/* Character other than double-quote or backslash. */
REGULAR_STRING_CHAR          [^\"\\]

/* Character inside a string literal. */
UNICODE_ESCAPE_SEQUENCE      {BACKSL}"u{"([0-9A-Fa-f]+)"}"
NEWLINE_ESCAPE_SEQUENCE      {BACKSL}{NL}({SPACE}|{TAB})*
STRING_CHARACTER             ({REGULAR_STRING_CHAR}|{ESCAPE_SEQUENCE}|{UNICODE_ESCAPE_SEQUENCE}|{NEWLINE_ESCAPE_SEQUENCE})

/* One of the listed characters, where both '-' and '^' are treated
 * literally. */
CORE_OPERATOR_CHAR           [-$&*+/=>@|^]

OPERATOR_CHAR                ([~!?%<:.]|{CORE_OPERATOR_CHAR})

/* Infix symbols begin with an operator character other than those used
 * for prefix. */
INFIX_SYMBOL1                ({CORE_OPERATOR_CHAR}|[%<]){OPERATOR_CHAR}*
INFIX_SYMBOL2                "#"{OPERATOR_CHAR}+
INFIX_SYMBOL                 ({INFIX_SYMBOL1}|{INFIX_SYMBOL2})

/* Prefix symbols begin with "!", "?", or "~". */
PREFIX_SYMBOL1               "!"{OPERATOR_CHAR}*
PREFIX_SYMBOL2               [?~]{OPERATOR_CHAR}+
PREFIX_SYMBOL                ({PREFIX_SYMBOL1}|{PREFIX_SYMBOL2})


/* ------------- token definition rules --------------- */
%%

  /* Identifier-like keywords except for "true" and "false". */
"and"              |
"as"               |
"assert"           |
"asr"              |
"begin"            |
"class"            |
"constraint"       |
"do"               |
"done"             |
"downto"           |
"else"             |
"end"              |
"exception"        |
"external"         |
"for"              |
"fun"              |
"function"         |
"functor"          |
"if"               |
"in"               |
"include"          |
"inherit"          |
"initializer"      |
"land"             |
"lazy"             |
"let"              |
"lor"              |
"lsl"              |
"lsr"              |
"lxor"             |
"match"            |
"method"           |
"mod"              |
"module"           |
"mutable"          |
"new"              |
"nonrec"           |
"object"           |
"of"               |
"open"             |
"or"               |
"private"          |
"rec"              |
"sig"              |
"struct"           |
"then"             |
"to"               |
"try"              |
"type"             |
"val"              |
"virtual"          |
"when"             |
"while"            |
"with"             {
  return TC_KEYWORD;
}

  /* Special values. */
"true"|"false" {
  return TC_SPECIAL;
}

  /* Operator-like keywords. */
"!="   |
"#"    |
"&"    |
"&&"   |
"'"    |
"("    |
")"    |
"*"    |
"+"    |
","    |
"-"    |
"-."   |
"->"   |
"."    |
".."   |
".~"   |
":"    |
"::"   |
":="   |
":>"   |
";"    |
";;"   |
"<"    |
"<-"   |
"="    |
">"    |
">]"   |
">}"   |
"?"    |
"["    |
"[<"   |
"[>"   |
"[|"   |
"]"    |
"_"    |
"`"    |
"{"    |
"{<"   |
"|"    |
"|]"   |
"||"   |
"}"    |
"~"    {
  return TC_OPERATOR;
}

  /* Symbols to be avoided. */
"parser"      |
"value"       |
"$"           |
"$$"          |
"$:"          |
"<:"          |
"<<"          |
">>"          |
"??"          {
  return TC_ERROR;
}

  /* Line number directive. */
^#[0-9]+.*    {
  return TC_PREPROCESSOR;
}

  /* Identifier. */
({LETTER}|"_")({LETTER}|[0-9_]|{TICK})* {
  return TC_NORMAL;
}

  /* Integer literal. */
"-"?[0-9][0-9_]*[lLn]?                           |
"-"?("0x"|"0X")[0-9A-Fa-f][0-9A-Fa-f_]*[lLn]?    |
"-"?("0o"|"0O")[0-7][0-7_]*[lLn]?                |
"-"?("0b"|"0B")[0-1][0-1_]*[lLn]?                {
  return TC_NUMBER;
}

  /* Floating literal. */
"-"?[0-9][0-9_]*("."[0-9_]*)?([eE][+-][0-9][0-9_]*)?                               |
"-"?("0x"|"0X")[0-9A-Fa-f][0-9A-Fa-f_]*("."[0-9A-Fa-f_]*)?([pP][+-][0-9][0-9_]*)?  {
  return TC_NUMBER;
}

  /* Character literal. */
{TICK}{REGULAR_CHAR}{TICK}    |
{TICK}{ESCAPE_SEQUENCE}{TICK} {
  return TC_STRING;
}

  /* String literal. */
  /* TODO: Handle quoted-string-id strings. */
{QUOTE} {
  YY_SET_START_CONDITION(STRING);
  return TC_STRING;
}

<STRING>{
  {STRING_CHARACTER} {
    return TC_STRING;
  }

  {QUOTE} {
    YY_SET_START_CONDITION(INITIAL);
    return TC_STRING;
  }

  /* A special character at EOF. */
  . {
    return TC_STRING;
  }
}

  /* Comment. */
"(*" {
  xassert(m_commentNesting == 0);
  m_commentNesting++;
  YY_SET_START_CONDITION(COMMENT);
  return TC_COMMENT;
}

<COMMENT>{
  /* Normal characters in a comment. */
  [^(*]+ {
    return TC_COMMENT;
  }

  /* Open paren, not followed by a star. */
  "("[^*] {
    YY_LESS_TEXT(1);
    return TC_COMMENT;
  }

  /* Start of a nested comment. */
  "(*" {
    xassert(m_commentNesting > 0);
    m_commentNesting++;
    return TC_COMMENT;
  }

  /* Star, not followed by a close paren. */
  "*"[^)] {
    YY_LESS_TEXT(1);
    return TC_COMMENT;
  }

  /* Termination of comment. */
  "*)" {
    xassert(m_commentNesting > 0);
    m_commentNesting--;
    if (m_commentNesting == 0) {
      YY_SET_START_CONDITION(INITIAL);
    }
    return TC_COMMENT;
  }

  /* An open paren or star just before EOF. */
  . {
    return TC_COMMENT;
  }
}

  /* TODO: Naming labels. */

  /* Prefix and infix symbols. */
{INFIX_SYMBOL}|{PREFIX_SYMBOL} {
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


// ----------------------- OCaml_FlexLexer ---------------------
int OCaml_FlexLexer::yym_read_input(void *dest, int size)
{
  return bufsrc.fillBuffer(dest, size);
}


void OCaml_FlexLexer::setState(LexerState state)
{
  if (state >= 10) {
    yym_set_start_condition(COMMENT);
    m_commentNesting = state - 10;
    xassert(m_commentNesting > 0);
  }
  else {
    xassert(state != COMMENT);
    yym_set_start_condition((int)state);
    m_commentNesting = 0;
  }
}

LexerState OCaml_FlexLexer::getState() const
{
  int sc = yym_get_start_condition();

  if (sc == COMMENT) {
    xassert(m_commentNesting > 0);

    // Encode the comment state as 10 + nesting.
    return static_cast<LexerState>(10 + m_commentNesting);
  }
  else {
    xassert(m_commentNesting == 0);

    // Other states are encoded as the SC itself.
    return static_cast<LexerState>(sc);
  }
}


// ------------------------- OCaml_Lexer -----------------------
OCaml_Lexer::OCaml_Lexer()
  : lexer(new OCaml_FlexLexer)
{}

OCaml_Lexer::~OCaml_Lexer()
{
  delete lexer;
}


void OCaml_Lexer::beginScan(TextDocumentCore const *buffer, int line, LexerState state)
{
  lexer->bufsrc.beginScan(buffer, line);
  lexer->setState(state);
}


int OCaml_Lexer::getNextToken(TextCategory &code)
{
  int result = lexer->yym_lex();

  if (result == 0) {
    // end of line
    switch ((int)lexer->getState()) {
      case STRING:
        code = TC_STRING;
        break;

      case COMMENT:
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


LexerState OCaml_Lexer::getState() const
{
  return lexer->getState();
}


// ---------------------- OCaml_Highlighter --------------------
string OCaml_Highlighter::highlighterName() const
{
  return "OCaml";
}


// EOF
