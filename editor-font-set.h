// editor-font-set.h
// `EditorFontSet`, a collection of fonts for use by `EditorWidget`.

// See license.txt for copyright and terms of use.

#ifndef EDITOR_EDITOR_FONT_SET_H
#define EDITOR_EDITOR_FONT_SET_H

#include "editor-font-set-fwd.h"       // fwds for this module

#include "font-variant.h"              // FontVariant, FV_BOLD
#include "styledb-fwd.h"               // StyleDB
#include "textcategory.h"              // TextCategoryAOA, NUM_TEXT_OVERLAY_ATTRIBUTES

#include "smqtutil/qtbdffont-fwd.h"    // QtBDFFont [n]

#include "smbase/array.h"              // ObjArrayStack
#include "smbase/bdffont-fwd.h"        // BDFFont [n]

#include <array>                       // std::array
#include <memory>                      // std::unique_ptr

class QColor;


// Collection of `QtBDFFont`s for various purposes within
// `EditorWidget`.
class EditorFontSet {
private:     // types
  using QtBDFFont_uptr =
    std::unique_ptr<QtBDFFont>;

  using CategoryToFont =
    std::array<QtBDFFont_uptr,
               NUM_STANDARD_TEXT_CATEGORIES>;

  using OverlayToCategoryToFont =
    std::array<CategoryToFont,
               NUM_TEXT_OVERLAY_ATTRIBUTES>;

private:     // data
  // True of the empty placeholder object.
  bool m_isEmpty;

  // Map from overlay attribute to:
  //   map from text category to:
  //     non-null font owner pointer
  OverlayToCategoryToFont m_fontMap;

  // Font for drawing the character under the cursor, indexed by
  // the FontVariant (modulo FV_UNDERLINE) there.
  //
  // Invariant: Unless `m_isEmpty`, all elements are non-null.
  std::array<QtBDFFont_uptr, FV_BOLD+1> m_cursorFontForFV;

  // Font containing miniature hexadecimal characters for use when
  // a glyph is missing.  Unless `m_isEmpty`, never null.
  QtBDFFont_uptr m_minihexFont;

public:      // methods
  ~EditorFontSet();

  // Build an empty set of fonts.  This cannot be used with `at`; it is
  // a placeholder to be swapped with another set.
  EditorFontSet();

  // Build the set of fonts.
  explicit EditorFontSet(
    StyleDB const *styleDB,
    ObjArrayStack<BDFFont> const &primaryBDFFonts,
    BDFFont const &minihexBDFFont,
    QColor cursorColor);

  // Assert invariants.
  void selfCheck() const;

  // Look up the font for `catAOA`.  Requires that it be mapped.
  //
  // Ensures: return != nullptr
  QtBDFFont const *forCatAOAC(TextCategoryAOA catAOA) const;
  QtBDFFont *forCatAOA(TextCategoryAOA catAOA);

  // Get the font to use when the cursor is over `fv`.
  //
  // Requires: 0 <= fv <= FV_BOLD
  //
  // Ensures: return != nullptr
  QtBDFFont *forCursorForFV(FontVariant fv);

  // Get the minihex font for drawing characters without glyphs.
  //
  // Ensures: return != nullptr
  QtBDFFont *minihex();

  void swapWith(EditorFontSet &obj);

  // Deallocate all font objects.
  void deleteAll();
};


#endif // EDITOR_EDITOR_FONT_SET_H
