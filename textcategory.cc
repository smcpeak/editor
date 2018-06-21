// textcategory.cc
// code for textcategory.h

#include "textcategory.h"              // this module


// ----------------------- LineCategories --------------------
void LineCategories::append(TextCategory category, int length)
{
  // same as preceeding category?
  if (isNotEmpty() && (top().category == category)) {
    // coalesce
    top().length += length;
  }
  else {
    // add new
    push(TCSpan(category, length));
  }
}

void LineCategories::overlay(int start, int ovlLength, TextCategory ovlCategory)
{
  // Walk over this category array, making a new one.
  LineCategoryIter iter(*this);
  LineCategories dest(endCategory);

  // Skip to the start of the relevant section, copying category entries.
  while (iter.length!=0 && iter.length <= start) {
    dest.append(iter.category, iter.length);
    start -= iter.length;
    iter.nextRun();
  }

  if (iter.length!=0 && start>0) {
    // we have a run that extends into the overlay section; copy
    // only part of it
    xassert(iter.length > start);
    dest.append(iter.category, start);
    iter.advanceChars(start);
    start = 0;
  }

  if (iter.length==0 && start>0) {
    // turn the previously infinite section into a finite section
    // to finish out the stuff before the overlay
    dest.append(endCategory, start);
    start = 0;
  }

  // write the overlay category into 'dest'
  xassert(start == 0);
  if (ovlLength == 0) {
    // infinite; it's remaining part
    dest.endCategory = ovlCategory;
  }
  else {
    dest.append(ovlCategory, ovlLength);

    // skip past the original category array's fragment under the overlay
    iter.advanceChars(ovlLength);

    // copy the remaining category elements
    while (!iter.atEnd()) {
      dest.append(iter.category, iter.length);
      iter.nextRun();
    }
  }

  // replace *this with 'dest'
  endCategory = dest.endCategory;
  swapWith(dest);
}


TextCategory LineCategories::getCategoryAt(int index) const
{
  for (LineCategoryIter iter(*this); !iter.atEnd(); iter.nextRun()) {
    if (index < iter.length) {
      return iter.category;
    }
    index -= iter.length;
  }

  return endCategory;
}


string LineCategories::asString() const
{
  stringBuilder sb;

  LineCategoryIter iter(*this);
  while (!iter.atEnd()) {
    sb << "[" << (int)iter.category << "," << iter.length << "]";
    iter.nextRun();
  }
  sb << "[" << (int)endCategory;     // leave last '[' unbalanced

  return sb;
}



// Get a single-character code for the category.
static char categoryCodeChar(int category)
{
  if (0 <= category && category <= 9) {
    return category+'0';
  }
  else if (category < 36) {
    return category-10+'A';
  }
  else if (category < 62) {
    return category-36+'a';
  }
  else {
    // I don't expect to have anywhere near 62 categories, so
    // collapsing the rest into one char shouldn't be a problem.
    return '+';
  }
}

string LineCategories::asUnaryString() const
{
  stringBuilder sb;

  LineCategoryIter iter(*this);
  while (!iter.atEnd()) {
    for (int i=0; i<iter.length; i++) {
      sb << categoryCodeChar(iter.category);
    }
    iter.nextRun();
  }
  sb << categoryCodeChar(endCategory) << "...";

  return sb;
}


// ----------------------- LineCategoryIter --------------------
LineCategoryIter::LineCategoryIter(LineCategories const &c)
  : categories(c),
    entry(-1)
{
  nextRun();
}

void LineCategoryIter::nextRun()
{
  entry++;
  if (entry < categories.length()) {
    length = categories[entry].length;
    category = categories[entry].category;
  }
  else {
    length = 0;    // infinite
    category = categories.endCategory;
  }
}


void LineCategoryIter::advanceChars(int n)
{
  while (length!=0 && n>0) {
    if (length <= n) {
      // skip past this entire run
      n -= length;
      nextRun();
    }
    else {
      // consume part of this run
      length -= n;
      n = 0;
    }
  }
}


// ----------------------- test code ---------------------
#ifdef TEST_TEXTCATEGORY

#include "test.h"        // USUAL_MAIN
#include "sm-iostream.h" // cout

void expect(LineCategories const &category, char const *str)
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

void entry()
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

#endif // TEST_TEXTCATEGORY
