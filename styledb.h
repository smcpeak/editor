// styledb.h
// database of text styles, indexed by 'enum Style' from style.h

#ifndef STYLEDB_H
#define STYLEDB_H

#include "style.h"        // enum Style
#include "macros.h"       // DMEMB
#include "array.h"        // GrowArray

#include <qcolor.h>       // QColor


// variations on the base font
enum FontVariant {
  FV_NORMAL,              // nothing different
  FV_ITALIC,              // italic (slanted)
  FV_BOLD,                // bold
  FV_UNDERLINE,           // underline, achieved by overstriking underscores
};


// font and colors; things named by 'Style'
class TextStyle {
public:
  FontVariant variant;    // text font
  QColor foreground;      // color for text
  QColor background;      // color behind text

public:
  TextStyle() : variant(FV_NORMAL), foreground(), background() {}
  TextStyle(FontVariant v, QColor const &f, QColor const &b)
    : variant(v), foreground(f), background(b) {}
  TextStyle(TextStyle const &obj)
    : DMEMB(variant), DMEMB(foreground), DMEMB(background) {}
};


// collection of styles
class StyleDB {
private:
  // array of defined styles
  ArrayStack<TextStyle> arr;

  // pointer to singleton instance
  static StyleDB *inst;

public:
  StyleDB();        // create default styles
  ~StyleDB();

  TextStyle const &getStyle(Style index) const;

  static StyleDB *instance();
};


#endif // STYLEDB_H
