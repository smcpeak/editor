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


/* start conditions */
/* these conditions are used to encode lexer state between lines,
   because the highlighter only sees one line at a time */
%x COMMENT


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

  /* Keywords. */
"define"           |
"else"             |
"endef"            |
"endif"            |
"ifeq"             |
"include"          return TC_KEYWORD;

  /* Punctuation. */
"="                |
"-"                |
","                |
":="               |
":"                |
"("                |
")"                |
"$"                |
"%"                |
"+="               return TC_OPERATOR;

  /* Identifiers and numbers. */
{ALNUM}+ {
  return TC_NORMAL;
}

  /* Comment with continuation to next line. */
"#".*{BACKSL} {
  BEGIN(COMMENT);
  return TC_COMMENT;
}

  /* Comment without continuation. (The regex here also matches
   * a continued comment, but the one above takes precedence.) */
"#".* {
  return TC_COMMENT;
}

  /* Continuation of comment with further extesion. */
<COMMENT>.*{BACKSL} {
  return TC_COMMENT;
}

  /* Continuation of comment, ends here. */
<COMMENT>.* {
  BEGIN(INITIAL);
  return TC_COMMENT;
}

  /* Whitespace. */
[ \t\n\f\v\r]+ {
  return TC_NORMAL;
}

  /* Anything else. */
{ANY} {
  return TC_NORMAL;
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
