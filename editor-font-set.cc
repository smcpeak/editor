// editor-font-set.cc
// Code for `editor-font-set` module.

#include "editor-font-set.h"           // this module

#include "styledb.h"                   // StyleDB, FV_BOLD

#include "smqtutil/qtbdffont.h"        // QtBDFFont

#include "smbase/chained-cond.h"       // smbase::cc::z_le_lt

using namespace smbase;


EditorFontSet::~EditorFontSet()
{
  // Just to be explicit; this would happen anyway.
  deleteAll();
}


EditorFontSet::EditorFontSet()
{}


EditorFontSet::EditorFontSet(
  StyleDB const *styleDB,
  ObjArrayStack<BDFFont> const &bdfFonts)
{
  FOR_EACH_TEXT_OVERLAY_ATTRIBUTE(overlay) {
    ObjArrayStack<QtBDFFont> &newFonts = m_fontMap[int(overlay)];

    for (int category = TC_ZERO; category < NUM_STANDARD_TEXT_CATEGORIES; category++) {
      TextStyle const &ts = styleDB->getStyle(
        TextCategoryAOA(TextCategory(category), overlay));

      STATIC_ASSERT(FV_BOLD == 2);
      BDFFont const *bdfFont = bdfFonts[ts.variant % 3];

      QtBDFFont *qfont = new QtBDFFont(*bdfFont);
      qfont->setFgColor(ts.foreground);
      qfont->setBgColor(ts.background);
      qfont->setTransparent(false);
      newFonts.push(qfont);
    }
  }
}


QtBDFFont const *EditorFontSet::atC(TextCategoryAOA catAOA) const
{
  int ovlIndex = int(catAOA.overlay());
  xassert(cc::z_le_lt(ovlIndex, TABLESIZE(m_fontMap)));

  int catIndex = int(catAOA.category());

  QtBDFFont const *ret = m_fontMap[ovlIndex][catIndex];
  xassert(ret);

  return ret;
}


QtBDFFont *EditorFontSet::at(TextCategoryAOA catAOA)
{
  return const_cast<QtBDFFont*>(atC(catAOA));
}


void EditorFontSet::swapWith(EditorFontSet &obj)
{
  FOR_EACH_TEXT_OVERLAY_ATTRIBUTE(overlay) {
    m_fontMap[int(overlay)].swapWith(obj.m_fontMap[int(overlay)]);
  }
}


void EditorFontSet::deleteAll()
{
  for (ObjArrayStack<QtBDFFont> &fontMap : m_fontMap) {
    fontMap.deleteAll();
  }
}


// EOF
