// textline.cc
// code for textline.h

#include "textline.h"     // this module
#include "ckheap.h"       // checkHeap
#include "xassert.h"      // xassert

#include <string.h>       // memmove, etc.


void TextLine::init()
{
  text = NULL;
  length = 0;
  allocated = 0;
}

void TextLine::dealloc()
{
  if (text) {
    delete[] text;
  }
}


bool TextLine::operator == (TextLine const &obj) const
{
  return length == obj.length &&
         0==memcmp(text, obj.text, length);
}


void TextLine::setText(char const *src, int len)
{
  if (allocated != len) {
    // realloc
    if (text) {
      delete[] text;
    }
    if (len != 0) {
      text = new char[len];
    }
    allocated = len;
  }

  // copy text
  memcpy(text, src, len);
  length = len;
}


// adjustable parameters; don't surround in parens
// because the ratios are supposed to left-mulitply
// before they divide ..
#define LINE_SHRINK_RATIO 1 / 3
#define LINE_GROW_RATIO 6 / 5
#define LINE_GROW_STEP 20


void TextLine::setLength(int newLength, bool margin)
{
  // decide how big the final buffer will be
  int newAllocated = allocated;
  if (!margin) {       // no margin
    newAllocated = newLength;
  }
  else {               // reasonable margin
    if (allocated < newLength  ||                                       // too small
        newLength < (allocated * LINE_SHRINK_RATIO) - LINE_GROW_STEP) { // too big
      // new space will be 20% larger, plus 20
      newAllocated = newLength * LINE_GROW_RATIO + LINE_GROW_STEP;
    }
  }

  // realloc & copy
  if (newAllocated != allocated) {
    char *newText = new char[newAllocated];

    // copy over the common prefix
    int preserved = length<newLength? length : newLength;
    memcpy(newText, text, preserved);

    // dealloc old
    if (text) {
      delete[] text;
    }

    // reassign to new
    text = newText;
    allocated = newAllocated;
  }

  // somewhat editor-centric: if the new length is bigger than
  // the old length, then we will init the gap with spaces
  // (could make the fill char a parameter, but until I need
  // something other than spaces, I will leave it like this)
  if (newLength > length) {
    memset(text+length, ' ', newLength-length);
  }

  // set length
  length = newLength;
}


void TextLine::insert(int startPos, char const *insText, int insLength)
{
  if (startPos >= length) {
    // start by expanding it to the full length
    setLengthMargin(startPos);
  }

  // slightly inefficient because we may copy some text twice
  int oldLength = length;
  setLengthMargin(length + insLength);

  // move right: the text to the right of the insertion point
  memmove(text+startPos+insLength,     // dest
          text+startPos,               // src
          oldLength-startPos);         // how much to move

  // copy in the new text
  memcpy(text+startPos, insText, insLength);

  // new length is already set by setLengthMargin, above!
}


void TextLine::remove(int startPos, int delLength)
{
  if (startPos >= length) {
    // nothing out here..
    return;
  }

  if (startPos + delLength >= length) {
    // trim the deletion length so we are only removing what's there
    delLength = length - startPos;
  }

  // move left: the text to the right of the insertion point
  memmove(text+startPos,                      // dest
          text+startPos+delLength,            // src
          length-(startPos+delLength));    // how much to move

  // slightly inefficient because we may copy some text twice
  setLengthMargin(length - delLength);
}
