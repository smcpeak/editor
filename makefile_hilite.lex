/* makefile_hilite.lex
   lexer for highlighting Makefiles */

/* ----------------------- C definitions ---------------------- */
%{

#include "makefile_hilite.h"           // Makefile_FlexLexer class
#include "bufferlinesource.h"          // BufferLineSource
#include "textcategory.h"              // TC_XXX constants

// this works around a problem with cygwin & fileno
#define YY_NEVER_INTERACTIVE 1

// Lexer context class.
class Makefile_FlexLexer : public yyFlexLexer {
public:      // data
  BufferLineSource bufsrc;

protected:   // funcs
  virtual int LexerInput(char *buf, int max_size);

public:      // funcs
  Makefile_FlexLexer() {}
  ~Makefile_FlexLexer() {}

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
%option yyclass="Makefile_FlexLexer"

/* output file name */
%option outfile="makefile_hilite.yy.cc"


/* Start conditions. */
/* These conditions are (also) used to encode lexer state between lines,
   because the highlighter only sees one line at a time. */

/* After "identifier". */
%x AFTER_IDENTIFIER

/* After '#', or its continuation onto subsequent lines. */
%x COMMENT

/* After "identifier:". */
%x RULE

/* After a tab character. */
%x SHELL

/* Text that is treated by 'make' as a string.  These are *not*
 * enclosed in double-quotes. */
%x STRING


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

/* Whitespace. */
WHITESPACE    [ \t\n\f\v\r]
TAB           [\t]


/* ------------- token definition rules --------------- */
%%

  /* The INITIAL state corresponds to the beginning of a line. */

  /* Operator at start of line; stay in initial state. */
"-" {
  return TC_OPERATOR;
}

  /* Keywords that begin a directive. */
  /* The handling of "export" is not ideal because what comes after
   * it should be highlighted like a variable assignment. */
"ifeq"             |
"ifneq"            |
"ifdef"            |
"ifndef"           |
"else"             |
"endif"            |
"export"           |
"include" {
  BEGIN(STRING);
  return TC_KEYWORD;
}

  /* Directive keywords I want to highlight even more prominently. */
"define"           |
"endef" {
  BEGIN(STRING);
  return TC_SPECIAL;
}

  /* Operator within a string. */
<STRING>[(),${}]+ {
  return TC_OPERATOR;
}

  /* Remainder of a string. */
<STRING>[^(),${}\n]+ {
  return TC_NORMAL;
}

  /* End of string. */
<STRING>{NL} {
  BEGIN(INITIAL);
  return TC_NORMAL;
}

  /* -------------- Line starting with an identifier ------------ */
  /* Identifier: variable or rule target name. */
{LETTER}{ALNUM}* {
  BEGIN(AFTER_IDENTIFIER);
  return TC_NORMAL;
}

<AFTER_IDENTIFIER>[ \t]+ {
  return TC_NORMAL;
}

<AFTER_IDENTIFIER>("="|":="|"+=") {
  BEGIN(STRING);
  return TC_OPERATOR;
}

<AFTER_IDENTIFIER>(":") {
  BEGIN(RULE);
  return TC_OPERATOR;
}

<AFTER_IDENTIFIER>{NL} {
  BEGIN(INITIAL);
  return TC_NORMAL;
}

  /* Transition on any other operator to treating it as a string. */
<AFTER_IDENTIFIER>[$(){}]+ {
  BEGIN(STRING);
  return TC_OPERATOR;
}

  /* And anything else too. */
<AFTER_IDENTIFIER>[^$(){}] {
  BEGIN(STRING);
  return TC_NORMAL;
}

  /* Rule target name.  Cannot start with "-". */
[a-zA-Z0-9_%./][a-zA-Z0-9_%./-]* {
  BEGIN(RULE);
  return TC_NORMAL;
}

<RULE>[:$(){},]+ {
  return TC_OPERATOR;
}

<RULE>[^:$(){},\n]+ {
  return TC_NORMAL;
}

<RULE>{NL} {
  BEGIN(INITIAL);
  return TC_NORMAL;
}

  /* ------------------------ Comments -------------------------- */
  /* Comment with continuation to next line. */
"#".*{BACKSL}{NL} {
  BEGIN(COMMENT);
  return TC_COMMENT;
}

  /* Comment without continuation. (The regex here also matches
   * a continued comment, but the one above takes precedence.) */
"#".*{NL}? {
  return TC_COMMENT;
}

  /* Continuation of comment with further extension. */
<COMMENT>.*{BACKSL}{NL} {
  return TC_COMMENT;
}

  /* Continuation of comment, ends here. */
<COMMENT>.*{NL}? {
  BEGIN(INITIAL);
  return TC_COMMENT;
}

  /* --------------------- Shell lines --------------------- */
  /* Start of multi-line shell line. */
{TAB}.*{BACKSL}{NL} {
  BEGIN(SHELL);
  return TC_STRING;
}

  /* One-line shell line. */
{TAB}.*{NL}? {
  return TC_STRING;
}

  /* Continued shell line, still continuing. */
<SHELL>.*{BACKSL}{NL} {
  return TC_STRING;
}

  /* End of continued shell line. */
<SHELL>.*{NL}? {
  BEGIN(INITIAL);
  return TC_STRING;
}


  /* --------------------- Miscellaneous --------------------- */
  /* Spaces used as indentation. */
  /* (I am not really sure where these can go in Makefiles.) */
[ ]+ {
  return TC_NORMAL;
}

  /* Assume a line starting with an operator is a string. */
[(),${}]+ {
  BEGIN(STRING);
  return TC_OPERATOR;
}

  /* Uncategorized. */
{ANY} {
  return TC_ERROR;
}


%%
// third section: extra C++ code


// ----------------------- Makefile_FlexLexer ---------------------
int Makefile_FlexLexer::LexerInput(char *buf, int max_size)
{
  return bufsrc.fillBuffer(buf, max_size);
}


// ------------------------- Makefile_Lexer -----------------------
Makefile_Lexer::Makefile_Lexer()
  : lexer(new Makefile_FlexLexer)
{}

Makefile_Lexer::~Makefile_Lexer()
{
  delete lexer;
}


void Makefile_Lexer::beginScan(TextDocumentCore const *buffer, int line, LexerState state)
{
  lexer->bufsrc.beginScan(buffer, line);
  lexer->setState(state);
}


int Makefile_Lexer::getNextToken(TextCategory &code)
{
  int result = lexer->yylex();

  if (result == 0) {
    // end of line
    switch ((int)lexer->getState()) {
      case COMMENT: code = TC_COMMENT;      break;
      default:      code = TC_NORMAL;       break;
    }
    return 0;
  }
  else {
    code = (TextCategory)result;
    return lexer->YYLeng();
  }
}


LexerState Makefile_Lexer::getState() const
{
  return lexer->getState();
}


// ---------------------- Makefile_Highlighter --------------------
string Makefile_Highlighter::highlighterName() const
{
  return "Makefile";
}


// EOF
