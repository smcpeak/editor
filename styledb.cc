// styledb.cc
// code for styledb.h

#include "styledb.h"     // this module

StyleDB::StyleDB()
  : arr(ST_PREPROCESSOR+1)    // initial size
{
  // These colors are the ones I like for C++ code.  They're vaguely
  // based on the Turbo C++ IDE default color scheme.  I'd like to
  // distribute several default color schemes, instead of making mine
  // the only default.

  QColor bg(0x00, 0x00, 0xA0);        // background: darkish blue
  QColor selectBG(0x00, 0x00, 0xF0);  // selection background: light blue
  QColor hitBG(0x00, 0xE0, 0x50);     // hit background: lime

  #define ENTRY(index, variant, fr, fg, fb, back) \
    xassert(arr.length() == index);               \
    arr.push(TextStyle(variant, QColor(fr, fg, fb), back)) /* user ; */

  ENTRY(ST_ZERO,         FV_NORMAL,  0xFF, 0xFF, 0xFF,  bg);  // not used

  ENTRY(ST_NORMAL,       FV_NORMAL,  0xFF, 0xFF, 0xFF,  bg);
  ENTRY(ST_SELECTION,    FV_NORMAL,  0xFF, 0xFF, 0xFF,  selectBG);
  ENTRY(ST_HITS,         FV_NORMAL,  0xFF, 0xFF, 0xFF,  hitBG);
  ENTRY(ST_COMMENT,      FV_ITALIC,  0xB0, 0xC0, 0xA0,  bg);
  ENTRY(ST_STRING,       FV_NORMAL,  0xFF, 0xFF, 0x00,  bg);
  ENTRY(ST_KEYWORD,      FV_NORMAL,  0x60, 0xFF, 0x70,  bg);
  ENTRY(ST_NUMBER,       FV_BOLD,    0xFF, 0x80, 0x00,  bg);
  ENTRY(ST_NUMBER2,      FV_BOLD,    0xFF, 0x40, 0x00,  bg);
  ENTRY(ST_OPERATOR,     FV_BOLD,    0xE0, 0x00, 0xB0,  bg);
  ENTRY(ST_PREPROCESSOR, FV_NORMAL,  0xFF, 0x80, 0xFF,  bg);

  #undef ENTRY
}


StyleDB::~StyleDB()
{}


TextStyle const &StyleDB::getStyle(Style index) const
{
  return arr[index];    // does bounds check
}

              
StyleDB *StyleDB::inst = NULL;

STATICDEF StyleDB *StyleDB::instance()
{
  if (!inst) {
    inst = new StyleDB();
  }
  return inst;
}
