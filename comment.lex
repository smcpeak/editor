/* comment.lex
   simple lexer for strings and comments, to experiment
   with using Flex for syntax highlighting */
   
/* ----------------------- C definitions ---------------------- */
%{

#include "comment.h"       // CommentLexer class
#include "style.h"         // ST_XXX constants

// this works around a problem with cygwin & fileno
#define YY_NEVER_INTERACTIVE 1

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
%option yyclass="CommentLexer"

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
// -------- add'l C++ code ---------

#include "buffer.h"      // BufferCore


// -------------------- CommentLexer -------------------
CommentLexer::CommentLexer()
  : buffer(NULL),
    bufferLine(0),
    lineLength(0),
    nextSlurpCol(0)
{}


CommentLexer::~CommentLexer()
{}


void CommentLexer::beginScan(BufferCore const *b, int line, int state)
{
  // set up variables so we'll be able to do LexerInput()
  buffer = b;
  bufferLine = line;
  lineLength = buffer->lineLength(line);
  nextSlurpCol = 0;
  BEGIN(state);
}


// this is called by the Flex lexer when it needs more data for
// its internal buffer; interface is similar to read(2)
int CommentLexer::LexerInput(char* buf, int max_size)
{
  if (nextSlurpCol == lineLength) {
    return 0;         // EOL
  }

  int len = min(max_size, lineLength-nextSlurpCol);
  buffer->getLine(bufferLine, nextSlurpCol, buf, len);
  nextSlurpCol += len;

  return len;
}


bool CommentLexer::getNextToken(int &len, int &code)
{
  code = yylex();
  if (!code) {
    return false;     // EOL
  }                   
  
  len = yyleng;
  return true;
}


int CommentLexer::getState() const
{
  return YY_START;
}
