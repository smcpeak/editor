// style.cc
// code for style.h

#include "style.h"       // this module


// ----------------------- LineStyle --------------------
void LineStyle::append(Style style, int length)
{
  // same as preceeding style?
  if (isNotEmpty() && (top().style == style)) {
    // coalesce
    top().length += length;
  }
  else {
    // add new
    push(StyleEntry(style, length));
  }
}

void LineStyle::overlay(int start, int ovlLength, Style ovlStyle)
{
  // walk over this style array, making a new one
  LineStyleIter iter(*this);
  LineStyle dest(endStyle);

  // skip to the start of the relevant section, copying style entries
  while (iter.length!=0 && iter.length <= start) {
    dest.append(iter.style, iter.length);
    start -= iter.length;
    iter.nextRun();
  }

  if (iter.length!=0 && start>0) {
    // we have a run that extends into the overlay section; copy
    // only part of it
    xassert(iter.length > start);
    dest.append(iter.style, start);
    iter.advanceChars(start);
    start = 0;
  }

  if (iter.length==0 && start>0) {
    // turn the previously infinite section into a finite section
    // to finish out the stuff before the overlay
    dest.append(endStyle, start);
    start = 0;
  }

  // write the overlay style into 'dest'
  xassert(start == 0);
  if (ovlLength == 0) {
    // infinite; it's remaining part
    dest.endStyle = ovlStyle;
  }
  else {
    dest.append(ovlStyle, ovlLength);

    // skip past the original style array's fragment under the overlay
    iter.advanceChars(ovlLength);

    // copy the remaining style elements
    while (!iter.atEnd()) {
      dest.append(iter.style, iter.length);
      iter.nextRun();
    }
  }

  // replace *this with 'dest'
  endStyle = dest.endStyle;
  swapWith(dest);
}


Style LineStyle::getStyleAt(int index) const
{
  for (LineStyleIter iter(*this); !iter.atEnd(); iter.nextRun()) {
    if (index < iter.length) {
      return iter.style;
    }
    index -= iter.length;
  }
  
  return endStyle;
}


string LineStyle::asString() const
{
  stringBuilder sb;

  LineStyleIter iter(*this);
  while (!iter.atEnd()) {
    sb << "[" << (int)iter.style << "," << iter.length << "]";
    iter.nextRun();
  }
  sb << "[" << (int)endStyle;     // leave last '[' unbalanced

  return sb;
}



// get a single-character code for the style
static char styleCodeChar(int style)
{
  if (0 <= style && style <= 9) {
    return style+'0';
  }
  else if (style < 36) {
    return style-10+'A';
  }
  else if (style < 62) {
    return style-36+'a';
  }
  else {
    // I don't expect to have anywhere near 62 styles, so collapsing
    // the rest into one char shouldn't be a problem
    return '+';
  }
}

string LineStyle::asUnaryString() const
{
  stringBuilder sb;

  LineStyleIter iter(*this);
  while (!iter.atEnd()) {
    for (int i=0; i<iter.length; i++) {
      sb << styleCodeChar(iter.style);
    }
    iter.nextRun();
  }
  sb << styleCodeChar(endStyle) << "...";

  return sb;
}


// ----------------------- LineStyleIter --------------------
LineStyleIter::LineStyleIter(LineStyle const &s)
  : styles(s),
    entry(-1)
{           
  nextRun();
}

void LineStyleIter::nextRun()
{
  entry++;
  if (entry < styles.length()) {
    length = styles[entry].length;
    style = styles[entry].style;
  }
  else {
    length = 0;    // infinite
    style = styles.endStyle;
  }
}


void LineStyleIter::advanceChars(int n)
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
#ifdef TEST_STYLE

#include "test.h"        // USUAL_MAIN
#include "sm-iostream.h" // cout

void expect(LineStyle const &style, char const *str)
{
  string s = style.asString();
  cout << s << endl;

  if (!s.equals(str)) {
    cout << "expected: " << str << endl;
    exit(2);
  }
}

// less awkward literal casts..
static inline Style s(int sty) { return (Style)sty; }

void entry()
{
  LineStyle style(s(3));
  expect(style, "[3");
  // 3...

  style.append(s(4),5);
  expect(style, "[4,5][3");
  // 444443...

  style.append(s(6),7);
  expect(style, "[4,5][6,7][3");
  // 4444466666663...

  style.overlay(2, 5, s(8));
  expect(style, "[4,2][8,5][6,5][3");
  // 4488888666663...

  style.overlay(0, 9, s(1));
  expect(style, "[1,9][6,3][3");
  // 1111111116663...

  style.overlay(3, 4, s(5));
  expect(style, "[1,3][5,4][1,2][6,3][3");
  // 1115555116663...

  style.overlay(9, 0, s(7));
  expect(style, "[1,3][5,4][1,2][7");
  // 1115555117...

  style.overlay(5, 0, s(8));
  expect(style, "[1,3][5,2][8");
  // 111558...

  style.overlay(10, 0, s(7));
  expect(style, "[1,3][5,2][8,5][7");
  // 11155888887...

  style.append(s(4), 3);
  expect(style, "[1,3][5,2][8,5][4,3][7");
  // 11155888884447...

  style.overlay(4, 9, s(3));
  expect(style, "[1,3][5,1][3,9][7");
  // 11153333333337...

  style.overlay(0, 4, s(6));
  expect(style, "[6,4][3,9][7");
  // 66663333333337...

  style.overlay(6, 4, s(4));
  expect(style, "[6,4][3,2][4,4][3,3][7");
  // 66663344443337...

  style.overlay(4, 6, s(8));
  expect(style, "[6,4][8,6][3,3][7");
  // 66668888883337...

  style.overlay(2, 10, s(1));
  expect(style, "[6,2][1,10][3,1][7");
  // 66111111111137...

  style.clear(s(2));
  expect(style, "[2");
  // 2...
}

USUAL_MAIN

#endif // TEST_STYLE
