// textline.h
// a single line of text

#ifndef TEXTLINE_H
#define TEXTLINE_H

// a single line of text; bit-zero-initable, and bit-copyable
// if the old copy is destroyed without 'dealloc'
class TextLine {
private:   // data
  char *text;         // (owner) text in this line; NULL if no text;
                      // 'text' is *not* null-terminated
  int length;         // # of chars in this line
  int allocated;      // # of bytes allocated to 'text'

public:
  // no ctor or dtor so we can make arrays w/o C++ stuff going on
  void init();        // set all to 0
  void dealloc();     // dealloc 'text' if not NULL

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
  // of more data being added
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
