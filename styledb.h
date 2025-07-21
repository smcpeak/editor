// styledb.h
// database of text styles, indexed by 'enum TextCategory' from style.h

#ifndef EDITOR_STYLEDB_H
#define EDITOR_STYLEDB_H

#include "styledb-fwd.h"               // fwds for this module

#include "textcategory.h"              // TextCategory

#include "smqtutil/qtbdffont-fwd.h"    // QtBDFFont

#include "smbase/sm-macros.h"          // DMEMB
#include "smbase/array.h"              // GrowArray, ObjArrayStack

#include <qcolor.h>                    // QColor

class QPainter;


// variations on the base font
enum FontVariant {
  FV_NORMAL,              // nothing different
  FV_ITALIC,              // italic (slanted)
  FV_BOLD,                // bold
  FV_UNDERLINE,           // underline, achieved by overstriking underscores
};


// font and colors; things named by 'TextCategory'
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
private:     // class data
  // pointer to singleton instance
  static StyleDB *inst;

private:     // instance data
  // array of defined styles
  ArrayStack<TextStyle> arr;

public:      // methods
  StyleDB();        // create default styles
  ~StyleDB();

  TextStyle const &getStyle(TextCategory index) const;

  static StyleDB *instance();
};


// A `TextCategory` along with some implied style details and the
// ability to push those into a `QPainter`.
class TextCategoryAndStyle {
public:      // data
  // ---- Read-only references, used when changing styles ----
  // Style DB to get details from.
  StyleDB const *m_styleDB;

  // Map from category to font.
  ObjArrayStack<QtBDFFont> const &m_fontForCategory;

  // When choosing the background color, adjust it to be slightly darker
  // than what `m_textStyle` indicates.
  bool m_useDarkerBackground;

  // ---- Current category and style ----
  // The current category.  Tracking this makes it possible to avoid
  // changing the `QPainter` if the old and new styles are the same.
  TextCategory m_textCategory;

  // Current style.  Never null.
  TextStyle const *m_textStyle;

  // Font to use for this style.  Never null.
  QtBDFFont *m_font;

private:     // methods
  // Set `m_textStyle` and `m_curFont` based on `m_textCategory`.
  // Ensures neither is null.
  void setStyleDetails();

public:      // methods
  explicit TextCategoryAndStyle(
    ObjArrayStack<QtBDFFont> const &fontForCategory,
    TextCategory textCategory,
    bool useDarkerBackground);

  // True if the current style calls for underlining.
  bool underlining() const
    { return m_textStyle->variant == FV_UNDERLINE; }

  QtBDFFont *getFont() const
    { return m_font; }

  // Push the current style details into `paint`.
  void setDrawStyle(QPainter &paint) const;

  // If `tc != m_textCategory`, update it along with other details and
  // push those into `paint`.
  void setDrawStyleIfNewCategory(QPainter &paint, TextCategory tc);
};


#endif // EDITOR_STYLEDB_H
