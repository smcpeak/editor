// lex_hilite.cc
// code for lex_hilite.h

#include "lex_hilite.h"    // this module
#include "inclexer.h"      // IncLexer
#include "style.h"         // LineStyle
#include "buffer.h"        // BufferCore, Buffer
#include "trace.h"         // TRACE
#include "strutil.h"       // quoted

#include <stdlib.h>        // exit


LexHighlighter::LexHighlighter(BufferCore const &buf, IncLexer &L)
  : buffer(buf),
    lexer(L),
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

LexHighlighter::~LexHighlighter()
{
  buffer.observers.removeItem(this);
}


void LexHighlighter::checkInvar() const
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


void LexHighlighter::addToChanged(int line)
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
    xassert(changedBegin <= line &&
                            line < changedEnd);
  }

  checkInvar();
}


LexerState LexHighlighter::getSavedState(int line)
{
  if (line < 0) {
    return LS_INITIAL;
  }
  else {
    return (LexerState)savedState.get(line);
  }
}


void LexHighlighter::insertLine(BufferCore const &, int line)
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


void LexHighlighter::deleteLine(BufferCore const &, int line)
{
  // if region ends after 'line', then now it ends one line earlier
  if (!changedIsEmpty() &&
      changedEnd > line) {
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



void LexHighlighter::insertText(BufferCore const &, int line, int, char const *, int)
{
  addToChanged(line);
}

void LexHighlighter::deleteText(BufferCore const &, int line, int, int)
{
  addToChanged(line);
}


void LexHighlighter::saveLineState(int line, LexerState _state)
{
  LineState state = (LineState)_state;
  xassert(state == _state);      // make sure didn't truncate; if did, need to enlarge LineState's representation size

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


void LexHighlighter::highlight(BufferCore const &buf, int line, LineStyle &style)
{
  xassert(&buf == &buffer);

  // push the changed region down to the line of interest
  LexerState prevState = getSavedState(changedBegin-1);
  while (!changedIsEmpty() && changedBegin < line) {
    TRACE("highlight", "push changed: scanning line " << changedBegin);
    lexer.beginScan(&buf, changedBegin, prevState);

    Style code;
    while (lexer.getNextToken(code))
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

    Style code;
    while (lexer.getNextToken(code))
      {}

    prevState = lexer.getState();

    // this increments 'waterline'
    saveLineState(waterline, prevState);
  }

  // recall the saved state for the line of interest
  TRACE("highlight", "at requested: scanning line " << line);
  prevState = getSavedState(line-1);
  lexer.beginScan(&buf, line, prevState);

  // append each styled segment
  Style code;
  int len = lexer.getNextToken(code);
  while (len) {
    style.append(code, len);
    len = lexer.getNextToken(code);
  }
  style.endStyle = code;    // line trails off with the final code

  saveLineState(line, lexer.getState());
}


// ---------------------- test code -------------------------
static MakeHighlighterFunc makeHigh;
static Buffer *buf;

static void printLine(LexHighlighter &hi, int line)
{
  LineStyle style(ST_NORMAL);
  hi.highlight(*buf, line, style);

  cout << "line " << line << ":\n"
       << "  text : " << buf->getWholeLine(line) << "\n"
       << "  style: " << style.asUnaryString() << "\n"
       << "  rle  : " << style.asString() << "\n"
       ;
}

static void printStyles(LexHighlighter &hi)
{
  // stop short so I have a waterline
  for (int i=0; i < buf->numLines()-1; i++) {
    printLine(hi, i);
  }
}

static void insert(int line, int col, char const *text)
{
  cout << "insert(" << line << ", " << col << ", "
       << quoted(text) << ")\n";
  buf->insertTextRange(line, col, text);
}

static void del(int line, int col, int len)
{
  cout << "del(" << line << ", " << col << ", " << len << ")\n";
  buf->deleteText(line, col, len);
}

static void innerCheckLine(LexHighlighter &hi,
                           LexHighlighter &batch, int i)
{
  LineStyle style1(ST_NORMAL);
  hi.highlight(*buf, i, style1);
  string rendered1 = style1.asUnaryString();

  LineStyle style2(ST_NORMAL);
  batch.highlight(*buf, i, style2);
  string rendered2 = style2.asUnaryString();

  // compare using rendered strings, instead of looking at
  // the run-length ranges, since it's ok if the incrementality
  // somehow gives rise to slightly different ranges (say, in one
  // version there are two adjacent ranges of same-style chars)

  if (rendered1 != rendered2) {
    cout << "check: mismatch at line " << i << ":\n"
         << "  line: " << buf->getWholeLine(i) << "\n"
         << "  inc.: " << rendered1 << "\n"
         << "  bat.: " << rendered2 << "\n"
         ;
    exit(2);
  }
}

// check that the incremental highlighter matches a batch highlighter
static void check(LexHighlighter &hi)
{
  LexHighlighter *batch = makeHigh(*buf);    // batch because it has no initial info

  // go backwards in hopes of finding more incrementality bugs
  for (int i = buf->numLines()-1; i>=0; i--) {
    innerCheckLine(hi, *batch, i);
  }

  delete batch;
}


static void checkLine(LexHighlighter &hi, int line)
{
  LexHighlighter *batch = makeHigh(*buf);

  innerCheckLine(hi, *batch, line);

  delete batch;
}


void exerciseHighlighter(MakeHighlighterFunc func)
{
  // at first this was global, then I thought of a problem, and then I
  // forgot what the problem was...
  Buffer _buf;
  buf = &_buf;

  makeHigh = func;
  LexHighlighter *_hi = makeHigh(*buf);
  LexHighlighter &hi = *_hi;

  int line=0, col=0;
  buf->insertTextRange(line, col,
    "hi there\n"
    "here is \"a string\" ok?\n"
    "and how about /*a comment*/ yo\n"
    "C++ comment: // I like C++\n"
    "back to int normalcy\n"
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

  insert(2, 30, "*/");
  checkLine(hi, 3);
  check(hi);

  buf = NULL;
  makeHigh = NULL;

  delete _hi;
}


// EOF
