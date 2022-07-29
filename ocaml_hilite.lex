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
"0"                   |
[1-9][0-9]*           |
[0][xX][0-9A-Fa-f]*   {
  return TC_NUMBER;
}

  /* integer literal; oct */
[0][0-7]+             {
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
{QUOTE}({STRCHAR}|({BACKSL}.))*{BACKSL}{NL} {
  YY_SET_START_CONDITION(STRING);
  return TC_STRING;
}

  /* string literal, including one that does not end with
     a quote; that way we'll just highlight the string but
     subsequent lines are normal, on the assumption the user
     will close the string soon */
{QUOTE}({STRCHAR}|({BACKSL}.))*({QUOTE}|{NL})? {
  return TC_STRING;
}

  /* One-line string literal that ends at EOF. */
{QUOTE}({STRCHAR}|({BACKSL}.))*{BACKSL} {
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

  /* Start of comment. */
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
