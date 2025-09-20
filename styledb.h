// styledb.h
// database of text styles, indexed by 'enum TextCategory' from style.h

#ifndef EDITOR_STYLEDB_H
#define EDITOR_STYLEDB_H

#include "styledb-fwd.h"               // fwds for this module

#include "textcategory.h"              // TextCategoryAOA, NUM_TEXT_OVERLAY_ATTRIBUTES

#include "smqtutil/qtbdffont-fwd.h"    // QtBDFFont [n]

#include "smbase/array.h"              // GrowArray, ObjArrayStack
#include "smbase/bdffont-fwd.h"        // BDFFont [n]
#include "smbase/sm-macros.h"          // DMEMB

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
  // Styles for use with each overlay.
  ArrayStack<TextStyle> m_styles[NUM_TEXT_OVERLAY_ATTRIBUTES];

public:      // methods
  StyleDB();        // create default styles
  ~StyleDB();

  TextStyle const &getStyle(TextCategoryAOA index) const;

  static StyleDB *instance();
};


// Collection of `QtBDFFont`s for various purposes within the
// `EditorWidget`.
//
// TODO: Move into its own module.
class EditorFontSet {
private:     // data
  // Map from overlay attribute to:
  //   map from text category to:
  //     non-null font owner pointer
  ObjArrayStack<QtBDFFont> m_fontMap[NUM_TEXT_OVERLAY_ATTRIBUTES];

public:      // methods
  ~EditorFontSet();

  // Build an empty set of fonts.  This cannot be used with `at`; it is
  // a placeholder to be swapped with another set.
  EditorFontSet();

  // Build the set of fonts.
  explicit EditorFontSet(
    StyleDB const *styleDB,
    ObjArrayStack<BDFFont> const &bdfFonts);

  // Look up the font for `catAOA`.  Requires that it be mapped.
  //
  // Ensures: return != nullptr
  QtBDFFont const *atC(TextCategoryAOA catAOA) const;
  QtBDFFont *at(TextCategoryAOA catAOA);

  void swapWith(EditorFontSet &obj);

  // Deallocate all font objects.
  void deleteAll();
};


// A `TextCategory` along with some implied style details and the
// ability to push those into a `QPainter`.
class TextCategoryAndStyle {
public:      // data
  // ---- Read-only references, used when changing styles ----
  // Style DB to get details from.
  StyleDB const *m_styleDB;

  // Map from categoryAOA to font.
  EditorFontSet const &m_fontForCategory;

  // When choosing the background color, adjust it to be slightly darker
  // than what `m_textStyle` indicates.
  bool m_useDarkerBackground;

  // ---- Current category and style ----
  // The current category+overlay.  Tracking this makes it possible to
  // avoid changing the `QPainter` if the old and new styles are the
  // same.
  TextCategoryAOA m_textCategoryAOA;

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
    EditorFontSet const &fontForCategory,
    TextCategoryAOA textCategoryAOA,
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
  void setDrawStyleIfNewCategory(QPainter &paint, TextCategoryAOA tco);
};


#endif // EDITOR_STYLEDB_H
