// pixmaps.cc
// code for pixmaps.h

#include "pixmaps.h"             // this module

#include "pix/icon.xpm"          // icon_xpm[]
#include "pix/search.xpm"        // search_xpm[]
#include "pix/getreplace.xpm"    // getreplace_xpm[]
#include "pix/replace.xpm"       // replace_xpm[]

Pixmaps *pixmaps = NULL;


#define INIT(name) \
  name((char const**)name##_xpm)

Pixmaps::Pixmaps()
  : INIT(icon),
    INIT(search),
    INIT(getReplace),
    INIT(replace)
{
  if (!pixmaps) {
    pixmaps = this;
  }
}

#undef INIT


Pixmaps::~Pixmaps()
{
  if (pixmaps == this) {
    pixmaps = NULL;
  }
}


// EOF
