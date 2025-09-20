// editor-font-set.h
// `EditorFontSet`, a collection of fonts for use by `EditorWidget`.

// See license.txt for copyright and terms of use.

#ifndef EDITOR_EDITOR_FONT_SET_H
#define EDITOR_EDITOR_FONT_SET_H

#include "editor-font-set-fwd.h"       // fwds for this module

#include "styledb-fwd.h"               // StyleDB [n]
#include "textcategory.h"              // TextCategoryAOA, NUM_TEXT_OVERLAY_ATTRIBUTES

#include "smqtutil/qtbdffont-fwd.h"    // QtBDFFont [n]

#include "smbase/array.h"              // ObjArrayStack
#include "smbase/bdffont-fwd.h"        // BDFFont [n]


// Collection of `QtBDFFont`s for various purposes within
// `EditorWidget`.
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


#endif // EDITOR_EDITOR_FONT_SET_H
