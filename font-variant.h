// font-variant.h
// `FontVariant` enumeration: normal, italic, bold, underline.

// See license.txt for copyright and terms of use.

#ifndef EDITOR_FONT_VARIANT_H
#define EDITOR_FONT_VARIANT_H

#include "font-variant-fwd.h"          // fwds for this module


enum FontVariant : int {
  FV_NORMAL,              // nothing different
  FV_ITALIC,              // italic (slanted)
  FV_BOLD,                // bold
  FV_UNDERLINE,           // underline, achieved by overstriking underscores
};


#endif // EDITOR_FONT_VARIANT_H
