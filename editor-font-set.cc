// editor-font-set.cc
// Code for `editor-font-set` module.

#include "editor-font-set.h"           // this module

#include "styledb.h"                   // StyleDB, FV_BOLD

#include "smqtutil/qtbdffont.h"        // QtBDFFont

#include "smbase/chained-cond.h"       // smbase::cc::z_le_lt
#include "smbase/xassert.h"            // xassertPtr

#include <utility>                     // std::swap

using namespace smbase;


EditorFontSet::~EditorFontSet()
{
  // Just to be explicit; this would happen anyway.
  deleteAll();
}


EditorFontSet::EditorFontSet()
:
  m_cursorFontForFV(),
  m_minihexFont()
{}


EditorFontSet::EditorFontSet(
  StyleDB const *styleDB,
  ObjArrayStack<BDFFont> const &primaryBDFFonts,
  BDFFont const &minihexBDFFont,
  QColor cursorColor)
{
  xassertPrecondition(primaryBDFFonts.length() == FV_BOLD + 1);

  // Make the main fonts.
  FOR_EACH_TEXT_OVERLAY_ATTRIBUTE(overlay) {
    ObjArrayStack<QtBDFFont> &newFonts = m_fontMap.at(overlay);

    for (int category = TC_ZERO; category < NUM_STANDARD_TEXT_CATEGORIES; category++) {
      TextStyle const &ts = styleDB->getStyle(
        TextCategoryAOA(TextCategory(category), overlay));

      STATIC_ASSERT(FV_BOLD == 2);
      BDFFont const *bdfFont = primaryBDFFonts[ts.variant % 3];

      QtBDFFont *qfont = new QtBDFFont(*bdfFont);
      qfont->setFgColor(ts.foreground);
      qfont->setBgColor(ts.background);
      qfont->setTransparent(false);
      newFonts.push(qfont);
    }
  }

  // Similar procedure for the cursor fonts.
  for (int fv = 0; fv <= FV_BOLD; fv++) {
    QtBDFFont *qfont = new QtBDFFont(*(primaryBDFFonts[fv]));

    // The character under the cursor is drawn with the normal background
    // color, and the cursor box (its background) is drawn in 'cursorColor'.
    qfont->setFgColor(styleDB->getStyle(TC_NORMAL).background);
    qfont->setBgColor(cursorColor);
    qfont->setTransparent(false);

    m_cursorFontForFV.push(qfont);
  }

  // Font for missing glyphs.
  m_minihexFont.reset(new QtBDFFont(minihexBDFFont));
  m_minihexFont->setTransparent(false);
}


void EditorFontSet::selfCheck() const
{
  for (auto const &fonts : m_fontMap) {
    for (int i=0; i < fonts.length(); ++i) {
      xassertPtr(fonts[i])->selfCheck();
    }
  }

  for (int i=0; i < m_cursorFontForFV.length(); ++i) {
    xassertPtr(m_cursorFontForFV[i])->selfCheck();
  }

  xassertPtr(m_minihexFont.get())->selfCheck();
}


QtBDFFont const *EditorFontSet::forCatAOAC(TextCategoryAOA catAOA) const
{
  QtBDFFont const *ret =
    m_fontMap.at(catAOA.overlay())[catAOA.category()];

  return xassertPtr(ret);
}


QtBDFFont *EditorFontSet::forCatAOA(TextCategoryAOA catAOA)
{
  return const_cast<QtBDFFont*>(forCatAOAC(catAOA));
}


QtBDFFont *EditorFontSet::forCursorForFV(FontVariant fv)
{
  xassertPrecondition(cc::z_le_le(fv, FV_BOLD));

  return xassertPtr(m_cursorFontForFV[fv]);
}


QtBDFFont *EditorFontSet::minihex()
{
  return xassertPtr(m_minihexFont.get());
}


void EditorFontSet::swapWith(EditorFontSet &obj)
{
  FOR_EACH_TEXT_OVERLAY_ATTRIBUTE(overlay) {
    m_fontMap.at(overlay).swapWith(obj.m_fontMap.at(overlay));
  }

  m_cursorFontForFV.swapWith(obj.m_cursorFontForFV);

  using std::swap;
  swap(m_minihexFont, obj.m_minihexFont);
}


void EditorFontSet::deleteAll()
{
  for (ObjArrayStack<QtBDFFont> &fontMap : m_fontMap) {
    fontMap.deleteAll();
  }

  m_cursorFontForFV.deleteAll();

  m_minihexFont.reset();
}


// EOF
