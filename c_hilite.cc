// c_hilite.cc
// code for c_hilite.h

#include "c_hilite.h"      // this module
#include "style.h"         // LineStyle
#include "buffer.h"        // BufferCore, Buffer
#include "trace.h"         // TRACE


C_Highlighter::C_Highlighter(BufferCore const &buf)
  : buffer(buf),
    lexer(),
    savedState(),
    changedBegin(0),
    changedEnd(0),
    waterline(0)
{
  buffer.observers.append(this);

  // all of the saved state is stale
  savedState.insertManyZeroes(0, buf.numLines());

  checkInvar();
}

C_Highlighter::~C_Highlighter()
{
  buffer.observers.removeItem(this);
}


void C_Highlighter::checkInvar() const
{   
  xassert(0 <= waterline &&
               waterline <= buffer.numLines());

  if (!changedIsEmpty()) {   
    xassert(changedBegin < changedEnd);
    xassert(waterline >= changedEnd);

    xassert(0 <= changedBegin &&
                 changedEnd <= buffer.numLines());
  }
}


void C_Highlighter::addToChanged(int line)
{
  // invariant: considering the actual set of changed lines (not the
  // conservative overapproximation I store), the 'changed' region is
  // the topmost contiguous subset of actual changed lines, and the
  // water begins at the next actual changed line after that

  if (line >= waterline) {
    // do nothing, it's in the water
  }
  else if (changedIsEmpty()) {
    changedBegin = line;
    changedEnd = line+1;
  }
  else if (line == changedBegin-1) {
    // extend changed region up by 1
    changedBegin--;
  }
  else if (line == changedEnd) {
    // extend changed region down by 1
    changedEnd++;
  }
  else if (line < changedBegin) {
    // line is discontiguous with existing changed region, so absorb
    // the current region into the water and set the new changed region
    // to contain just this line
    waterline = changedBegin;
    changedBegin = line;
    changedEnd = line+1;
  }
  else if (line > changedEnd) {
    // line is below changed region, move the water up to absorb it
    waterline = line;
  }
  else {
    // line must be in changed region already, nothing to do
    xassert(changedEnd <= line &&
                          line < changedEnd);
  }

  checkInvar();
}


C_Highlighter::LineState C_Highlighter::getSavedState(int line)
{
  if (line < 0) {
    return 0;
  }
  else {
    return savedState.get(line);
  }
}


void C_Highlighter::insertLine(BufferCore const &, int line)
{
  // if region ends after 'line', then now it ends one line later
  if (!changedIsEmpty() &&
      changedEnd >= line+1) {
    changedEnd++;
  }

  // similarly for the waterline
  if (waterline >= line) {
    waterline++;
  }

  addToChanged(line);

  // insert a new saved state, initialized to the state of the
  // line above it
  savedState.insert(line, getSavedState(line-1));
}


void C_Highlighter::deleteLine(BufferCore const &, int line)
{
  // if region ends after 'line', then now it ends one line earlier
  if (!changedIsEmpty() &&
      changedEnd > line+1) {
    changedEnd--;
  }

  // similarly for waterline
  if (waterline > line) {
    waterline--;
  }

  addToChanged(line);

  // remove a saved state
  savedState.remove(line);
}



void C_Highlighter::insertText(BufferCore const &, int line, int, char const *, int)
{
  addToChanged(line);
}

void C_Highlighter::deleteText(BufferCore const &, int line, int, int)
{
  addToChanged(line);
}


void C_Highlighter::saveLineState(int line, LineState state)
{
  LineState prev = savedState.get(line);

  if (line >= waterline) {
    savedState.set(line, state);
    if (line == waterline) {
      // push down waterline by 1
      waterline++;
    }
  }
  else if (changedIsEmpty()) {
    xassert(prev == state);
  }
  else {
    if (line == changedBegin) {
      changedBegin++;
      savedState.set(line, state);
    }

    if (line+1 == changedEnd && prev != state) {
      // the state has changed, so we need to re-eval the next line
      if (changedEnd < waterline) {
        changedEnd++;
      }
      else {                             
        // no need to keep moving the 'changed' region, the waterline
        // has responsibility for marking 'line+1' and beyond as "changed"
        xassert(changedEnd == waterline);
      }
    }
  }

  checkInvar();
}


