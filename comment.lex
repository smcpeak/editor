/* comment.lex
   simple lexer for strings and comments, to experiment
   with using Flex for syntax highlighting */

/* ----------------------- C definitions ---------------------- */
%{

#include "comment.h"                   // CommentLexer class
#include "bufferlinesource.h"          // BufferLineSource
#include "style.h"                     // ST_XXX constants

// this works around a problem with cygwin & fileno
#define YY_NEVER_INTERACTIVE 1

// lexer context class
class CommentFlexLexer : public yyFlexLexer {
public:      // data
  BufferLineSource bufsrc;

protected:   // funcs
  virtual int LexerInput(char *buf, int max_size);

public:      // funcs
  CommentFlexLexer() {}
  ~CommentFlexLexer() {}

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
%option yyclass="CommentFlexLexer"

/* output file name */
%option outfile="comment.yy.cc"


/* start conditions */
/* these conditions are used to encode lexer state between lines,
   because the highlighter only sees one line at a time */
%x STRING
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

/* normal string character: any but quote, newline, or backslash */
STRCHAR       [^\"\n\\]

/* double quote */
QUOTE         [\"]


/* ------------- token definition rules --------------- */
%%

  /* string literal, including one that does not end with
     a quote; that way we'll just highlight the string but
     subsequent lines are normal, on the assumption the user
     will close the string soon */
"L"?{QUOTE}({STRCHAR}|({BACKSL}{ANY}))*{QUOTE}? {
  return ST_STRING;
}

  /* string literal with escaped newline at end: we remember
     we're in a string literal so the next line will be
     highlighted accordingly */
"L"?{QUOTE}({STRCHAR}|({BACKSL}{ANY}))*{BACKSL} {
  BEGIN(STRING);
  return ST_STRING;
}

  /* string continuation that ends on this line */
<STRING>({STRCHAR}|({BACKSL}{ANY}))*{QUOTE}? {
  BEGIN(INITIAL);
  return ST_STRING;
}

  /* string continuation */
<STRING>({STRCHAR}|{BACKSL}{ANY})*{BACKSL} {
  return ST_STRING;
}

  /* this won't actually happen in the context of the editor, because
   * literal newlines won't be passed, but it makes flex happy since
   * it now thinks all contigencies are handled */
<STRING>{NL} {
  return ST_STRING;
}


  /* C++ comment */
"//"{ANY}* {
  return ST_COMMENT;
}


  /* C comment */
"/""*"([^*]|"*"[^/])*"*"?"*/" {
  return ST_COMMENT;
}

  /* C comment extending to next line */
"/""*"([^*]|"*"[^/])*"*"? {
  BEGIN(COMMENT);
  return ST_COMMENT;
}

  /* continuation of C comment, ends here */
<COMMENT>"*/" {
  BEGIN(INITIAL);
  return ST_COMMENT;
}

  /* continuation of C comment, comment text */
<COMMENT>([^*]+)|"*" {
  return ST_COMMENT;
}


  /* anything else */
([^\"/]+)|"/" {
  return ST_NORMAL;
}


%%
// third section: extra C++ code


// ----------------------- CommentFlexLexer ---------------------
int CommentFlexLexer::LexerInput(char *buf, int max_size)
{
  return bufsrc.fillBuffer(buf, max_size);
}


// ------------------------- CommentLexer -----------------------
CommentLexer::CommentLexer()
  : lexer(new CommentFlexLexer)
{}

CommentLexer::~CommentLexer()
{
  delete lexer;
}


void CommentLexer::beginScan(BufferCore const *buffer, int line, LexerState state)
{
  lexer->bufsrc.beginScan(buffer, line);
  lexer->setState(state);
}


int CommentLexer::getNextToken(Style &code)
{
  int result = lexer->yylex();

  if (result == 0) {
    // end of line
    switch ((int)lexer->getState()) {
      case STRING:  code = ST_STRING;  break;
      case COMMENT: code = ST_COMMENT; break;
      default:      code = ST_NORMAL;  break;
    }
    return 0;
  }
  else {
    code = (Style)result;
    return lexer->YYLeng();
  }
}


LexerState CommentLexer::getState() const
{
  return lexer->getState();
}
