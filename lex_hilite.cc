// lex_hilite.cc
// code for lex_hilite.h

#include "lex_hilite.h"                // this module

// editor
#include "inclexer.h"                  // IncLexer
#include "td-core.h"                   // TextDocumentCore
#include "td-editor.h"                 // TextDocument[And]Editor
#include "textcategory.h"              // LineCategories

// smbase
#include "smbase/exc.h"                // GENERIC_CATCH_BEGIN/END
#include "smbase/sm-file-util.h"       // SMFileUtil
#include "smbase/sm-fstream.h"         // ofstream
#include "smbase/sm-test.h"            // DIAG, EXPECT_EQ
#include "smbase/string-util.h"        // doubleQuote
#include "smbase/strutil.h"            // readLinesFromFile
#include "smbase/trace.h"              // TRACE
#include "smbase/xassert.h"            // xfailure_stringbc

// libc++
#include <memory>                      // std::unique_ptr

// libc
#include <stdlib.h>                    // exit


LexHighlighter::LexHighlighter(TextDocumentCore const &buf, IncLexer &L)
  : buffer(&buf),
    lexer(L),
    savedState(),
    changedBegin(0),
    changedEnd(0),
    waterline(0)
{
  buffer->addObserver(this);

  // all of the saved state is stale
  savedState.insertManyZeroes(LineIndex(0), buf.numLines());

  checkInvar();
}

LexHighlighter::~LexHighlighter()
{
  buffer->removeObserver(this);
}


void LexHighlighter::checkInvar() const
{
  xassert(waterline <= buffer->numLines());

  if (!changedIsEmpty()) {
    xassert(changedBegin < changedEnd);
    xassert(waterline >= changedEnd);

    xassert(changedEnd <= buffer->numLines());
  }
}


void LexHighlighter::addToChanged(LineIndex line)
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
    changedEnd = line.succ();
  }
  else if (line.succ() == changedBegin) {
    // extend changed region up by 1
    --changedBegin;
  }
  else if (line == changedEnd) {
    // extend changed region down by 1
    ++changedEnd;
  }
  else if (line < changedBegin) {
    // line is discontiguous with existing changed region, so absorb
    // the current region into the water and set the new changed region
    // to contain just this line
    waterline = changedBegin;
    changedBegin = line;
    changedEnd = line.succ();
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


LexerState LexHighlighter::getPreviousLineSavedState(LineIndex line) const
{
  if (line.isZero()) {
    return LS_INITIAL;
  }
  else {
    return (LexerState)savedState.get(line.pred());
  }
}


void LexHighlighter::observeInsertLine(TextDocumentCore const &, LineIndex line) NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  // if region ends after 'line', then now it ends one line later
  if (!changedIsEmpty() &&
      changedEnd >= line.succ()) {
    ++changedEnd;
  }

  // similarly for the waterline
  if (waterline >= line) {
    ++waterline;
  }

  addToChanged(line);

  // insert a new saved state, initialized to the state of the
  // line above it
  savedState.insert(line, getPreviousLineSavedState(line));

  GENERIC_CATCH_END
}


void LexHighlighter::observeDeleteLine(TextDocumentCore const &, LineIndex line) NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  // if region ends after 'line', then now it ends one line earlier
  if (!changedIsEmpty() &&
      changedEnd > line) {
    --changedEnd;
  }

  // similarly for waterline
  if (waterline > line) {
    --waterline;
  }

  addToChanged(line);

  // remove a saved state
  savedState.remove(line);

  GENERIC_CATCH_END
}


void LexHighlighter::observeInsertText(TextDocumentCore const &, TextMCoord tc, char const *, ByteCount) NOEXCEPT
{
  GENERIC_CATCH_BEGIN
  addToChanged(tc.m_line);
  GENERIC_CATCH_END
}

void LexHighlighter::observeDeleteText(TextDocumentCore const &, TextMCoord tc, ByteCount) NOEXCEPT
{
  GENERIC_CATCH_BEGIN
  addToChanged(tc.m_line);
  GENERIC_CATCH_END
}


