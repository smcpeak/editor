/* comment.lex
   simple lexer for strings and comments, to experiment
   with using Flex for syntax highlighting */

%smflex 101

/* ----------------------- C definitions ---------------------- */
%{

#include "comment.h"                   // CommentLexer class
#include "bufferlinesource.h"          // BufferLineSource
#include "textcategory.h"              // TC_XXX constants

// lexer context class
class CommentFlexLexer : public comment_yyFlexLexer {
public:      // data
  BufferLineSource bufsrc;

protected:   // funcs
  virtual int yym_read_input(void *dest, int size) override;

public:      // funcs
  CommentFlexLexer() {}
  ~CommentFlexLexer() {}

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
  return TC_STRING;
}

  /* string literal with escaped newline at end: we remember
     we're in a string literal so the next line will be
     highlighted accordingly */
"L"?{QUOTE}({STRCHAR}|({BACKSL}{ANY}))*{BACKSL} {
  YY_SET_START_CONDITION(STRING);
  return TC_STRING;
}

  /* string continuation that ends on this line */
<STRING>({STRCHAR}|({BACKSL}{ANY}))*{QUOTE}? {
  YY_SET_START_CONDITION(INITIAL);
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


  /* anything else */
([^\"/]+)|"/" {
  return TC_NORMAL;
}


%%
// third section: extra C++ code


// ----------------------- CommentFlexLexer ---------------------
int CommentFlexLexer::yym_read_input(void *dest, int size)
{
  return bufsrc.fillBuffer(dest, size);
}


// ------------------------- CommentLexer -----------------------
CommentLexer::CommentLexer()
  : lexer(new CommentFlexLexer)
{}

CommentLexer::~CommentLexer()
{
  delete lexer;
}


void CommentLexer::beginScan(TextDocumentCore const *buffer, LineIndex line, LexerState state)
{
  lexer->bufsrc.beginScan(buffer, line);
  lexer->setState(state);
}


int CommentLexer::getNextToken(TextCategoryAOA &code)
{
  int result = lexer->yym_lex();

  if (result == 0) {
    // end of line
    switch ((int)lexer->getState()) {
      case STRING:  code = TC_STRING;  break;
      case COMMENT: code = TC_COMMENT; break;
      default:      code = TC_NORMAL;  break;
    }
    return 0;
  }
  else {
    code = (TextCategory)result;
    return lexer->yym_leng();
  }
}


LexerState CommentLexer::getState() const
{
  return lexer->getState();
}


// --------------------- CommentHighlighter ----------------------
string CommentHighlighter::highlighterName() const
{
  return "Comment";
}


// EOF
