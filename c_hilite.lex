/* c_hilite.lex
   lexer for highlighting C/C++
   based on comment.lex, and elsa's lexer.lex */

/* ----------------------- C definitions ---------------------- */
%{

#include "c_hilite.h"                  // C_FlexLexer class
#include "bufferlinesource.h"          // BufferLineSource
#include "textcategory.h"              // TC_XXX constants

// this works around a problem with cygwin & fileno
#define YY_NEVER_INTERACTIVE 1

// lexer context class
class C_FlexLexer : public yyFlexLexer {
public:      // data
  BufferLineSource bufsrc;

protected:   // funcs
  virtual int LexerInput(char *buf, int max_size);

public:      // funcs
  C_FlexLexer() {}
  ~C_FlexLexer() {}

  virtual int yylex();

  void setState(LexerState state) { BEGIN((int)state); }
  LexerState getState() const     { return (LexerState)(YY_START); }
};

%}


/* -------------------- flex options ------------------ */
/* no wrapping is needed; setting this means we don't have to link with libfl.a */
%option noyywrap

/* don't use the default-echo rules */
%option nodefault

/* I don't call unput */
%option nounput

/* generate a c++ lexer */
%option c++

/* use the "fast" algorithm with no table compression */
%option full

/* utilize character equivalence classes */
%option ecs

/* the scanner is never interactive */
%option never-interactive

/* and I will define the class */
%option yyclass="C_FlexLexer"

/* output file name */
%option outfile="c_hilite.yy.cc"


/* start conditions */
/* these conditions are used to encode lexer state between lines,
   because the highlighter only sees one line at a time */
%x STRING
%x COMMENT
%x PREPROC


/* ------------------- definitions -------------------- */
/* newline */
NL            "\n"

/* anything but newline */
NOTNL         .

/* any of 256 source characters */
ANY           ({NOTNL}|{NL})

/* backslash */
BACKSL        "\\"

/* letter or underscore */
LETTER        [A-Za-z_]

/* letter or underscore or digit */
ALNUM         [A-Za-z_0-9]

/* decimal digit */
DIGIT         [0-9]

/* sequence of decimal digits */
DIGITS        ({DIGIT}+)

/* sign of a number */
SIGN          ("+"|"-")

/* integer suffix */
/* added 'LL' option for GNU long long compatibility.. */
ELL_SUFFIX    [lL]([lL]?)
INT_SUFFIX    ([uU]{ELL_SUFFIX}?|{ELL_SUFFIX}[uU]?)

/* floating-point suffix letter */
FLOAT_SUFFIX  [flFL]