void LexHighlighter::observeTotalChange(TextDocumentCore const &doc) NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  changedBegin = LineIndex(0);
  changedEnd = LineIndex(0);
  waterline = LineIndex(0);

  // all of the saved state is stale
  savedState.clear();
  savedState.insertManyZeroes(LineIndex(0), doc.numLines());

  GENERIC_CATCH_END
}


void LexHighlighter::saveLineState(LineIndex line, LexerState _state)
{
  LineState state = (LineState)_state;
  xassert(state == _state);      // make sure didn't truncate; if did, need to enlarge LineState's representation size

  LineState prev = savedState.get(line);

  if (line >= waterline) {
    savedState.set(line, state);
    if (line == waterline) {
      // push down waterline by 1
      ++waterline;
    }
  }
  else if (changedIsEmpty()) {
    xassert(prev == state);
  }
  else {
    if (line == changedBegin) {
      ++changedBegin;
      savedState.set(line, state);
    }

    if (line.succ() == changedEnd && prev != state) {
      // the state has changed, so we need to re-eval the next line
      if (changedEnd < waterline) {
        ++changedEnd;
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


void LexHighlighter::highlight(
  TextDocumentCore const &buf,
  LineIndex line,
  LineCategories &categories)
{
  xassert(&buf == buffer);

  // push the changed region down to the line of interest
  LexerState prevState = getPreviousLineSavedState(changedBegin);
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
  prevState = getPreviousLineSavedState(waterline);
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
  prevState = getPreviousLineSavedState(line);
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


void printHighlightedLine(TextDocumentCore const &tdc,
                          LexHighlighter &hi, LineIndex line)
{
  LineCategories categories(TC_NORMAL);
  hi.highlight(tdc, line, categories);

  if (verbose) {
    cout << "line " << line << ":\n"
         << "  text : " << tdc.getWholeLineString(line) << "\n"
         << "  catgy: " << categories.asUnaryString() << "\n"
         << "  rle  : " << categories.asString() << "\n"
         ;
  }
}

void printHighlightedLines(TextDocumentCore const &tdc,
                           LexHighlighter &hi)
{
  FOR_EACH_LINE_INDEX_IN(i, tdc) {
    printHighlightedLine(tdc, hi, i);
  }
}


// For convenience, e.g., so I can copy into my expected output after a
// major change, save the entire actual output.
static void dumpActualOutput(ArrayStack<string> const &actualOutputLines)
{
  {
    ofstream out("actual.out");
    xassert(out);

    for (int line=0; line < actualOutputLines.length(); line++) {
      out << actualOutputLines[line] << '\n';
    }
  }

  cout << "wrote full actual output to \"actual.out\"\n";
}


void testHighlighter(LexHighlighter &hi, TextDocumentAndEditor &tde,
                     string inputFname)
{
  // Read the input file into the document.
  tde.writableDoc().replaceWholeFile(SMFileUtil().readFile(inputFname));

  // Work through them, highlighting each line, storing the result
  // in 'actualOutputLines'.
  ArrayStack<string> actualOutputLines;
  FOR_EACH_LINE_INDEX_IN(line, tde) {
    // Highlight the line in model coordinates.
    LineCategories modelCategories(TC_NORMAL);
    hi.highlight(tde.getDocument()->getCore(),
                 line, modelCategories);

    // Convert to layout coordinates.
    LineCategories layoutCategories(TC_NORMAL);
    tde.modelToLayoutSpans(line,
      /*OUT*/ layoutCategories, /*IN*/ modelCategories);

    // Render as a string and add to the output.
    actualOutputLines.push(layoutCategories.asUnaryString());
  }

  try {
    // Read the expected output.
    ArrayStack<string> expectedOutputLines;
    readLinesFromFile(expectedOutputLines, stringb(inputFname << ".hi"));

    // The number of lines must agree.
    EXPECT_EQ(actualOutputLines.length(), expectedOutputLines.length());

    // Now compare them line by line so we can identify where the mismatch
    // is if there is one.
    for (LineIndex line(0);
         line < LineCount(actualOutputLines.length());
         ++line) {
      // Compare the highlighted results.
      string actual = actualOutputLines[line.get()];
      string expect = expectedOutputLines[line.get()];
      if (actual != expect) {
        xfailure_stringbc("testHighlighter failure:\n" <<
          "  index : " << line << "\n" <<
          "  line  : " << tde.getWholeLineString(line) << "\n" <<
          "  expect: " << expect << "\n" <<
          "  actual: " << actual);
      }
    }
  }
  catch (...) {
    dumpActualOutput(actualOutputLines);
    cout << "failing input file name: " << inputFname << endl;
    throw;
  }
}


// ---------------------- test code -------------------------
static MakeHighlighterFunc makeHigh;
static TextDocumentEditor *tde;

static void printLine(LexHighlighter &hi, int line)
{
  printHighlightedLine(tde->getDocument()->getCore(), hi, LineIndex(line));
}

static void printCategories(LexHighlighter &hi)
{
  printHighlightedLines(tde->getDocument()->getCore(), hi);
}

static void insert(int line, int col, char const *text)
{
  DIAG("insert(" << line << ", " << col << ", " <<
       doubleQuote(text) << ")");
  tde->setCursor(TextLCoord(LineIndex(line), col));
  tde->insertNulTermText(text);
}

static void del(int line, int col, int len)
{
  DIAG("del(" << line << ", " << col << ", " << len << ")");
  tde->setCursor(TextLCoord(LineIndex(line), col));
  tde->deleteTextBytes(ByteCount(len));
}

static void innerCheckLine(LexHighlighter &hi,
                           LexHighlighter &batch, int i)
{
  LineCategories categories1(TC_NORMAL);
  hi.highlightTDE(tde, LineIndex(i), categories1);
  string rendered1 = categories1.asUnaryString();

  LineCategories categories2(TC_NORMAL);
  batch.highlightTDE(tde, LineIndex(i), categories2);
  string rendered2 = categories2.asUnaryString();

  // compare using rendered strings, instead of looking at
  // the run-length ranges, since it's ok if the incrementality
  // somehow gives rise to slightly different ranges (say, in one
  // version there are two adjacent ranges of same-category chars)

  if (rendered1 != rendered2) {
    cout << "check: mismatch at line " << i << ":\n"
         << "  line: " << tde->getWholeLineString(LineIndex(i)) << "\n"
         << "  inc.: " << rendered1 << "\n"
         << "  bat.: " << rendered2 << "\n"
         ;
    exit(2);
  }
}

// check that the incremental highlighter matches a batch highlighter
static void check(LexHighlighter &hi)
{
  // batch because it has no initial info
  std::unique_ptr<LexHighlighter> batch(
    makeHigh(tde->getDocument()->getCore()));

  // go backwards in hopes of finding more incrementality bugs
  for (int i = tde->numLines().get() - 1; i>=0; i--) {
    innerCheckLine(hi, *batch, i);
  }
}


static void checkLine(LexHighlighter &hi, int line)
{
  std::unique_ptr<LexHighlighter> batch(
    makeHigh(tde->getDocument()->getCore()));

  innerCheckLine(hi, *batch, line);
}


void exerciseHighlighter(MakeHighlighterFunc func)
{
  // at first this was global, then I thought of a problem, and then I
  // forgot what the problem was...
  TextDocumentAndEditor tde_;
  tde = &tde_;

  makeHigh = func;
  std::unique_ptr<LexHighlighter> hi_(
    makeHigh(tde->getDocument()->getCore()));
  LexHighlighter &hi = *hi_;

  int line=0, col=0;
  tde->setCursor(TextLCoord(LineIndex(line), col));
  tde->insertNulTermText(
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
}


// EOF
