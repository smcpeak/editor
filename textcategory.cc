// textcategory.cc
// code for textcategory.h

#include "textcategory.h"              // this module


// --------------------------- TCSpan ------------------------
bool TCSpan::operator== (TCSpan const &obj) const
{
  return EMEMB(category) &&
         EMEMB(length);
}


// ----------------------- LineCategories --------------------
bool LineCategories::operator== (LineCategories const &obj) const
{
  return this->endCategory == obj.endCategory &&

         // Use the operator== that works on ArrayStacks to compare the
         // subobjects.
         static_cast<ArrayStack<TCSpan> const &>(*this) ==
           static_cast<ArrayStack<TCSpan> const &>(obj);
}


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
    iter.advanceCharsOrCols(start);
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
    iter.advanceCharsOrCols(ovlLength);

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


void LineCategoryIter::advanceCharsOrCols(int n)
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


// EOF
