// textcategory.cc
// code for textcategory.h

#include "textcategory.h"              // this module

#include "smbase/chained-cond.h"       // smbase::cc::z_le_lt
#include "smbase/compare-util.h"       // smbase::compare, RET_IF_COMPARE_MEMBERS

#include <iostream>                    // std::ostream
#include <optional>                    // std::optional

using namespace smbase;


// -------------------------- TextCategoryAOA --------------------------
TextCategoryAOA::TextCategoryAOA()
:
  m_category(int(TC_NORMAL)),
  m_overlay(int(TextOverlayAttribute::TOA_NONE))
{
  selfCheck();
}


TextCategoryAOA::TextCategoryAOA(TextCategory category)
:
  m_category(int(category)),
  m_overlay(int(TextOverlayAttribute::TOA_NONE))
{
  selfCheck();
}


TextCategoryAOA::TextCategoryAOA(
  TextCategory category,
  TextOverlayAttribute overlay)
:
  m_category(int(category)),
  m_overlay(int(overlay))
{
  selfCheck();
}


void TextCategoryAOA::selfCheck() const
{
  xassert(m_category < int(NUM_STANDARD_TEXT_CATEGORIES));
  xassert(m_overlay < int(TextOverlayAttribute::NUM_TEXT_OVERLAY_ATTRIBUTES));
}


TextCategory TextCategoryAOA::category() const
{
  return TextCategory(m_category);
}


TextOverlayAttribute TextCategoryAOA::overlay() const
{
  return TextOverlayAttribute(m_overlay);
}


int TextCategoryAOA::compareTo(TextCategoryAOA const &b) const
{
  auto const &a = *this;

  using smbase::compare;

  RET_IF_COMPARE_MEMBERS(m_category);
  RET_IF_COMPARE_MEMBERS(m_overlay);

  return 0;
}


char TextCategoryAOA::categoryLetter() const
{
  int cat = categoryNumber();

  if (0 <= cat && cat <= 9) {
    return cat+'0';
  }
  else if (cat < 36) {
    return cat-10+'A';
  }
  else if (cat < 62) {
    return cat-36+'a';
  }
  else {
    // I don't expect to have anywhere near 62 categories, so
    // collapsing the rest into one char shouldn't be a problem.
    return '+';
  }
}


int TextCategoryAOA::categoryNumber() const
{
  return int(m_category);
}


char TextCategoryAOA::overlayLetter() const
{
  char const letters[] = { ' ', 's', 'h', 'p' };
  ASSERT_TABLESIZE(letters,
    int(TextOverlayAttribute::NUM_TEXT_OVERLAY_ATTRIBUTES));

  int ovl = m_overlay;
  xassert(cc::z_le_lt(ovl, TABLESIZE(letters)));

  return letters[ovl];
}


TextCategoryAOA TextCategoryAOA::withOverlay(
  TextOverlayAttribute overlay) const
{
  return TextCategoryAOA(category(), overlay);
}


void TextCategoryAOA::write(std::ostream &os) const
{
  os << categoryLetter();
  if (overlay() != TextOverlayAttribute::TOA_NONE) {
    os << overlayLetter();
  }
}


void LineCategoryAOAs::overlay(
  ByteOrColumnIndex start,
  ByteOrColumnIndex ovlLength,
  TextOverlayAttribute overlay)
{
  RLEInfiniteSequence<std::optional<TextOverlayAttribute>> ovl;

  ovl.append(std::nullopt, start);

  if (ovlLength > 0) {
    ovl.append(overlay, ovlLength);
  }
  else {
    ovl.setTailValue(overlay);
  }

  auto combine =
    [](TextCategoryAOA catAOA,
       std::optional<TextOverlayAttribute> overlayOpt)
      -> TextCategoryAOA
    {
      if (overlayOpt) {
        // Apply overlay.
        return catAOA.withOverlay(*overlayOpt);
      }
      else {
        // No change.
        return catAOA;
      }
    };

  Base dest = combineSequences<TextCategoryAOA>(*this, ovl, combine);
  swapWith(dest);
}


// The logic here deliberately mirrors that of `overlay` since its
// purpose is to test that logic.
void LineCategoryAOAs::overwrite(
  ByteOrColumnIndex start,
  ByteOrColumnIndex replLength,
  TextCategoryAOA newValue)
{
  RLEInfiniteSequence<std::optional<TextCategoryAOA>> repl;

  repl.append(std::nullopt, start);

  if (replLength > 0) {
    repl.append(newValue, replLength);
  }
  else {
    repl.setTailValue(newValue);
  }

  auto combine =
    [](TextCategoryAOA catAOA,
       std::optional<TextCategoryAOA> newValueOpt)
      -> TextCategoryAOA
    {
      if (newValueOpt) {
        return *newValueOpt;
      }
      else {
        return catAOA;
      }
    };

  Base dest = combineSequences<TextCategoryAOA>(*this, repl, combine);
  swapWith(dest);
}


TextCategoryAOA LineCategoryAOAs::getCategoryAOAAt(
  ByteOrColumnIndex index) const
{
  return at(index);
}


// EOF