void C_Highlighter::highlight(BufferCore const &buf, int line, LineStyle &style)
{
  xassert(&buf == &buffer);

  // push the changed region down to the line of interest
  int prevState = getSavedState(changedBegin-1);
  while (!changedIsEmpty() && changedBegin < line) {
    TRACE("highlight", "push changed: scanning line " << changedBegin);
    lexer.beginScan(&buf, changedBegin, prevState);

    int len, code;
    while (lexer.getNextToken(len, code))
      {}

    prevState = lexer.getState();

    // this increments 'changedBegin', and if the state doesn't change
    // then it won't increment 'changedEnd', so we might exit the loop
    // due to changedIsEmpty() becoming true
    saveLineState(changedBegin, prevState);
  }

  // push the waterline down also; do this after moving 'changed'
  // because 'changes' is above and we need those highlighting actions
  // to have completed so we're not working with stale saved stages
  prevState = getSavedState(waterline-1);
  while (waterline < line) {
    TRACE("highlight", "push waterline: scanning line " << waterline);
    lexer.beginScan(&buf, waterline, prevState);

    int len, code;
    while (lexer.getNextToken(len, code))
      {}

    prevState = lexer.getState();

    // this increments 'waterline'
    saveLineState(waterline, prevState);
  }

  // recall the saved state for the line of interest
  TRACE("highlight", "at requested: scanning line " << line);
  lexer.beginScan(&buf, line, getSavedState(line-1));

  // append each styled segment
  int len, code;
  while (lexer.getNextToken(len, code)) {
    style.append((Style)code, len);
  }
  
  saveLineState(line, lexer.getState());
}


// ---------------------- test code -------------------------
#ifdef TEST_C_HILITE

#include "test.h"        // USUAL_MAIN

Buffer buf;

void printLine(C_Highlighter &hi, int line)
{
  LineStyle style(ST_NORMAL);
  hi.highlight(buf, line, style);

  cout << "line " << line << ":\n"
       << "  text : " << buf.getWholeLine(line) << "\n"
       << "  style: " << style.asUnaryString() << endl;
}

void printStyles(C_Highlighter &hi)
{
  // stop short so I have a waterline
  for (int i=0; i<buf.numLines()-1; i++) {
    printLine(hi, i);
  }
}

void insert(int line, int col, char const *text)
{
  buf.insertTextRange(line, col, text);
}

void del(int line, int col, int len)
{
  buf.deleteText(line, col, len);
}

// check that the incremental highlighter matches a batch highlighter
void check(C_Highlighter &hi)
{
  C_Highlighter batch(buf);    // batch because it has no initial info

  for (int i=0; i<buf.numLines(); i++) {
    LineStyle style1(ST_NORMAL);
    hi.highlight(buf, i, style1);
    string rendered1 = style1.asUnaryString();

    LineStyle style2(ST_NORMAL);
    hi.highlight(buf, i, style2);
    string rendered2 = style1.asUnaryString();

    // compare using rendered strings, instead of looking at
    // the run-length ranges, since it's ok if the incrementality
    // somehow gives rise to slightly different ranges (say, in one
    // version there are two adjacent ranges of same-style chars)

    if (rendered1 != rendered2) {
      cout << "check: mismatch at line " << i << ":\n"
           << "  line: " << buf.getWholeLine(i) << "\n"
           << "  inc.: " << rendered1 << "\n"
           << "  bat.: " << rendered2 << "\n"
           ;
    }
  }
}


void entry()
{
  traceAddSys("highlight");

  C_Highlighter hi(buf);

  int line=0, col=0;
  buf.insertTextRange(line, col,
    "hi there\n"
    "here is \"a string\" ok?\n"
    "and how about /*a comment*/ yo\n"
    "C++ comment: // I like C++\n"
    "back to normalcy\n"
  );
  printStyles(hi);
  check(hi);

  insert(2, 3, " what");
  printLine(hi, line);
  check(hi);

  insert(0, 3, "um, ");
  insert(2, 0, "derf ");
  insert(4, 5, "there ");
  printLine(hi, 1);
  printLine(hi, 2);
  printLine(hi, 4);
  check(hi);

  insert(0, 7, "/""*");
  printLine(hi, 4);
  printStyles(hi);
  check(hi);
  printStyles(hi);

  insert(0, 2, "\"");
  del(2, 35, 2);
  insert(4, 2, "Arg");
  printLine(hi, 4);
  check(hi);
  printStyles(hi);

  insert(0, 15, "\\");
  check(hi);
  printStyles(hi);

}

USUAL_MAIN

#endif //  TEST_C_HILITE
