// textcategory.h
// data structures to represent descriptions of character styles

#ifndef TEXTCATEGORY_H
#define TEXTCATEGORY_H

#include "macros.h"        // DMEMB, CMEMB
#include "array.h"         // ArrayStack
#include "str.h"           // string

class LineCategoryIter;


// Standard categories; I envision being able to add more dynamically,
// but to have this set always available by default.
enum TextCategory {
  TC_ZERO=0,               // not used; 0 is used to signal EOL during lexing

  // General text editor categories.
  TC_NORMAL,               // normal text
  TC_SELECTION,            // selected text
  TC_HITS,                 // buffer text that matches a search string

  // Categories for any language.
  TC_ERROR,                // text that can't be lexed

  // Categories for C/C++ and similar languages.
  TC_COMMENT,              // comment
  TC_STRING,               // string literal
  TC_KEYWORD,              // keyword
  TC_SPECIAL,              // special value: true, false, NULL
  TC_NUMBER,               // numeric literal
  TC_NUMBER2,              // numeric literal, alternate (I use this for octal)
  TC_OPERATOR,             // operator
  TC_PREPROCESSOR,         // preprocessor directive

  // Categories for unified diff output.
  TC_DIFF_CONTEXT,         // Context output.
  TC_DIFF_OLD_FILE,        // "---" line showing the old file name.
  TC_DIFF_NEW_FILE,        // "+++" line showing the new file name.
  TC_DIFF_SECTION,         // "@@ ... @@" line showing a section within a file.
  TC_DIFF_REMOVAL,         // "-" line showing removed text.
  TC_DIFF_ADDITION,        // "+" line showing added text.

  NUM_STANDARD_TEXT_CATEGORIES
};


// Specify a category for a run of characters.
class TCSpan {
public:
  TextCategory category;     // color/font to use
  int length;                // # of characters covered

public:
  TCSpan() : category(TC_NORMAL), length(1) {}
  TCSpan(TextCategory S, int L) : category(S), length(L)
    { xassert(length > 0); }
  TCSpan(TCSpan const &obj)
    : DMEMB(category), DMEMB(length) {}
  TCSpan& operator=(TCSpan const &obj)
    { CMEMB(category); CMEMB(length); return *this; }
};


// Text category info for an entire line.
class LineCategories : private ArrayStack<TCSpan> {
  friend class LineCategoryIter;

public:      // data
  // Category of the characters beyond the last entry.
  TextCategory endCategory;

public:
  LineCategories(TextCategory end) : endCategory(end) {}

  // clear existing runs
  void clear(TextCategory end)
    { empty(); endCategory=end; }

  // Add a new category run to those already present.
  void append(TextCategory category, int length);

  // Overwrite a subsequence of characters with a given category;
  // 'length' can be 0 to mean infinite.
  void overlay(int start, int length, TextCategory category);

  // Retrieve the category for the given 0-indexed character.
  TextCategory getCategoryAt(int index) const;

  // debugging: render the runs as a string
  string asString() const;          // e.g. "[1,4][2,3][4"
  string asUnaryString() const;     // e.g. "11112224..."
};


// iterator for walking over LineCategory descriptors
class LineCategoryIter {
private:
  LineCategories const &categories;    // array of categories
  int entry;                           // which entry of 'categories' we're on

public:
  int length;                // how many chars remain on this run (0=infinite)
  TextCategory category;     // category of the current run

public:
  LineCategoryIter(LineCategories const &s);

  // advance the iterator a certain number of characters
  void advanceChars(int n);

  // advance to the next run
  void nextRun();

  // true if we're on the last, infinite run
  bool atEnd() const { return length==0; }
};


#endif // TEXTCATEGORY_H
