// textcategory-test.cc
// Tests for 'textcategory' module.

#include "unit-tests.h"                // decl for my entry point

#include "textcategory.h"              // module under test

#include "smbase/sm-test.h"            // DIAG, EXPECT_EQ


static void expect(
  LineCategoryAOAs const &category,
  char const *str,
  char const *unaryStr)
{
  string s = category.asString();
  DIAG(s);
  EXPECT_EQ(s, str);

  EXPECT_EQ(category.asUnaryString(), unaryStr);
}

// less awkward literal casts..
static inline TextCategoryAOA s(int sty)
{
  return TextCategoryAOA(TextCategory(sty));
}


// Test the `overwrite` method.  These are some tests written before
// overlay was added.
void test_overwrite()
{
  LineCategoryAOAs category(s(3));
  expect(category, "[3",
    "3...");

  category.append(s(4),5);
  expect(category, "[4,5][3",
    "444443...");

  category.append(s(6),7);
  expect(category, "[4,5][6,7][3",
    "4444466666663...");

  category.overwrite(2, 5, s(8));
  expect(category, "[4,2][8,5][6,5][3",
    "4488888666663...");

  category.overwrite(0, 9, s(1));
  expect(category, "[1,9][6,3][3",
    "1111111116663...");

  category.overwrite(3, 4, s(5));
  expect(category, "[1,3][5,4][1,2][6,3][3",
    "1115555116663...");

  category.overwrite(9, 0, s(7));
  expect(category, "[1,3][5,4][1,2][7",
    "1115555117...");

  category.overwrite(5, 0, s(8));
  expect(category, "[1,3][5,2][8",
    "111558...");

  category.overwrite(10, 0, s(7));
  expect(category, "[1,3][5,2][8,5][7",
    "11155888887...");

  category.append(s(4), 3);
  expect(category, "[1,3][5,2][8,5][4,3][7",
    "11155888884447...");

  category.overwrite(4, 9, s(3));
  expect(category, "[1,3][5,1][3,9][7",
    "11153333333337...");

  category.overwrite(0, 4, s(6));
  expect(category, "[6,4][3,9][7",
    "66663333333337...");

  category.overwrite(6, 4, s(4));
  expect(category, "[6,4][3,2][4,4][3,3][7",
    "66663344443337...");

  category.overwrite(4, 6, s(8));
  expect(category, "[6,4][8,6][3,3][7",
    "66668888883337...");

  category.overwrite(2, 10, s(1));
  expect(category, "[6,2][1,10][3,1][7",
    "66111111111137...");

  category.clear(s(2));
  expect(category, "[2",
    "2...");
}


static inline TextOverlayAttribute o(int ov)
{
  return TextOverlayAttribute(ov);
}


static char componentLetter(
  TextCategoryAOA const &catAOA,
  bool wantCategory)
{
  return wantCategory?
           catAOA.categoryLetter() :
           catAOA.overlayLetter();
}


static std::string unaryComponentString(
  LineCategoryAOAs const &categories,
  bool wantCategory)
{
  std::ostringstream oss;

  LineCategoryAOAIter iter(categories);
  while (!iter.atEnd()) {
    smbase_loopi(iter.runLength()) {
      oss << componentLetter(iter.value(), wantCategory);
    }
    iter.nextRun();
  }
  oss << componentLetter(categories.tailValue(), wantCategory) << "...";

  return oss.str();
}


static void expectOverlay(
  LineCategoryAOAs const &category,
  char const *str,
  char const *unaryCategoryStr,
  char const *unaryOverlayStr)
{
  string s = category.asString();
  DIAG(s);
  EXPECT_EQ(s, str);

  EXPECT_EQ(unaryComponentString(category, true /*wantCategory*/),
            unaryCategoryStr);

  EXPECT_EQ(unaryComponentString(category, false /*wantCategory*/),
            unaryOverlayStr);
}


void test_overlay()
{
  LineCategoryAOAs category(s(3));
  expectOverlay(category, "[3",
    "3...",
    " ...");

  category.append(s(4),5);
  expectOverlay(category, "[4,5][3",
    "444443...",
    "      ...");

  category.append(s(6),7);
  expectOverlay(category, "[4,5][6,7][3",
    "4444466666663...",
    "             ...");

  category.overlay(2, 5, o(1));
  expectOverlay(category, "[4,2][4s,3][6s,2][6,5][3",
    "4444466666663...",
    "  sssss      ...");

  category.overlay(0, 9, o(2));
  expectOverlay(category, "[4h,5][6h,4][6,3][3",
    "4444466666663...",
    "hhhhhhhhh    ...");

  category.overlay(3, 4, o(1));
  expectOverlay(category, "[4h,3][4s,2][6s,2][6h,2][6,3][3",
    "4444466666663...",
    "hhhsssshh    ...");

  category.overlay(10, 0, o(1));
  expectOverlay(category, "[4h,3][4s,2][6s,2][6h,2][6,1][6s,2][3s",
    "4444466666663...",
    "hhhsssshh sss...");

  category.overlay(5, 0, o(2));
  expectOverlay(category, "[4h,3][4s,2][6h,7][3h",
    "4444466666663...",
    "hhhsshhhhhhhh...");

  category.overlay(10, 0, o(1));
  expectOverlay(category, "[4h,3][4s,2][6h,5][6s,2][3s",
    "4444466666663...",
    "hhhsshhhhhsss...");

  category.append(s(4), 3);
  expectOverlay(category, "[4h,3][4s,2][6h,5][6s,2][4,3][3s",
    "4444466666664443...",
    "hhhsshhhhhss   s...");

  category.overlay(5, 9, o(0));
  expectOverlay(category, "[4h,3][4s,2][6,7][4,3][3s",
    "4444466666664443...",
    "hhhss          s...");

  category.overlay(0, 4, o(1));
  expectOverlay(category, "[4s,5][6,7][4,3][3s",
    "4444466666664443...",
    "sssss          s...");

  category.overlay(6, 4, o(2));
  expectOverlay(category, "[4s,5][6,1][6h,4][6,2][4,3][3s",
    "4444466666664443...",
    "sssss hhhh     s...");

  category.overlay(4, 6, o(3));
  expectOverlay(category, "[4s,4][4p,1][6p,5][6,2][4,3][3s",
    "4444466666664443...",
    "sssspppppp     s...");

  category.overlay(2, 10, o(2));
  expectOverlay(category, "[4s,2][4h,3][6h,7][4,3][3s",
    "4444466666664443...",
    "sshhhhhhhhhh   s...");

  category.clear(s(2));
  expectOverlay(category, "[2",
    "2...",
    " ...");
}


// Called from unit-tests.cc.
void test_textcategory(CmdlineArgsSpan args)
{
  test_overwrite();
  test_overlay();
}


// EOF
