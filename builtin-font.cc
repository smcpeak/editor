// builtin-font.cc
// Code for builtin-font.h.

#include "builtin-font.h"              // this module

// smbase
#include "smbase/sm-macros.h"          // DEFINE_ENUMERATION_TO_STRING


char const *builtinFontName(BuiltinFont bfont)
{
  RETURN_ENUMERATION_STRING(
    BuiltinFont,
    NUM_BUILTIN_FONTS,
    (
      "Editor 14 px",
      "Courier 24 px",
    ),
    bfont
  );
}


// EOF

