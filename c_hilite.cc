// c_hilite.cc
// code for c_hilite.h

#include "c_hilite.h"      // this module
#include "style.h"         // LineStyle
#include "buffer.h"        // BufferCore, Buffer


C_Highlighter::C_Highlighter(BufferCore const &buf)
  : buffer(buf),
    lexer(),
    savedState(),
    changedBegin(0),
    changedEnd(0)
{
  buffer.observers.append(this);

  // all of the saved state is stale
  savedState.insertManyZeroes(0, buf.numLines());
  changedEnd = buf.numLines();
}

C_Highlighter::~C_Highlighter()
{
  buffer.observers.removeItem(this);
}


void C_Highlighter::addToChanged(int line)
{
  if (changedIsEmpty()) {
    changedBegin = line;
    changedEnd = line+1;
  }
  else {
    // changed region starts with 'line', if not earlier
    changedBegin = min(changedBegin, line);

    // region ends with 'line', if not later
    changedEnd = max(changedEnd, line+1);
  }
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

  if (changedIsEmpty()) {
    xassert(prev == state);
  }
  else {
    if (line == changedBegin) {
      changedBegin++;
      savedState.set(line, state);
    }

    if (line == changedEnd && prev != state) {
      // the state has changed, so we need to re-eval the next line
      changedEnd++;
    }
  }
}

void C_Highlighter::highlight(BufferCore const &buf, int line, LineStyle &style)
{
  xassert(&buf == &buffer);

  // push the changed region down to the line of interest
  int prevState = getSavedState(changedBegin-1);
  while (!changedIsEmpty() &&
         changedBegin < line) {
    lexer.beginScan(&buf, changedBegin, prevState);

    int len, code;
    while (lexer.getNextToken(len, code))
      {}

    prevState = lexer.getState();

    // this increments 'changedBegin', and if the state doesn't
    // change then it won't increment 'changedEnd', so we might
    // exit the loop due to changedIsEmpty() becoming true
    saveLineState(changedBegin, prevState);
  }

  // recall the saved state for the line of interest
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

void entry()
{
  Buffer buf;
  int line=0, col=0;
  buf.insertTextRange(line, col,
    "hi there\n"
    "here is \"a string\" ok?\n"
    "and how about /*a comment*/ yo\n"
    "C++ comment: // I like C++\n"
    "back to normalcy\n"
  );
  
  C_Highlighter hi(buf);
  for (int i=0; i<line; i++) {
    LineStyle style(ST_NORMAL);
    hi.highlight(buf, i, style);
    
    cout << "line " << i << ":\n"
         << "  text : " << buf.getTextRange(i, 0, i, buf.lineLength(i)) << "\n"
         << "  style: " << style.asString() << endl;
  }
}

USUAL_MAIN

#endif //  TEST_C_HILITE
