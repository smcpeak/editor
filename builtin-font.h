// builtin-font.h
// BuiltinFont enumeration.

#ifndef BUILTIN_FONT_H
#define BUILTIN_FONT_H

// smbase
#include "sm-macros.h"                 // FOR_EACH_ENUM_ELEMENT


// Enumeration of the bitmap fonts that are built in to the executable.
enum BuiltinFont {
  BF_EDITOR14,     // My 14 pixel courier derivative.
  BF_COURIER24,    // X11 24 pixel 75 DPIcourier.

  NUM_BUILTIN_FONTS
};

#define FOR_EACH_BUILTIN_FONT(iter) \
  FOR_EACH_ENUM_ELEMENT(BuiltinFont, NUM_BUILTIN_FONTS, iter)

// Return a name suitable for use in a dropdown that allows the user to
// choose among the fonts.
char const *builtinFontName(BuiltinFont bfont);

#endif // BUILTIN_FONT_H
