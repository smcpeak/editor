// style.h
// data structures to represent descriptions of character styles

#ifndef STYLE_H
#define STYLE_H

#include "macros.h"        // DMEMB, CMEMB
#include "array.h"         // ArrayStack
#include "str.h"           // string

// specify a color/font for a run of characters
class StyleEntry {
public:
  int style;             // color/font to use
  int length;            // # of characters covered

public:
  StyleEntry() : style(-1), length(1) {}
  StyleEntry(int S, int L) : style(S), length(L) 
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
  int endStyle;

public:
  LineStyle(int end) : endStyle(end) {}

  // clear existing runs
  void clear(int end)
    { empty(); endStyle=end; }

  // add a new style run to those already present
  void append(int style, int length)
    { push(StyleEntry(style, length)); }

  // overwrite a subsequence of characters with a given style;
  // 'length' can be 0 to mean infinite
  void overlay(int start, int length, int style);
  
  // debugging: render the runs as a string
  string asString() const;
};


// iterator for walking over LineStyle descriptors
class LineStyleIter {
private:
  LineStyle const &styles; // array of styles
  int entry;               // which entry of 'styles' we're on

public:
  int length;              // how many chars remain on this run (0=infinite)
  int style;               // style of the current run

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
