// styledb.cc
// code for styledb.h

#include "styledb.h"                   // this module

#include "smqtutil/qtbdffont.h"        // QtBDFFont [n]

#include "smbase/chained-cond.h"       // smbase::cc::z_le_lt

#include <QPainter>

using namespace smbase;


StyleDB::StyleDB()
{
  // Reserve the appropriate sizes in each array.
  FOR_EACH_TEXT_OVERLAY_ATTRIBUTE(oa) {
    m_styles[int(oa)].ensureAtLeast(NUM_STANDARD_TEXT_CATEGORIES);
  }

  // These colors are the ones I like for C++ code.  They're vaguely
  // based on the Turbo C++ IDE default color scheme.  I'd like to
  // distribute several default color schemes, instead of making mine
  // the only default.

  QColor bg(0x00, 0x00, 0x9C);        // background: darkish blue
  QColor selectBG(0x40, 0x40, 0xF0);  // selection background: light blue/purple
  QColor hitBG(0x00, 0x80, 0x40);     // hit background: lime
  QColor errorBG(0x00, 0x00, 0x00);   // error background: black

  QColor diffFileBG(0x00, 0x40, 0x9C);// diff file bg: lighter blue

  // Array of styles with no overlay.
  ArrayStack<TextStyle> &noneArr = m_styles[TOA_NONE];

  // Populate an entry in the "none" overlay.
  #define ENTRY(index, variant, fr, fg, fb, back)                           \
    xassert(noneArr.length() == int(index));                                \
    noneArr.push(TextStyle(variant, QColor(fr, fg, fb), back)) /* user ; */

  ENTRY(TC_ZERO,         FV_NORMAL,  0xFF, 0xFF, 0xFF,  bg);  // not used

  ENTRY(TC_NORMAL,       FV_NORMAL,  0xFF, 0xFF, 0xFF,  bg);
  ENTRY(TC_unused1,      FV_NORMAL,  0xFF, 0xFF, 0xFF,  bg);
  ENTRY(TC_unused2,      FV_NORMAL,  0xFF, 0xFF, 0xFF,  bg);

  ENTRY(TC_ERROR,        FV_NORMAL,  0xFF, 0xFF, 0xFF,  errorBG);

  ENTRY(TC_COMMENT,      FV_ITALIC,  0xB0, 0xC0, 0xA0,  bg);
  ENTRY(TC_STRING,       FV_NORMAL,  0xFF, 0xFF, 0x00,  bg);
  ENTRY(TC_KEYWORD,      FV_NORMAL,  0x60, 0xFF, 0x70,  bg);
  ENTRY(TC_SPECIAL,      FV_BOLD,    0xFF, 0x80, 0x00,  bg);
  ENTRY(TC_NUMBER,       FV_BOLD,    0xFF, 0x80, 0x00,  bg);
  ENTRY(TC_NUMBER2,      FV_BOLD,    0xFF, 0x40, 0x00,  bg);
  ENTRY(TC_OPERATOR,     FV_BOLD,    0xE0, 0x00, 0xB0,  bg);
  ENTRY(TC_PREPROCESSOR, FV_NORMAL,  0xFF, 0x80, 0xFF,  bg);

  ENTRY(TC_DIFF_CONTEXT, FV_NORMAL,  0xB0, 0xC0, 0xA0,  bg);
  ENTRY(TC_DIFF_OLD_FILE,FV_BOLD,    0xFF, 0x40, 0x00,  diffFileBG);
  ENTRY(TC_DIFF_NEW_FILE,FV_BOLD,    0x00, 0xFF, 0x00,  diffFileBG);
  ENTRY(TC_DIFF_SECTION, FV_BOLD,    0x00, 0xFF, 0xFF,  bg);
  ENTRY(TC_DIFF_REMOVAL, FV_NORMAL,   244,  118,  104,  bg);
  ENTRY(TC_DIFF_ADDITION,FV_NORMAL,  0x20, 0xFF, 0x20,  bg);

  #undef ENTRY

  // Fill in the overlay variants.
  FOR_EACH_TEXT_OVERLAY_ATTRIBUTE(overlay) {
    if (overlay == TOA_NONE) {
      // Already did it.
      continue;
    }

    ArrayStack<TextStyle> &overlayArr = m_styles[int(overlay)];

    // It should not have been populated yet.
    xassert(overlayArr.length() == 0);

    for (int i=0; i < noneArr.length(); ++i) {
      // Start with the "none" overlay as a base.
      TextStyle ts = noneArr[i];

      // Modify it per `overlay`.
      //
      // The ability to express overlays this way is one of the main
      // reasons to have the concept, since I can keep all the base
      // style details and only adjust what I want to.
      switch (overlay) {
        case TOA_NONE:
        case NUM_TEXT_OVERLAY_ATTRIBUTES:
          xfailure("not reached");

        case TOA_SELECTION:
          ts.background = selectBG;
          break;

        case TOA_SEARCH_HIT:
          ts.background = hitBG;
          break;

        case TOA_PREPROCESSOR:
          // TODO: Define the appearance.
          break;
      }

      overlayArr.push(ts);
    }
  }
}


