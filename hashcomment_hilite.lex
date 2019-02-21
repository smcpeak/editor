/* hashcomment_hilite.lex
   lexer for highlighting hash-commented files */

/* ----------------------- C definitions ---------------------- */
%{

#include "hashcomment_hilite.h"        // HashComment_FlexLexer class
#include "bufferlinesource.h"          // BufferLineSource
#include "textcategory.h"              // TC_XXX constants

// this works around a problem with cygwin & fileno
#define YY_NEVER_INTERACTIVE 1

// Lexer context class.
class HashComment_FlexLexer : public yyFlexLexer {
public:      // data
  BufferLineSource bufsrc;

protected:   // funcs
  virtual int LexerInput(char *buf, int max_size);

public:      // funcs
  HashComment_FlexLexer() {}
  ~HashComment_FlexLexer() {}

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
%option yyclass="HashComment_FlexLexer"

/* output file name */
%option outfile="hashcomment_hilite.yy.cc"


/* Start conditions. */
/* These conditions are (also) used to encode lexer state between lines,
   because the highlighter only sees one line at a time. */

/* After '#', or its continuation onto subsequent lines. */
%x COMMENT


/* ------------------- definitions -------------------- */
/* newline */
NL            "\n"

/* backslash */
BACKSL        "\\"

/* ------------- token definition rules --------------- */
%%

  /* The INITIAL state corresponds to being outside any comment. */

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

  /* Anything else. */
[^#]+ {
  return TC_NORMAL;
}


%%
// third section: extra C++ code


// ----------------------- HashComment_FlexLexer ---------------------
int HashComment_FlexLexer::LexerInput(char *buf, int max_size)
{
  return bufsrc.fillBuffer(buf, max_size);
}


// ------------------------- HashComment_Lexer -----------------------
HashComment_Lexer::HashComment_Lexer()
  : lexer(new HashComment_FlexLexer)
{}

HashComment_Lexer::~HashComment_Lexer()
{
  delete lexer;
}


void HashComment_Lexer::beginScan(TextDocumentCore const *buffer, int line, LexerState state)
{
  lexer->bufsrc.beginScan(buffer, line);
  lexer->setState(state);
}


int HashComment_Lexer::getNextToken(TextCategory &code)
{
  int result = lexer->yylex();

  if (result == 0) {
    // end of line
    switch ((int)lexer->getState()) {
      default:
        xfailure("unrecognized lexer state");

      case INITIAL:
        code = TC_NORMAL;
        break;

      case COMMENT:
        code = TC_COMMENT;
        break;
    }
    return 0;
  }
  else {
    code = (TextCategory)result;
    return lexer->YYLeng();
  }
}


LexerState HashComment_Lexer::getState() const
{
  return lexer->getState();
}


// ---------------------- HashComment_Highlighter --------------------
string HashComment_Highlighter::highlighterName() const
{
  return "HashComment";
}


// EOF
