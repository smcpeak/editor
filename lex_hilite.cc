// lex_hilite.cc
// code for lex_hilite.h

#include "lex_hilite.h"                // this module

#include "inclexer.h"                  // IncLexer
#include "strutil.h"                   // quoted
#include "td-core.h"                   // TextDocumentCore
#include "td-editor.h"                 // TextDocumentEditor
#include "textcategory.h"              // LineCategories
#include "trace.h"                     // TRACE

#include <stdlib.h>                    // exit


LexHighlighter::LexHighlighter(TextDocumentCore const &buf, IncLexer &L)
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


void LexHighlighter::observeInsertLine(TextDocumentCore const &, int line)
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


void LexHighlighter::observeDeleteLine(TextDocumentCore const &, int line)
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



void LexHighlighter::observeInsertText(TextDocumentCore const &, TextCoord tc, char const *, int)
{
  addToChanged(tc.line);
}

void LexHighlighter::observeDeleteText(TextDocumentCore const &, TextCoord tc, int)
{
  addToChanged(tc.line);
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


void LexHighlighter::highlight(TextDocumentCore const &buf, int line, LineCategories &categories)
{
  xassert(&buf == &buffer);

  // push the changed region down to the line of interest
  LexerState prevState = getSavedState(changedBegin-1);
  while (!changedIsEmpty() && changedBegin < line) {
    TRACE("highlight", "push changed: scanning line " << changedBegin);
    lexer.beginScan(&buf, changedBegin, prevState);

    TextCategory code;
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

    TextCategory code;
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

  // Append each categorized segment.
  TextCategory code;
  int len = lexer.getNextToken(code);
  while (len) {
    categories.append(code, len);
    len = lexer.getNextToken(code);
  }
  categories.endCategory = code;    // line trails off with the final code

  saveLineState(line, lexer.getState());
}


// ---------------------- test code -------------------------
static MakeHighlighterFunc makeHigh;
static TextDocumentEditor *tde;

static void printLine(LexHighlighter &hi, int line)
{
  LineCategories category(TC_NORMAL);
  hi.highlight(tde->core(), line, category);

  cout << "line " << line << ":\n"
       << "  text : " << tde->getWholeLine(line) << "\n"
       << "  catgy: " << category.asUnaryString() << "\n"
       << "  rle  : " << category.asString() << "\n"
       ;
}

static void printCategories(LexHighlighter &hi)
{
  // stop short so I have a waterline
  for (int i=0; i < tde->numLines()-1; i++) {
    printLine(hi, i);
  }
}

static void insert(int line, int col, char const *text)
{
  cout << "insert(" << line << ", " << col << ", "
       << quoted(text) << ")\n";
  tde->moveAbsCursor(TextCoord(line, col));
  tde->insertText(text);
}

static void del(int line, int col, int len)
{
  cout << "del(" << line << ", " << col << ", " << len << ")\n";
  tde->moveAbsCursor(TextCoord(line, col));
  tde->deleteText(len);
}

static void innerCheckLine(LexHighlighter &hi,
                           LexHighlighter &batch, int i)
{
  LineCategories categories1(TC_NORMAL);
  hi.highlight(tde->core(), i, categories1);
  string rendered1 = categories1.asUnaryString();

  LineCategories categories2(TC_NORMAL);
  batch.highlight(tde->core(), i, categories2);
  string rendered2 = categories2.asUnaryString();

  // compare using rendered strings, instead of looking at
  // the run-length ranges, since it's ok if the incrementality
  // somehow gives rise to slightly different ranges (say, in one
  // version there are two adjacent ranges of same-category chars)

  if (rendered1 != rendered2) {
    cout << "check: mismatch at line " << i << ":\n"
         << "  line: " << tde->getWholeLine(i) << "\n"
         << "  inc.: " << rendered1 << "\n"
         << "  bat.: " << rendered2 << "\n"
         ;
    exit(2);
  }
}

// check that the incremental highlighter matches a batch highlighter
static void check(LexHighlighter &hi)
{
  LexHighlighter *batch = makeHigh(tde->core());    // batch because it has no initial info

  // go backwards in hopes of finding more incrementality bugs
  for (int i = tde->numLines()-1; i>=0; i--) {
    innerCheckLine(hi, *batch, i);
  }

  delete batch;
}


static void checkLine(LexHighlighter &hi, int line)
{
  LexHighlighter *batch = makeHigh(tde->core());

  innerCheckLine(hi, *batch, line);

  delete batch;
}


void exerciseHighlighter(MakeHighlighterFunc func)
{
  // at first this was global, then I thought of a problem, and then I
  // forgot what the problem was...
  TextDocumentAndEditor tde_;
  tde = &tde_;

  makeHigh = func;
  LexHighlighter *hi_ = makeHigh(tde->core());
  LexHighlighter &hi = *hi_;

  int line=0, col=0;
  tde->moveAbsCursor(TextCoord(line, col));
  tde->insertText(
    "hi there\n"
    "here is \"a string\" ok?\n"
    "and how about /*a comment*/ yo\n"
    "C++ comment: // I like C++\n"
    "back to int normalcy\n"
  );
  printCategories(hi);
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
  printCategories(hi);
  check(hi);
  printCategories(hi);

  insert(0, 2, "\"");
  del(2, 35, 2);
  insert(4, 2, "Arg");
  printLine(hi, 4);
  check(hi);
  printCategories(hi);

  insert(0, 15, "\\");
  check(hi);
  printCategories(hi);

  insert(2, 30, "*/");
  checkLine(hi, 3);
  check(hi);

  tde = NULL;
  makeHigh = NULL;

  delete hi_;
}


// EOF