StyleDB::~StyleDB()
{}


TextStyle const &StyleDB::getStyle(TextCategoryAOA index) const
{
  int oa = int(index.overlay());
  xassert(cc::z_le_lt(oa, TABLESIZE(m_styles)));

  // The second index operator does its own bounds check.
  return m_styles[oa][index.category()];
}


StyleDB *StyleDB::inst = NULL;

STATICDEF StyleDB *StyleDB::instance()
{
  if (!inst) {
    inst = new StyleDB();
  }
  return inst;
}


// -------------------------- FontForCategory --------------------------
FontForCategory::~FontForCategory()
{
  // Just to be explicit; this would happen anyway.
  deleteAll();
}


FontForCategory::FontForCategory()
{}


FontForCategory::FontForCategory(
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


QtBDFFont const *FontForCategory::atC(TextCategoryAOA catAOA) const
{
  int ovlIndex = int(catAOA.overlay());
  xassert(cc::z_le_lt(ovlIndex, TABLESIZE(m_fontMap)));

  int catIndex = int(catAOA.category());

  QtBDFFont const *ret = m_fontMap[ovlIndex][catIndex];
  xassert(ret);

  return ret;
}


QtBDFFont *FontForCategory::at(TextCategoryAOA catAOA)
{
  return const_cast<QtBDFFont*>(atC(catAOA));
}


void FontForCategory::swapWith(FontForCategory &obj)
{
  FOR_EACH_TEXT_OVERLAY_ATTRIBUTE(overlay) {
    m_fontMap[int(overlay)].swapWith(obj.m_fontMap[int(overlay)]);
  }
}


void FontForCategory::deleteAll()
{
  for (ObjArrayStack<QtBDFFont> &fontMap : m_fontMap) {
    fontMap.deleteAll();
  }
}


// ----------------------- TextCategoryAndStyle ------------------------
void TextCategoryAndStyle::setStyleDetails()
{
  m_textStyle = &( m_styleDB->getStyle(m_textCategoryAOA) );

  // `QtBDFFont` has somewhat screwy constness, requiring a non-const
  // pointer to perform drawing operations.  I've added a TODO in
  // `qtbdffont.h` to fix it, but for now, just bypass it with a cast.
  m_font = const_cast<QtBDFFont*>(
    m_fontForCategory.atC(m_textCategoryAOA));

  xassert(m_textStyle != nullptr);
  xassert(m_font != nullptr);
}


TextCategoryAndStyle::TextCategoryAndStyle(
  FontForCategory const &fontForCategory,
  TextCategoryAOA textCategoryAOA,
  bool useDarkerBackground)
  : m_styleDB(StyleDB::instance()),
    m_fontForCategory(fontForCategory),
    m_useDarkerBackground(useDarkerBackground),
    m_textCategoryAOA(textCategoryAOA),
    m_textStyle(nullptr),
    m_font(nullptr)
{
  setStyleDetails();
}


void TextCategoryAndStyle::setDrawStyle(QPainter &paint) const
{
  // This is needed for underlining since we draw that as a line,
  // whereas otherwise the foreground color comes from the font glyphs.
  paint.setPen(m_textStyle->foreground);

  QColor bg = m_textStyle->background;
  if (m_useDarkerBackground) {
    bg = bg.darker();
  }
  paint.setBackground(bg);
}


void TextCategoryAndStyle::setDrawStyleIfNewCategory(
  QPainter &paint, TextCategoryAOA tco)
{
  // I don't know if this is really a useful optimization, as I've never
  // measured it.  This is just what the code did in the past, so I'm
  // preserving it.

  if (tco != m_textCategoryAOA) {
    m_textCategoryAOA = tco;
    setStyleDetails();
    setDrawStyle(paint);
  }
}


// EOF
