// textline.h
// a single line of text

#ifndef TEXTLINE_H
#define TEXTLINE_H

#include "sm-macros.h"    // NOTEQUAL_OPERATOR

// a single line of text
// note: Buffer manipulates (zeroes and copies) arrays of TextLines by
// directly manipulating the representation, via memset and memcpy
class TextLine {
private:   // data
  char *text;         // (owner) text in this line; NULL if no text;
                      // 'text' is *not* null-terminated
  int length;         // # of chars in this line
  int allocated;      // # of bytes allocated to 'text'

  // invariants:
  //   length <= allocated
  //   the memory pointed to by 'text' is exclusively pointed
  //     by 'text', and has size 'allocated'

private:   // disallowed
  // for now, no implicit copying
  TextLine(TextLine&);
  void operator = (TextLine&);

public:    // funcs
  // empty ctor, no dtor, so we can make arrays w/o C++ stuff going on
  // (NOTE: that means you have to call init and dealloc explicitly)
  TextLine() {}

  void init();        // set all to 0
  void dealloc();     // dealloc 'text' if not NULL

  // two text lines are equal if they have the same
  // length and the same characters in 0 thru length-1
  bool operator == (TextLine const &obj) const;
  NOTEQUAL_OPERATOR(TextLine)    // defines !=

  // accessors
  char const *getText() const { return text; }
  int getLength() const { return length; }

  // don't access this if you can at all avoid it!
  int _please_getAllocated() const { return allocated; }

  // set the entire text contents; will not allocate any
  // margin space
  void setText(char const *src, int srcLen);

  // set the length, and realloc if necessary; if 'margin'
  // is true, make 'allocated' a bit bigger in expectation
  // of more data being added; if new length > old length,
  // the gap is filled with spaces
  void setLength(int newLength, bool margin);
  void setLengthMargin(int n) { setLength(n, true); }
  void setLengthNoMargin(int n) { setLength(n, false); }

  // insert some text; first char of 'insText' will become
  // the character at (0-based) index 'startPos', and the
  // existing text will be shifted over; margin space will
  // be allocated
  void insert(int startPos, char const *insText, int insLength);

  // remove some text
  void remove(int startPos, int delLength);
};

#endif // TEXTLINE_H