/* normal string character: any but quote, newline, or backslash */
STRCHAR       [^\"\n\\]

/* double quote */
QUOTE         [\"]

/* normal character literal character: any but single-quote, newline, or backslash */
CCCHAR        [^\'\n\\]

/* single quote */
TICK          [\']


/* ------------- token definition rules --------------- */
%%

  /* keywords other than true and false */
"asm"              |
"auto"             |
"break"            |
"bool"             |
"case"             |
"catch"            |
"cdecl"            |
"char"             |
"class"            |
"const"            |
"const_cast"       |
"continue"         |
"default"          |
"delete"           |
"do"               |
"double"           |
"dynamic_cast"     |
"else"             |
"enum"             |
"explicit"         |
"export"           |
"extern"           |
"float"            |
"for"              |
"friend"           |
"goto"             |
"if"               |
"inline"           |
"int"              |
"long"             |
"mutable"          |
"namespace"        |
"new"              |
"operator"         |
"pascal"           |
"private"          |
"protected"        |
"public"           |
"register"         |
"reinterpret_cast" |
"return"           |
"short"            |
"signed"           |
"sizeof"           |
"static"           |
"static_cast"      |
"struct"           |
"switch"           |
"template"         |
"this"             |
"throw"            |
"try"              |
"typedef"          |
"typeid"           |
"typename"         |
"union"            |
"unsigned"         |
"using"            |
"virtual"          |
"void"             |
"volatile"         |
"wchar_t"          |
"while"            return TC_KEYWORD;

  /* GNU keywords */
"__attribute__"    |
"__extension__"    |
"__typeof__"       |
"typeof"           return TC_KEYWORD;

  /* operators, punctuators */
"("                |
")"                |
"["                |
"]"                |
"->"               |
"::"               |
"."                |
"!"                |
"~"                |
"+"                |
"-"                |
"++"               |
"--"               |
"&"                |
"*"                |
".*"               |
"->*"              |
"/"                |
"%"                |
"<<"               |
">>"               |
"<"                |
"<="               |
">"                |
">="               |
"=="               |
"!="               |
"^"                |
"|"                |
"&&"               |
"||"               |
"?"                |
":"                |
"="                |
"*="               |
"/="               |
"%="               |
"+="               |
"-="               |
"&="               |
"^="               |
"|="               |
"<<="              |
">>="              |
","                |
"..."              |
";"                |
"{"                |
"}"                return TC_OPERATOR;

  /* my extension */
"==>"              return TC_OPERATOR;

  /* to avoid backing up; treat as two consecutive "." */
".."               return TC_OPERATOR;

  /* special values */
"true"|"false"|"null"|"TRUE"|"FALSE"|"NULL" {
  return TC_SPECIAL;
}

  /* additional words to highlight as keywords */
"string" {
  return TC_KEYWORD;
}

  /* identifiers */
{LETTER}{ALNUM}*   return TC_NORMAL;


  /* integer literal; dec or hex */
"0"{INT_SUFFIX}?                   |
[1-9][0-9]*{INT_SUFFIX}?           |
[0][xX][0-9A-Fa-f]*{INT_SUFFIX}?   {
  return TC_NUMBER;
}

  /* integer literal; oct */
[0][0-7]+{INT_SUFFIX}?             {
  return TC_NUMBER2;
}

  /* floating literal */
{DIGITS}"."{DIGITS}?([eE]{SIGN}?{DIGITS}?)?{FLOAT_SUFFIX}?   |
{DIGITS}"."?([eE]{SIGN}?{DIGITS}?)?{FLOAT_SUFFIX}?	    |
"."{DIGITS}([eE]{SIGN}?{DIGITS}?)?{FLOAT_SUFFIX}?	    {
  return TC_NUMBER;
}


  /* string literal, including one that does not end with
     a quote; that way we'll just highlight the string but
     subsequent lines are normal, on the assumption the user
     will close the string soon */
"L"?{QUOTE}({STRCHAR}|({BACKSL}{ANY}))*{QUOTE}? {
  return TC_STRING;
}

  /* string literal with escaped newline at end: we remember
     we're in a string literal so the next line will be
     highlighted accordingly */
"L"?{QUOTE}({STRCHAR}|({BACKSL}{ANY}))*{BACKSL} {
  BEGIN(STRING);
  return TC_STRING;
}

  /* string continuation that ends on this line */
<STRING>({STRCHAR}|({BACKSL}{ANY}))*{QUOTE}? {
  BEGIN(INITIAL);
  return TC_STRING;
}

  /* string continuation */
<STRING>({STRCHAR}|{BACKSL}{ANY})*{BACKSL} {
  return TC_STRING;
}

  /* this won't actually happen in the context of the editor, because
   * literal newlines won't be passed, but it makes flex happy since
   * it now thinks all contigencies are handled */
<STRING>{NL} {
  return TC_STRING;
}


  /* character literal, possibly unterminated */
"L"?{TICK}({CCCHAR}|{BACKSL}{ANY})*{TICK}?{BACKSL}?   {
  return TC_STRING;
}

  /* C++ comment */
"//"{ANY}* {
  return TC_COMMENT;
}


  /* C comment */
"/""*"([^*]|"*"[^/])*"*"?"*/" {
  return TC_COMMENT;
}

  /* C comment extending to next line */
"/""*"([^*]|"*"[^/])*"*"? {
  BEGIN(COMMENT);
  return TC_COMMENT;
}

  /* continuation of C comment, ends here */
<COMMENT>"*/" {
  BEGIN(INITIAL);
  return TC_COMMENT;
}

  /* continuation of C comment, comment text */
<COMMENT>([^*]+)|"*" {
  return TC_COMMENT;
}


  /* preprocessor, with C++ comment */
  /* (handling of comments w/in preprocessor directives is fairly
   * ad-hoc right now..) */
^[ \t]*"#"{ANY}*"//"{ANY}* {
  // find the start of the comment
  char const *p = strchr(yytext, '/');
  while (p && p[1]!='/') {
    p = strchr(p+1, '/');
  }
  if (p) {
    // put the comment back; it will be matched as TC_COMMENT
    yyless(p-yytext);
  }
  else {
    // shouldn't happen, but not catastrophic if it does..
  }
  return TC_PREPROCESSOR;
}

  /* preprocessor, continuing to next line */
^[ \t]*"#"{ANY}*{BACKSL} {
  BEGIN(PREPROC);
  return TC_PREPROCESSOR;
}

  /* preprocessor, one line */
^[ \t]*"#"{ANY}* {
  return TC_PREPROCESSOR;
}

  /* continuation of preprocessor, continuing to next line */
<PREPROC>{ANY}*{BACKSL} {
  return TC_PREPROCESSOR;
}

  /* continuation of preprocessor, ending here */
<PREPROC>{ANY}+ {
  BEGIN(INITIAL);
  return TC_PREPROCESSOR;
}

  /* continuation of preprocessor, empty line */
<PREPROC><<EOF>> {
  if (bufsrc.nextSlurpCol == 0) {
    // buffer was empty
    BEGIN(INITIAL);
  }
  yyterminate();
}


  /* whitespace */
[ \t\n\f\v\r]+ {
  return TC_NORMAL;
}


  /* anything else */
{ANY} {
  return TC_ERROR;
}


%%
// third section: extra C++ code


// ----------------------- C_FlexLexer ---------------------
int C_FlexLexer::LexerInput(char *buf, int max_size)
{
  return bufsrc.fillBuffer(buf, max_size);
}


// ------------------------- C_Lexer -----------------------
C_Lexer::C_Lexer()
  : lexer(new C_FlexLexer)
{}

C_Lexer::~C_Lexer()
{
  delete lexer;
}


void C_Lexer::beginScan(TextDocumentCore const *buffer, int line, LexerState state)
{
  lexer->bufsrc.beginScan(buffer, line);
  lexer->setState(state);
}


int C_Lexer::getNextToken(TextCategory &code)
{
  int result = lexer->yylex();

  if (result == 0) {
    // end of line
    switch ((int)lexer->getState()) {
      case STRING:  code = TC_STRING;       break;
      case COMMENT: code = TC_COMMENT;      break;
      case PREPROC: code = TC_PREPROCESSOR; break;
      default:      code = TC_NORMAL;       break;
    }
    return 0;
  }
  else {
    code = (TextCategory)result;
    return lexer->YYLeng();
  }
}


LexerState C_Lexer::getState() const
{
  return lexer->getState();
}
