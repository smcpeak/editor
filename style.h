// style.h
// data structures to represent descriptions of character styles

#ifndef STYLE_H
#define STYLE_H

#include "macros.h"        // DMEMB, CMEMB
#include "array.h"         // ArrayStack
#include "str.h"           // string


// standard styles; I envision being able to add more dynamically,
// but to have this set always available by default
enum Style {
  ST_ZERO=0,               // not used; 0 is used to signal EOL during lexing
  ST_NORMAL,               // normal text
  ST_SELECTION,            // selected text
  ST_HITS,                 // buffer text that matches a search string
  ST_ERROR,                // text that can't be lexed
  ST_COMMENT,              // comment
  ST_STRING,               // string literal
  ST_KEYWORD,              // keyword
  ST_SPECIAL,              // special value: true, false, NULL
  ST_NUMBER,               // numeric literal
  ST_NUMBER2,              // numeric literal, alternate (I use this for octal)
  ST_OPERATOR,             // operator
  ST_PREPROCESSOR,         // preprocessor directive
};


// specify a color/font for a run of characters
class StyleEntry {
public:
  Style style;           // color/font to use
  int length;            // # of characters covered

public:
  StyleEntry() : style(ST_NORMAL), length(1) {}
  StyleEntry(Style S, int L) : style(S), length(L)
    { xassert(length > 0); }
  StyleEntry(StyleEntry const &obj)
    : DMEMB(style), DMEMB(length) {}
  StyleEntry& operator=(StyleEntry const &obj)
    { CMEMB(style); CMEMB(length); return *this; }
};


// style info for an entire line
class LineStyle : public ArrayStack<StyleEntry> {
public:      // data
  // style of the characters beyond the last entry
  Style endStyle;

public:
  LineStyle(Style end) : endStyle(end) {}

  // clear existing runs
  void clear(Style end)
    { empty(); endStyle=end; }

  // add a new style run to those already present
  void append(Style style, int length);

  // overwrite a subsequence of characters with a given style;
  // 'length' can be 0 to mean infinite
  void overlay(int start, int length, Style style);

  // debugging: render the runs as a string
  string asString() const;          // e.g. "[1,4][2,3][4"
  string asUnaryString() const;     // e.g. "11112224..."
};


// iterator for walking over LineStyle descriptors
class LineStyleIter {
private:
  LineStyle const &styles; // array of styles
  int entry;               // which entry of 'styles' we're on

public:
  int length;              // how many chars remain on this run (0=infinite)
  Style style;             // style of the current run

public:
  LineStyleIter(LineStyle const &s);

  // advance the iterator a certain number of characters
  void advanceChars(int n);

  // advance to the next run
  void nextRun();

  // true if we're on the last, infinite run
  bool atEnd() const { return length==0; }
};


#endif // STYLE_H
