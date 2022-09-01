// textcategory-test.cc
// Tests for 'textcategory' module.

#include "textcategory.h"              // module under test

#include "sm-test.h"                   // USUAL_MAIN
#include "sm-iostream.h"               // cout


static void expect(LineCategories const &category, char const *str)
{
  string s = category.asString();
  cout << s << endl;

  if (!s.equals(str)) {
    cout << "expected: " << str << endl;
    exit(2);
  }
}

// less awkward literal casts..
static inline TextCategory s(int sty) { return (TextCategory)sty; }

static void entry()
{
  LineCategories category(s(3));
  expect(category, "[3");
  // 3...

  category.append(s(4),5);
  expect(category, "[4,5][3");
  // 444443...

  category.append(s(6),7);
  expect(category, "[4,5][6,7][3");
  // 4444466666663...

  category.overlay(2, 5, s(8));
  expect(category, "[4,2][8,5][6,5][3");
  // 4488888666663...

  category.overlay(0, 9, s(1));
  expect(category, "[1,9][6,3][3");
  // 1111111116663...

  category.overlay(3, 4, s(5));
  expect(category, "[1,3][5,4][1,2][6,3][3");
  // 1115555116663...

  category.overlay(9, 0, s(7));
  expect(category, "[1,3][5,4][1,2][7");
  // 1115555117...

  category.overlay(5, 0, s(8));
  expect(category, "[1,3][5,2][8");
  // 111558...

  category.overlay(10, 0, s(7));
  expect(category, "[1,3][5,2][8,5][7");
  // 11155888887...

  category.append(s(4), 3);
  expect(category, "[1,3][5,2][8,5][4,3][7");
  // 11155888884447...

  category.overlay(4, 9, s(3));
  expect(category, "[1,3][5,1][3,9][7");
  // 11153333333337...

  category.overlay(0, 4, s(6));
  expect(category, "[6,4][3,9][7");
  // 66663333333337...

  category.overlay(6, 4, s(4));
  expect(category, "[6,4][3,2][4,4][3,3][7");
  // 66663344443337...

  category.overlay(4, 6, s(8));
  expect(category, "[6,4][8,6][3,3][7");
  // 66668888883337...

  category.overlay(2, 10, s(1));
  expect(category, "[6,2][1,10][3,1][7");
  // 66111111111137...

  category.clear(s(2));
  expect(category, "[2");
  // 2...
}

USUAL_MAIN


// EOF
