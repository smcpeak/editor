/* c_hilite.lex
   lexer for highlighting C/C++
   based on comment.lex, and elsa's lexer.lex */

%smflex 101

/* ----------------------- C definitions ---------------------- */
%{

#include "c_hilite.h"                  // C_Lexer class
#include "bufferlinesource.h"          // BufferLineSource
#include "textcategory.h"              // TC_XXX constants

// lexer context class
class C_FlexLexer : public c_hilite_yyFlexLexer {
public:      // data
  BufferLineSource bufsrc;

protected:   // funcs
  virtual int yym_read_input(void *dest, int size) override;

public:      // funcs
  C_FlexLexer() {}
  ~C_FlexLexer() {}

  int yym_lex();

  void setState(LexerState state) { yym_set_start_condition((int)state); }
  LexerState getState() const     { return (LexerState)(yym_get_start_condition()); }
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
%option yyclass="C_FlexLexer"

/* output file name */
%option outfile="c_hilite.yy.cc"


/* start conditions */
/* these conditions are used to encode lexer state between lines,
   because the highlighter only sees one line at a time */
%x STRING
%x CPP_COMMENT
%x COMMENT
%x PREPROC


/* ------------------- definitions -------------------- */
/* newline */
NL            "\n"

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

  /* C++14 keywords (2.11p1) other than true, false, and nullptr. */
"alignas"          |
"alignof"          |
"asm"              |
"auto"             |
"bool"             |
"break"            |
"case"             |
"catch"            |
"cdecl"            |  /* Where did I get this?  Maybe from an old Borland compiler? */
"char"             |
"char16_t"         |
"char32_t"         |
"class"            |
"const"            |
"constexpr"        |
"const_cast"       |
"continue"         |
"decltype"         |
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
"final"            |  /* Not technically a keyword, but I want to highlight it like one anyway. */
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
"noexcept"         |
"operator"         |
"pascal"           |  /* More Borland stuff I think. */
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
"static_assert"    |
"static_cast"      |
"struct"           |
"switch"           |
"template"         |
"this"             |
"thread_local"     |
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

  /* This is not a keyword but I want to highlight it like one.*/
"override"         return TC_KEYWORD;

  /* I have defined these as a macros in smbase/sm-macros.h and
   * smbase/sm-override.h, and want to see them like a keyword. */
"NOEXCEPT"         |
"NULLABLE"         |
"OVERRIDE"         return TC_KEYWORD;

  /* C++14 2.11p2: alternative representations for operators.
   *
   * Originally I had these as TC_OPERATOR, but my color scheme uses an
   * intense purple color that works well for symbols but is too much
   * for these words. */
"and"              |
"and_eq"           |
"bitand"           |
"bitor"            |
"compl"            |
"not"              |
"not_eq"           |
"or"               |
"or_eq"            |
"xor"              |
"xor_eq"           return TC_KEYWORD;

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
"true"|"false"|"null"|"nullptr"|"TRUE"|"FALSE"|"NULL" {
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


  /* string literal with escaped newline at end: we remember
     we're in a string literal so the next line will be
     highlighted accordingly */
"L"?{QUOTE}({STRCHAR}|({BACKSL}.))*{BACKSL}{NL} {
  YY_SET_START_CONDITION(STRING);
  return TC_STRING;
}

  /* string literal, including one that does not end with
     a quote; that way we'll just highlight the string but
     subsequent lines are normal, on the assumption the user
     will close the string soon */
"L"?{QUOTE}({STRCHAR}|({BACKSL}.))*({QUOTE}|{NL})? {
  return TC_STRING;
}

  /* One-line string literal that ends at EOF. */
"L"?{QUOTE}({STRCHAR}|({BACKSL}.))*{BACKSL} {
  return TC_STRING;
}

  /* string continuation */
<STRING>({STRCHAR}|{BACKSL}.)*{BACKSL}{NL} {
  return TC_STRING;
}

  /* string continuation that ends on this line */
<STRING>({STRCHAR}|({BACKSL}.))*({QUOTE}|{NL})? {
  YY_SET_START_CONDITION(INITIAL);
  return TC_STRING;
}

  /* Continued string literal runs into EOF. */
<STRING>({STRCHAR}|{BACKSL}.)*{BACKSL}? {
  return TC_STRING;
}

  /* character literal, possibly unterminated */
"L"?{TICK}({CCCHAR}|{BACKSL}.)*{TICK}?   {
  return TC_STRING;
}

  /* character literal ending with backslash */
"L"?{TICK}({CCCHAR}|{BACKSL}.)*{TICK}?{BACKSL}   {
  YY_LESS_TEXT(YY_LENG-1);     // put the backslash back
  return TC_STRING;
}

  /* C++ comment continued to next line*/
"//".*{BACKSL}{NL} {
  YY_SET_START_CONDITION(CPP_COMMENT);
  return TC_COMMENT;
}

  /* C++ comment all on one line */
"//".*{NL}? {
  return TC_COMMENT;
}

  /* Continued C++ comment, continuing again. */
<CPP_COMMENT>.*{BACKSL}{NL} {
  return TC_COMMENT;
}

  /* Continued C++ comment ends here. */
<CPP_COMMENT>.*{NL}? {
  YY_SET_START_CONDITION(INITIAL);
  return TC_COMMENT;
}

  /* C comment */
"/""*"([^*]|"*"+[^*/])*"*"+"/" {
  return TC_COMMENT;
}

  /* C comment extending to next line */
"/""*"([^*]|"*"+[^*/])*"*"* {
  YY_SET_START_CONDITION(COMMENT);
  return TC_COMMENT;
}

  /* continuation of C comment, ends here */
<COMMENT>"*/" {
  YY_SET_START_CONDITION(INITIAL);
  return TC_COMMENT;
}

  /* continuation of C comment, comment text */
<COMMENT>([^*]+)|"*" {
  return TC_COMMENT;
}


  /* preprocessor, with C++ comment */
  /* (handling of comments w/in preprocessor directives is fairly
   * ad-hoc right now..) */
^[ \t]*"#".*"//".*{NL}? {
  // find the start of the comment
  char const *p = strchr(YY_TEXT, '/');
  while (p && p[1]!='/') {
    p = strchr(p+1, '/');
  }
  if (p) {
    // put the comment back; it will be matched as TC_COMMENT
    YY_LESS_TEXT(p-YY_TEXT);
  }
  else {
    // shouldn't happen, but not catastrophic if it does..
  }
  return TC_PREPROCESSOR;
}

  /* preprocessor, continuing to next line */
^[ \t]*"#".*{BACKSL}{NL} {
  YY_SET_START_CONDITION(PREPROC);
  return TC_PREPROCESSOR;
}

  /* preprocessor, one line */
^[ \t]*"#".*{NL}? {
  return TC_PREPROCESSOR;
}

  /* continuation of preprocessor, continuing to next line */
<PREPROC>.*{BACKSL}{NL} {
  return TC_PREPROCESSOR;
}

  /* continuation of preprocessor, ending here */
<PREPROC>.*{NL}? {
  YY_SET_START_CONDITION(INITIAL);
  return TC_PREPROCESSOR;
}

  /* continuation of preprocessor, empty line */
<PREPROC><<EOF>> {
  if (bufsrc.lineIsEmpty()) {
    YY_SET_START_CONDITION(INITIAL);
  }
  YY_TERMINATE();
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


// ----------------------- C_FlexLexer ---------------------
int C_FlexLexer::yym_read_input(void *dest, int size)
{
  return bufsrc.fillBuffer(dest, size);
}


// ------------------------- C_Lexer -----------------------
C_Lexer::C_Lexer()
  : lexer(new C_FlexLexer)
{}

C_Lexer::~C_Lexer()
{
  delete lexer;
}


void C_Lexer::beginScan(TextDocumentCore const *buffer, LineIndex line, LexerState state)
{
  lexer->bufsrc.beginScan(buffer, line);
  lexer->setState(state);
}


int C_Lexer::getNextToken(TextCategoryAOA &code)
{
  int result = lexer->yym_lex();

  if (result == 0) {
    // end of line
    switch ((int)lexer->getState()) {
      case STRING:  code = TC_STRING;       break;
      case CPP_COMMENT:
      case COMMENT: code = TC_COMMENT;      break;
      case PREPROC: code = TC_PREPROCESSOR; break;
      default:      code = TC_NORMAL;       break;
    }
    return 0;
  }
  else {
    code = (TextCategory)result;
    return lexer->yym_leng();
  }
}


LexerState C_Lexer::getState() const
{
  return lexer->getState();
}


// ---------------------- C_Highlighter --------------------
string C_Highlighter::highlighterName() const
{
  return "C/C++";
}


// EOF
