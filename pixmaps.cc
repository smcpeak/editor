// pixmaps.cc
// code for pixmaps.h

#include "pixmaps.h"                   // this module

#include "pix/connections-icon.xpm"    // connectionsIcon_xpm
#include "pix/down-arrow.xpm"          // downArrow_xpm
#include "pix/icon.xpm"                // icon_xpm[]
#include "pix/search.xpm"              // search_xpm[]
#include "pix/getreplace.xpm"          // getreplace_xpm[]
#include "pix/replace.xpm"             // replace_xpm[]

// Random note on XPM: XPM is C syntax, but to make it legal C++,
// I had to add 'const' to the second lines of the files.  Concerned
// that might break some programs' ability to read them, I looked at
// this parser in ImageMagick:
//
// https://github.com/ImageMagick/ImageMagick/blob/master/coders/xpm.c
//
// That code (around line 330, near "Remove comments") skips everything
// until it finds a double-quote character followed by four numbers,
// so it will skip over my 'const' without noticing it.  The comments
// in xpm.c make me think it is based on the primordial implementation
// in X, so it is likely that other XPM parsers are also based on it,
// and they should behave similarly.  Therefore, I conclude that adding
// 'const' is safe.
//
// FWIW, I also checked with GIMP, and it can still read the files.

Pixmaps *g_editorPixmaps = NULL;


#define INIT(name) \
  name((char const**)name##_xpm)

Pixmaps::Pixmaps()
  : INIT(icon),
    INIT(search),
    INIT(getReplace),
    INIT(replace),
    INIT(connectionsIcon),
    INIT(downArrow)
{
  if (!g_editorPixmaps) {
    g_editorPixmaps = this;
  }
}

#undef INIT


Pixmaps::~Pixmaps()
{
  if (g_editorPixmaps == this) {
    g_editorPixmaps = NULL;
  }
}


// EOF
