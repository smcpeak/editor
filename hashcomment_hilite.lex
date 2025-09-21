/* hashcomment_hilite.lex
   lexer for highlighting hash-commented files */

%smflex 101

/* ----------------------- C definitions ---------------------- */
%{

#include "hashcomment_hilite.h"        // HashComment_Lexer class
#include "bufferlinesource.h"          // BufferLineSource
#include "textcategory.h"              // TC_XXX constants

// Lexer context class.
class HashComment_FlexLexer : public hashcomment_hilite_yyFlexLexer {
public:      // data
  BufferLineSource bufsrc;

protected:   // funcs
  virtual int yym_read_input(void *dest, int size) override;

public:      // funcs
  HashComment_FlexLexer() {}
  ~HashComment_FlexLexer() {}

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
  YY_SET_START_CONDITION(COMMENT);
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
  YY_SET_START_CONDITION(INITIAL);
  return TC_COMMENT;
}

  /* Anything else. */
[^#]+ {
  return TC_NORMAL;
}


%%
// third section: extra C++ code


// ----------------------- HashComment_FlexLexer ---------------------
int HashComment_FlexLexer::yym_read_input(void *dest, int size)
{
  return bufsrc.fillBuffer(dest, size);
}


// ------------------------- HashComment_Lexer -----------------------
HashComment_Lexer::HashComment_Lexer()
  : lexer(new HashComment_FlexLexer)
{}

HashComment_Lexer::~HashComment_Lexer()
{
  delete lexer;
}


void HashComment_Lexer::beginScan(TextDocumentCore const *buffer, LineIndex line, LexerState state)
{
  lexer->bufsrc.beginScan(buffer, line);
  lexer->setState(state);
}


int HashComment_Lexer::getNextToken(TextCategoryAOA &code)
{
  int result = lexer->yym_lex();

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
    return lexer->yym_leng();
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
