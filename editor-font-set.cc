// editor-font-set.cc
// Code for `editor-font-set` module.

#include "editor-font-set.h"           // this module

#include "styledb.h"                   // StyleDB, FV_BOLD

#include "smqtutil/qtbdffont.h"        // QtBDFFont

#include "smbase/chained-cond.h"       // smbase::cc::z_le_lt
#include "smbase/sm-macros.h"          // SWAP_MEMB
#include "smbase/xassert.h"            // xassertPtr

#include <cstddef>                     // std::size_t
#include <utility>                     // std::{move,swap}

using namespace smbase;


EditorFontSet::~EditorFontSet()
{
  // Just to be explicit; this would happen anyway.
  deleteAll();
}


EditorFontSet::EditorFontSet()
:
  m_isEmpty(true),
  m_fontMap(),
  m_cursorFontForFV(),
  m_minihexFont()
{
  selfCheck();
}


EditorFontSet::EditorFontSet(
  StyleDB const *styleDB,
  FontVariantToBDFFont const &primaryBDFFonts,
  BDFFont const &minihexBDFFont,
  QColor cursorColor)
:
  m_isEmpty(false),
  m_fontMap(),
  m_cursorFontForFV(),
  m_minihexFont()
{
  // Make the main fonts.
  FOR_EACH_TEXT_OVERLAY_ATTRIBUTE(overlay) {
    CategoryToFont &newFonts = m_fontMap.at(overlay);

    for (int category = TC_ZERO; category < NUM_STANDARD_TEXT_CATEGORIES; category++) {
      TextStyle const &ts = styleDB->getStyle(
        TextCategoryAOA(TextCategory(category), overlay));

      STATIC_ASSERT(FV_BOLD == 2);
      BDFFont const *bdfFont = primaryBDFFonts.at(ts.variant % 3).get();

      QtBDFFont_uptr qfont(new QtBDFFont(*bdfFont));
      qfont->setFgColor(ts.foreground);
      qfont->setBgColor(ts.background);
      qfont->setTransparent(false);
      newFonts.at(category) = std::move(qfont);
    }
  }

  // Similar procedure for the cursor fonts.
  for (int fv = 0; fv <= FV_BOLD; fv++) {
    QtBDFFont_uptr qfont(
      new QtBDFFont(*(primaryBDFFonts[fv])));

    // The character under the cursor is drawn with the normal background
    // color, and the cursor box (its background) is drawn in 'cursorColor'.
    qfont->setFgColor(styleDB->getStyle(TC_NORMAL).background);
    qfont->setBgColor(cursorColor);
    qfont->setTransparent(false);

    m_cursorFontForFV.at(fv) = std::move(qfont);
  }

  // Font for missing glyphs.
  m_minihexFont.reset(new QtBDFFont(minihexBDFFont));
  m_minihexFont->setTransparent(false);

  selfCheck();
}


void EditorFontSet::selfCheck() const
{
  if (m_isEmpty) {
    // The empty object does not have important invariants.
    return;
  }

  for (CategoryToFont const &catToFont : m_fontMap) {
    for (QtBDFFont_uptr const &ptr : catToFont) {
      xassertPtr(ptr.get())->selfCheck();
    }
  }

  for (QtBDFFont_uptr const &ptr : m_cursorFontForFV) {
    xassertPtr(ptr.get())->selfCheck();
  }

  xassertPtr(m_minihexFont.get())->selfCheck();
}


QtBDFFont const *EditorFontSet::forCatAOAC(TextCategoryAOA catAOA) const
{
  QtBDFFont const *ret =
    m_fontMap.at(catAOA.overlay()).at(catAOA.category()).get();

  return xassertPtr(ret);
}


QtBDFFont *EditorFontSet::forCatAOA(TextCategoryAOA catAOA)
{
  return const_cast<QtBDFFont*>(forCatAOAC(catAOA));
}


QtBDFFont *EditorFontSet::forCursorForFV(FontVariant fv)
{
  xassertPrecondition(cc::z_le_le(fv, FV_BOLD));

  return xassertPtr(m_cursorFontForFV.at(fv).get());
}


QtBDFFont *EditorFontSet::minihex()
{
  return xassertPtr(m_minihexFont.get());
}


void EditorFontSet::swapWith(EditorFontSet &obj)
{
  using std::swap;

  SWAP_MEMB(m_isEmpty);
  SWAP_MEMB(m_fontMap);
  SWAP_MEMB(m_cursorFontForFV);
  SWAP_MEMB(m_minihexFont);

  selfCheck();
}


void EditorFontSet::deleteAll()
{
  for (CategoryToFont &catToFont : m_fontMap) {
    for (QtBDFFont_uptr &ptr : catToFont) {
      ptr.reset();
    }
  }

  for (QtBDFFont_uptr &ptr : m_cursorFontForFV) {
    ptr.reset();
  }

  m_minihexFont.reset();

  m_isEmpty = true;
}


// EOF
