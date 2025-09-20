// textcategory.h
// data structures to represent descriptions of character styles

#ifndef TEXTCATEGORY_H
#define TEXTCATEGORY_H

#include "textcategory-fwd.h"          // fwds for this module

#include "byte-or-column-count.h"      // ByteOrColumnCount
#include "byte-or-column-index.h"      // ByteOrColumnIndex
#include "rle-inf-sequence.h"          // RLEInfiniteSequence

#include "smbase/array.h"              // ArrayStack
#include "smbase/compare-util-iface.h" // DECLARE_COMPARETO_AND_DEFINE_RELATIONALS
#include "smbase/sm-macros.h"          // DMEMB, CMEMB
#include "smbase/str.h"                // string


// Standard categories; I envision being able to add more dynamically,
// but to have this set always available by default.
enum TextCategory : unsigned char {
  TC_ZERO=0,               // not used; 0 is used to signal EOL during lexing

  // General text editor categories.
  TC_NORMAL,               // normal text

  // These slots are currently unused.  I do not collapse them because I
  // have tests that use the numeric values of the later enumerators.
  TC_unused1,
  TC_unused2,

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

// Required for it to fit into 5 bits in `TextCategoryAOA`.
static_assert(NUM_STANDARD_TEXT_CATEGORIES <= 32);


// Overlay attributes.  At most one can be applied to a given character
// cell.
enum TextOverlayAttribute : unsigned char {
  TOA_NONE,                // No overlay.
  TOA_SELECTION,           // Text is selected.
  TOA_SEARCH_HIT,          // Text is part of a search hit.
  TOA_PREPROCESSOR,        // Text is part of a preprocessor directive.

  NUM_TEXT_OVERLAY_ATTRIBUTES
};

// Required for it to fit into 2 bits in `TextCategoryAOA`.
static_assert(NUM_TEXT_OVERLAY_ATTRIBUTES <= 4);


// Iterate over the elements of `TextOverlayAttribute`.
#define FOR_EACH_TEXT_OVERLAY_ATTRIBUTE(var) \
  for (TextOverlayAttribute var = TOA_NONE;  \
       var < NUM_TEXT_OVERLAY_ATTRIBUTES;    \
       var = TextOverlayAttribute(var + 1))


// A text category And an Overlay Attribute (AOA).
class TextCategoryAOA {
private:     // data
  // The category.
  //
  // Invariant: m_category < NUM_STANDARD_TEXT_CATEGORIES
  unsigned char m_category : 5;

  // The overlay.
  //
  // Invariant: m_overlay < NUM_TEXT_OVERLAY_ATTRIBUTES
  unsigned char m_overlay : 2;

public:
  // TC_NORMAL, TOA_NONE
  TextCategoryAOA();

  // TOA_NONE
  TextCategoryAOA(TextCategory category);

  TextCategoryAOA(TextCategory category, TextOverlayAttribute overlay);

  TextCategoryAOA(TextCategoryAOA const &obj) = default;
  TextCategoryAOA &operator=(TextCategoryAOA const &obj) = default;

  // Assert invariants.
  void selfCheck() const;

  TextCategory category() const;
  TextOverlayAttribute overlay() const;

  // Lexicographic order: category, style.
  DECLARE_COMPARETO_AND_DEFINE_RELATIONALS(TextCategoryAOA)

  // Return a single-letter code that represents `m_category`.
  char categoryLetter() const;

  // Return the category as a number.
  int categoryNumber() const;

  // Return a single-letter code that represents `m_overlay`.
  char overlayLetter() const;

  // Return `*this` except with `m_overlay` set to `overlay`.
  TextCategoryAOA withOverlay(TextOverlayAttribute overlay) const;

  // Write `categoryLetter()`, followed by `overlayLetter()` unless the
  // overlay value is `TOA_NONE`.
  void write(std::ostream &os) const;
  friend std::ostream &operator<<(std::ostream &os, TextCategoryAOA const &obj)
    { obj.write(os); return os; }
};

static_assert(sizeof(TextCategoryAOA) == 1);


// Text category info for an entire line.
//
// This class can be used with either model or layout coordinates,
// depending on the context.
class LineCategoryAOAs : public RLEInfiniteSequence<TextCategoryAOA> {
private:     // types
  using Base = RLEInfiniteSequence<TextCategoryAOA>;

public:      // methods
  using Base::Base;

  // Adjust a subsequence of characters or columns with a given overlay;
  // 'length' can be 0 to mean infinite.
  void overlay(
    ByteOrColumnIndex start,
    ByteOrColumnCount length,
    TextOverlayAttribute overlay);

  // Overwrite [start,start+length) with `newValue`.
  //
  // This is only used by test code.
  void overwrite(
    ByteOrColumnIndex start,
    ByteOrColumnCount length,
    TextCategoryAOA newValue);

  // Retrieve the category+overlay for the given 0-indexed character.
  TextCategoryAOA getCategoryAOAAt(ByteOrColumnIndex index) const;
};


using LineCategoryAOAIter = LineCategoryAOAs::Iter;


#endif // TEXTCATEGORY_H
