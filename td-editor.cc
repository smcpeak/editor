// td-editor.cc
// code for td-editor.h

#include "td-editor.h"                 // this module

// editor
#include "editor-strutil.h"            // cIdentifierAt
#include "justify.h"                   // justifyNearLine

// smbase
#include "array.h"                     // Array
#include "datetime.h"                  // DateTimeSeconds
#include "objcount.h"                  // CHECK_OBJECT_COUNT
#include "sm-swap.h"                   // swap
#include "trace.h"                     // TRACE
#include "typ.h"                       // min, max


int TextDocumentEditor::s_objectCount = 0;

CHECK_OBJECT_COUNT(TextDocumentEditor);


TextDocumentEditor::TextDocumentEditor(TextDocument *doc)
  : m_doc(doc),
    m_cursor(),
    m_markActive(false),
    m_mark(),
    m_firstVisible(),

    // This size isn't intended to be user-visible since the client
    // code ought to set the size.  But it does get used by the tests,
    // where I want a small size in order to incidentally exercise the
    // scrolling code.  Tests that actually check scrolling should set
    // their own size though.
    m_lastVisible(4, 9)
{
  xassert(doc);
  selfCheck();

  TextDocumentEditor::s_objectCount++;
  TRACE("TextDocumentEditor",
    "created TDE at " << (void*)this <<
    ", doc=" << (void*)m_doc <<
    ", oc=" << s_objectCount);
}


TextDocumentEditor::~TextDocumentEditor()
{
  TextDocumentEditor::s_objectCount--;
  TRACE("TextDocumentEditor",
    "destroyed TDE at " << (void*)this <<
    ", doc=" << (void*)m_doc <<
    ", oc=" << s_objectCount);
}


void TextDocumentEditor::selfCheck() const
{
  xassert(m_cursor.nonNegative());
  if (m_markActive) {
    xassert(m_mark.nonNegative());
  }
  xassert(m_firstVisible.nonNegative());
  xassert(m_firstVisible.m_line <= m_lastVisible.m_line);
  xassert(m_firstVisible.m_column <= m_lastVisible.m_column);
}


TextMCoord TextDocumentEditor::innerToMCoord(TextCoord lc) const
{
  if (lc.m_line < 0) {
    return this->beginMCoord();
  }

  if (lc.m_line >= this->numLines()) {
    return this->endMCoord();
  }

  if (lc.m_column < 0) {
    return TextMCoord(lc.m_line, 0);
  }

  // TODO: Layout.
  int lengthBytes = this->lineLengthBytes(lc.m_line);
  if (lc.m_column > lengthBytes) {
    return TextMCoord(lc.m_line, lengthBytes);
  }
  else {
    return TextMCoord(lc.m_line, lc.m_column);
  }
}

TextMCoord TextDocumentEditor::toMCoord(TextCoord lc) const
{
  TextMCoord mc(this->innerToMCoord(lc));
  xassert(this->validMCoord(mc));
  return mc;
}


TextCoord TextDocumentEditor::toLCoord(TextMCoord mc) const
{
  // TODO: Layout.
  return TextCoord(mc.m_line, mc.m_byteIndex);
}


TextMCoordRange TextDocumentEditor::toMCoordRange(
  TextCoordRange const &range) const
{
  return TextMCoordRange(
    this->toMCoord(range.m_start),
    this->toMCoord(range.m_end));
}


TextCoordRange TextDocumentEditor::toLCoordRange(
  TextMCoordRange const &range) const
{
  return TextCoordRange(
    this->toLCoord(range.m_start),
    this->toLCoord(range.m_end));
}


int TextDocumentEditor::lineLengthColumns(int line) const
{
  if (0 <= line && line < numLines()) {
    return this->lineEndCoord(line).m_column;
  }
  else {
    return 0;
  }
}


TextCoord TextDocumentEditor::lineEndCoord(int line) const
{
  return this->toLCoord(m_doc->lineEndCoord(line));
}


int TextDocumentEditor::maxLineLengthColumns() const
{
  // TODO: Layout.
  return m_doc->maxLineLengthBytes();
}


TextCoord TextDocumentEditor::endCoord() const
{
  return this->toLCoord(this->endMCoord());
}


bool TextDocumentEditor::cursorAtEnd() const
{
  return m_cursor == this->endCoord();
}


void TextDocumentEditor::setCursor(TextCoord c)
{
  xassert(c.nonNegative());
  m_cursor = c;
}


void TextDocumentEditor::setMark(TextCoord m)
{
  xassert(m.nonNegative());
  m_mark = m;
  m_markActive = true;
}


void TextDocumentEditor::moveMarkBy(int deltaLine, int deltaCol)
{
  xassert(m_markActive);
  m_mark.m_line = max(0, m_mark.m_line + deltaLine);
  m_mark.m_column = max(0, m_mark.m_column + deltaCol);
}



void TextDocumentEditor::turnOnSelection()
{
  if (!m_markActive) {
    this->setMark(m_cursor);
  }
}


void TextDocumentEditor::turnOffSelectionIfEmpty()
{
  if (m_markActive && m_mark == m_cursor) {
    this->clearMark();
  }
}


void TextDocumentEditor::selectCursorLine()
{
  // Move the cursor to the start of its line.
  this->setCursorColumn(0);

  // Make the selection end at the start of the next line.
  this->setMark(TextCoord(this->cursor().m_line + 1, 0));
}


TextCoordRange TextDocumentEditor::getSelectRange() const
{
  if (!m_markActive) {
    return TextCoordRange(m_cursor, m_cursor);
  }
  else {
    return TextCoordRange(m_cursor, m_mark).rectified();
  }
}


TextMCoordRange TextDocumentEditor::getSelectModelRange() const
{
  return this->toMCoordRange(this->getSelectRange());
}


void TextDocumentEditor::setSelectRange(TextCoordRange const &range)
{
  this->setCursor(range.m_start);
  this->setMark(range.m_end);
}


string TextDocumentEditor::getSelectedText() const
{
  if (!m_markActive) {
    return "";
  }
  else {
    return this->getTextRange(this->getSelectRange());
  }
}


string TextDocumentEditor::getSelectedOrIdentifier() const
{
  if (m_markActive) {
    return this->getSelectedText();
  }

  string text = this->getWholeLine(m_cursor.m_line);
  return cIdentifierAt(text, m_cursor.m_column);
}


void TextDocumentEditor::swapCursorAndMark()
{
  if (m_markActive) {
    TextCoord tmp(m_mark);
    m_mark = m_cursor;
    m_cursor = tmp;
  }
}


void TextDocumentEditor::normalizeCursorGTEMark()
{
  if (m_markActive && m_mark > m_cursor) {
    this->swapCursorAndMark();
  }
}


void TextDocumentEditor::setFirstVisible(TextCoord fv)
{
  xassert(fv.nonNegative());
  int h = m_lastVisible.m_line - m_firstVisible.m_line;
  int w = m_lastVisible.m_column - m_firstVisible.m_column;
  m_firstVisible = fv;
  m_lastVisible.m_line = fv.m_line + h;
  m_lastVisible.m_column = fv.m_column + w;
}


void TextDocumentEditor::moveFirstVisibleBy(int deltaLine, int deltaCol)
{
  int line = max(0, m_firstVisible.m_line + deltaLine);
  int col = max(0, m_firstVisible.m_column + deltaCol);
  this->setFirstVisible(TextCoord(line, col));
}


void TextDocumentEditor::moveFirstVisibleAndCursor(int deltaLine, int deltaCol)
{
  TRACE("moveFirstVisibleAndCursor",
        "start: firstVis=" << m_firstVisible
     << ", cursor=" << m_cursor
     << ", delta=" << TextCoord(deltaLine, deltaCol));

  // first make sure the view contains the cursor
  this->scrollToCursor();

  // move viewport, but remember original so we can tell
  // when there's truncation
  int origVL = m_firstVisible.m_line;
  int origVC = m_firstVisible.m_column;
  this->moveFirstVisibleBy(deltaLine, deltaCol);

  // now move cursor by the amount that the viewport moved
  this->moveCursorBy(m_firstVisible.m_line - origVL,
                     m_firstVisible.m_column - origVC);

  TRACE("moveFirstVisibleAndCursor",
        "end: firstVis=" << m_firstVisible <<
        ", cursor=" << m_cursor);
}


void TextDocumentEditor::setVisibleSize(int lines, int columns)
{
  // If the user resizes the window down to nothing, we might calculate
  // a visible region with zero width.  Just force it to be
  // non-negative.
  lines = max(lines, 1);
  columns = max(columns, 1);
  m_lastVisible.m_line = m_firstVisible.m_line + lines - 1;
  m_lastVisible.m_column = m_firstVisible.m_column + columns - 1;
}


// For a particular dimension, return the new start coordinate
// of the viewport.
static int stcHelper(int firstVis, int lastVis, int cur, int gap)
{
  int width = lastVis - firstVis + 1;

  bool center = false;
  if (gap == -1) {
    center = true;
    gap = 0;
  }
  else if (width+1 < gap*2) {
    return max(0, cur - width/2);
  }

  bool changed = false;
  if (cur-gap < firstVis) {
    firstVis = max(0, cur-gap);
    changed = true;
  }
  else if (cur+gap > lastVis) {
    firstVis += cur+gap - lastVis;
    changed = true;
  }

  if (changed && center) {
    // we had to adjust the viewport; make it actually centered
    firstVis = max(0, cur - width/2);
  }

  return firstVis;
}

void TextDocumentEditor::scrollToCoord(TextCoord tc, int edgeGap)
{
  int fvline = stcHelper(this->firstVisible().m_line,
                         this->lastVisible().m_line,
                         tc.m_line,
                         edgeGap);

  int fvcol = stcHelper(this->firstVisible().m_column,
                        this->lastVisible().m_column,
                        tc.m_column,
                        edgeGap);

  setFirstVisible(TextCoord(fvline, fvcol));
}


void TextDocumentEditor::centerVisibleOnCursorLine()
{
  int newfv = max(0, m_cursor.m_line - this->visLines()/2);
  this->setFirstVisible(TextCoord(newfv, 0));
  this->scrollToCursor();
}


void TextDocumentEditor::moveCursor(bool relLine, int line, bool relCol, int col)
{
  if (relLine) {
    m_cursor.m_line += line;
  }
  else {
    m_cursor.m_line = line;
  }
  xassert(m_cursor.m_line >= 0);

  if (relCol) {
    m_cursor.m_column += col;
  }
  else {
    m_cursor.m_column = col;
  }
  xassert(m_cursor.m_column >= 0);
}


TextCoord TextDocumentEditor::mark() const
{
  xassert(markActive());
  return m_mark;
}


void TextDocumentEditor::insertText(char const *text, int textLen)
{
  this->deleteSelectionIf();
  this->fillToCursor();

  m_doc->insertAt(this->toMCoord(this->cursor()), text, textLen);

  // Put the cursor at the end of the inserted text.
  this->walkCursor(textLen);

  this->scrollToCursor();
}


void TextDocumentEditor::insertString(string text)
{
  // This does not actually prevent problems with embedded NULs
  // because I am still using my own string class which does not
  // handle them.  But at some point I will switch to std::string,
  // and then this will work as intended.
  this->insertText(text.c_str(), text.length());
}


void TextDocumentEditor::deleteLR(bool left, int columnCount)
{
  TextCoord start(this->cursor());

  TextCoord end(start);
  this->walkCoordColumns(end, left? -columnCount : +columnCount);

  TextCoordRange range(start, end);
  range.rectify();

  this->deleteTextRange(range);
}


void TextDocumentEditor::deleteSelection()
{
  xassert(m_markActive);

  TextCoordRange range(this->getSelectRange());
  if (range.m_start < this->endCoord()) {
    this->fillToCoord(range.m_start);
  }

  this->deleteTextRange(range);
  this->clearMark();
  this->scrollToCursor();
}


void TextDocumentEditor::backspaceFunction()
{
  if (m_markActive) {
    this->deleteSelection();
  }
  else if (m_cursor.m_column == 0) {
    if (m_cursor.m_line == 0) {
      // BOF, do nothing.
    }
    else if (m_cursor.m_line > m_doc->numLines() - 1) {
      // Move cursor up non-destructively.
      this->moveCursorBy(-1, 0);
    }
    else {
      // Move to end of previous line.
      this->moveToPrevLineEnd();

      // Splice them together.
      this->deleteChar();
    }
  }
  else if (m_cursor.m_column > this->cursorLineLengthColumns()) {
    // Move cursor left non-destructively.
    this->moveCursorBy(0, -1);
  }
  else {
    // Remove the character to the left of the cursor.
    this->deleteLR(true /*left*/, 1);
  }

  this->scrollToCursor();
}


void TextDocumentEditor::deleteKeyFunction()
{
  if (m_markActive) {
    this->deleteSelection();
  }
  else if (m_cursor >= this->endCoord()) {
    // Beyond EOF, do nothing.
  }
  else {
    // Add spaces if beyond EOF.
    this->fillToCursor();

    // Delete next char, which might be newline.
    this->deleteChar();
  }

  // No need to scroll; deleteSelection scrolls, other cases do not move.
}


void TextDocumentEditor::undo()
{
  this->setCursor(this->toLCoord(m_doc->undo()));
  this->clearMark();
  this->scrollToCursor();
}


void TextDocumentEditor::redo()
{
  this->setCursor(this->toLCoord(m_doc->redo()));
  this->clearMark();
  this->scrollToCursor();
}


void TextDocumentEditor::truncateCoord(TextCoord &tc) const
{
  tc.m_line = max(0, tc.m_line);
  tc.m_column = max(0, tc.m_column);

  if (tc.m_line >= this->numLines()) {
    // Beyond EOF.  Go to end of document *without* preserving
    // the column.
    tc = this->endCoord();
  }
  else {
    tc.m_column = min(tc.m_column, this->lineLengthColumns(tc.m_line));
  }
}


void TextDocumentEditor::walkCoordColumns(TextCoord &tc, int len) const
{
  xassert(tc.nonNegative());

  for (; len > 0; len--) {
    if (tc.m_column >= this->lineLengthColumns(tc.m_line)) {
      // cycle to next line
      tc.m_line++;
      tc.m_column = 0;
    }
    else {
      tc.m_column++;
    }
  }

  for (; len < 0; len++) {
    if (tc.m_column == 0) {
      // cycle up to end of preceding line
      if (tc.m_line == 0) {
        return;      // Stop at BOF.
      }
      tc.m_line--;
      tc.m_column = this->lineLengthColumns(tc.m_line);
    }
    else {
      tc.m_column--;
    }
  }
}


void TextDocumentEditor::getLineLayout(TextCoord tc, char *dest, int destLen) const
{
  xassert(tc.nonNegative());

  // How many columns of the source are in the defined region?
  int undef = destLen;
  int def = 0;

  if (tc.m_line < numLines() &&
      tc.m_column < lineLengthColumns(tc.m_line)) {
    //       <----- lineLength -------->
    // line: [-------------------------][..spaces...]
    //       <------ col -----><----- destLen ------>
    //                         <- def -><-- undef -->
    // dest:                   [--------------------]

    undef = max(0, (tc.m_column+destLen) - lineLengthColumns(tc.m_line));
    def = max(0, destLen - undef);
  }

  // initial part in defined region
  if (def) {
    TextCoord tcEnd(tc.m_line, tc.m_column + def);

    // Express the desired range in model coordinates.
    TextMCoord mcBegin(this->toMCoord(tc));
    TextMCoord mcEnd(this->toMCoord(tcEnd));
    xassert(mcBegin.m_line == mcEnd.m_line);
    int byteCount = mcEnd.m_byteIndex - mcBegin.m_byteIndex;

    // TODO: Layout: Properly translate between bytes and grid cells.
    // For now I'm continuing to equate them.
    xassert(byteCount == def);
    m_doc->getLine(mcBegin, dest, byteCount);
  }

  // spaces past defined region
  memset(dest+def, ' ', undef);
}


string TextDocumentEditor::getTextRange(TextCoordRange const &range) const
{
  xassert(range.nonNegative());
  xassert(range.isRectified());

  return m_doc->getTextRange(this->toMCoordRange(range));
}


string TextDocumentEditor::getWholeLine(int line) const
{
  xassert(line >= 0);
  if (line < m_doc->numLines()) {
    return m_doc->getWholeLine(line);
  }
  else {
    return string("");
  }
}


string TextDocumentEditor::getWordAfter(TextCoord tc) const
{
  stringBuilder sb;

  if (!( 0 <= tc.m_line && tc.m_line < numLines() )) {
    return "";
  }

  bool seenWordChar = false;
  while (tc.m_column < lineLengthColumns(tc.m_line)) {
    char ch = getTextRange(tc, TextCoord(tc.m_line, tc.m_column+1))[0];
    if (isalnum(ch) || ch=='_') {
      seenWordChar = true;
      sb << ch;
    }
    else if (seenWordChar) {
      // done, this is the end of the word
      break;
    }
    else {
      // consume this character, it precedes any word characters
      sb << ch;
    }
    tc.m_column++;
  }

  return sb;
}


int TextDocumentEditor::countLeadingSpacesTabs(int line) const
{
  xassert(line >= 0);
  if (line >= m_doc->numLines()) {
    return 0;
  }
  else {
    return m_doc->countLeadingSpacesTabs(line);
  }
}


int TextDocumentEditor::countTrailingSpacesTabs(int line) const
{
  xassert(line >= 0);
  if (line >= m_doc->numLines()) {
    return 0;
  }
  else {
    return m_doc->countTrailingSpacesTabs(line);
  }
}


int TextDocumentEditor::getIndentation(int line) const
{
  xassert(line >= 0);
  if (line >= m_doc->numLines()) {
    return -1;
  }
  else {
    int lineLen = m_doc->lineLengthBytes(line);
    int leading = m_doc->countLeadingSpacesTabs(line);
    if (lineLen == leading) {
      return -1;        // entirely whitespace
    }
    else {
      return leading;
    }
  }
}


int TextDocumentEditor::getAboveIndentation(int line) const
{
  while (line >= 0) {
    int ind = getIndentation(line);
    if (ind >= 0) {
      return ind;
    }
    line--;
  }
  return 0;
}


// ---------------- TextDocumentEditor: modifications ------------------

void TextDocumentEditor::moveCursorBy(int deltaLine, int deltaCol)
{
  // prevent moving into negative territory
  deltaLine = max(deltaLine, - cursor().m_line);
  deltaCol = max(deltaCol, - cursor().m_column);

  if (deltaLine || deltaCol) {
    moveCursor(true /*relLine*/, deltaLine,
               true /*relCol*/, deltaCol);
  }
}


void TextDocumentEditor::setCursorColumn(int newCol)
{
  moveCursor(true /*relLine*/, 0,
             false /*relCol*/, newCol);
}


void TextDocumentEditor::moveToNextLineStart()
{
  moveCursor(true /*relLine*/, +1,
             false /*relCol*/, 0);
}

void TextDocumentEditor::moveToPrevLineEnd()
{
  int prevLine = max(0, cursor().m_line - 1);
  moveCursor(false /*relLine*/, prevLine,
             false /*relCol*/, lineLengthColumns(prevLine));
}


void TextDocumentEditor::moveCursorToTop()
{
  setCursor(this->beginCoord());
  scrollToCursor();
}

void TextDocumentEditor::moveCursorToBottom()
{
  setCursor(TextCoord(m_doc->numLines() - 1, 0));
  scrollToCursor();
}


void TextDocumentEditor::advanceWithWrap(bool backwards)
{
  int line = cursor().m_line;
  int col = cursor().m_column;

  if (!backwards) {
    if (0 <= line &&
        line < numLines() &&
        col < cursorLineLengthColumns()) {
      moveCursorBy(0, 1);
    }
    else {
      moveToNextLineStart();
    }
  }

  else {
    if (0 <= line &&
        line < numLines() &&
        col > 0) {
      moveCursorBy(0, -1);
    }
    else if (line > 0) {
      moveToPrevLineEnd();
    }
    else {
      // cursor at buffer start.. do nothing, I guess
    }
  }
}


void TextDocumentEditor::confineCursorToVisible()
{
  m_cursor.m_line =
    max(m_firstVisible.m_line,
      min(m_lastVisible.m_line,   m_cursor.m_line));

  m_cursor.m_column =
    max(m_firstVisible.m_column,
      min(m_lastVisible.m_column, m_cursor.m_column));
}


void TextDocumentEditor::walkCursor(int distance)
{
  TextCoord tc = m_cursor;
  this->walkCoordColumns(tc, distance);
  m_cursor = tc;
}


// Given a coordinate that might be outside the buffer area (but must
// both be nonnegative), compute how many rows and spaces need to
// be added (to EOF, and 'tc.line', respectively) so that coordinate
// will be in the defined area.
static void computeSpaceFill(TextDocumentEditor const &tde, TextCoord tc,
                             int &rowfill, int &colfill)
{
  if (tc.m_line < tde.numLines()) {
    // case 1: only need to add spaces to the end of some line
    int diff = tc.m_column - tde.lineLengthColumns(tc.m_line);
    if (diff < 0) {
      diff = 0;
    }
    rowfill = 0;
    colfill = diff;
  }

  else {
    // case 2: need to add lines, then possibly add spaces
    rowfill = (tc.m_line - tde.numLines() + 1);    // # of lines to add
    colfill = tc.m_column;                         // # of cols to add
  }

  xassert(rowfill >= 0);
  xassert(colfill >= 0);
}

void TextDocumentEditor::fillToCoord(TextCoord const &tc)
{
  int rowfill, colfill;
  computeSpaceFill(*this, tc, rowfill, colfill);

  if (rowfill==0 && colfill==0) {
    return;     // nothing to do
  }

  // Restore cursor and scroll state afterwards.
  CursorRestorer restorer(*this);

  // Move cursor the end of the 'tc' line.
  moveCursor(false /*relLine*/, tc.m_line - rowfill,
             false /*relCol*/, lineLengthColumns(tc.m_line - rowfill));

  // Do not delete things implicitly here due to a selection!
  clearMark();

  // add newlines
  while (rowfill--) {
    insertNulTermText("\n");
  }

  // add spaces
  while (colfill--) {
    insertSpace();
  }

  // Cursor should have ended up at 'tc'.
  xassert(tc == cursor());

  // Now it will be restored by 'restorer'.
}


void TextDocumentEditor::insertSpaces(int howMany)
{
  // simple for now
  while (howMany--) {
    insertSpace();
  }
}


void TextDocumentEditor::insertNewline()
{
  int overEdge = cursor().m_column - cursorLineLengthColumns();
  if (overEdge > 0) {
    // move back to the end of this line
    moveCursorBy(0, -overEdge);
  }

  insertNulTermText("\n");
}


void TextDocumentEditor::insertNewlineAutoIndent()
{
  // The code below this assumes cursor > mark if mark is active.
  this->normalizeCursorGTEMark();

  // Will we be carrying text forward onto the new line?
  bool hadCharsToRight = (m_cursor.m_column < this->cursorLineLengthColumns());

  // typing replaces selection
  this->deleteSelectionIf();

  // Actually insert the newline character.  This will scroll
  // to the left edge too.
  this->insertNewline();

  // auto-indent
  int ind = this->getAboveIndentation(m_cursor.m_line - 1);
  if (hadCharsToRight) {
    // Insert spaces so the carried forward text starts
    // in the auto-indent column.
    this->insertSpaces(ind);
  }
  else {
    // Move the cursor to the auto-indent column but do not
    // fill with spaces.  This way I can press Enter more
    // than once without adding lots of spaces.
    this->moveCursorBy(0, ind);
  }

  this->scrollToCursor();
}


void TextDocumentEditor::deleteTextRange(TextCoordRange const &range)
{
  xassert(range.isRectified());
  xassert(range.nonNegative());

  m_doc->deleteTextRange(this->toMCoordRange(range));

  // Set cursor per spec.
  this->setCursor(range.m_start);
  this->clearMark();
}


void TextDocumentEditor::indentLines(int start, int lines, int ind)
{
  CursorRestorer cr(*this);

  // Don't let the selection interfere with the text insertions below.
  this->clearMark();

  for (int line=start; line < start+lines &&
                       line < this->numLines(); line++) {
    this->setCursor(TextCoord(line, 0));

    if (ind > 0) {
      if (this->isEmptyLine(line)) {
        // Do not add spaces to a blank line.
      }
      else {
        for (int i=0; i<ind; i++) {
          this->insertSpace();
        }
      }
    }

    else {
      int lineInd = this->countLeadingSpacesTabs(line);
      for (int i=0; i<(-ind) && i<lineInd; i++) {
        this->deleteChar();
      }
    }
  }
}


bool TextDocumentEditor::blockIndent(int amt)
{
  if (!this->markActive()) {
    return false;
  }

  TextCoordRange range = this->getSelectRange();

  int endLine = (range.m_end.m_column==0? range.m_end.m_line - 1 : range.m_end.m_line);
  this->indentLines(range.m_start.m_line, endLine - range.m_start.m_line + 1, amt);

  return true;
}


bool TextDocumentEditor::justifyNearCursor(int desiredWidth)
{
  bool ret = justifyNearLine(*this, m_cursor.m_line, desiredWidth);
  this->scrollToCursor();
  return ret;
}


void TextDocumentEditor::insertDateTime(DateTimeProvider *provider)
{
  DateTimeSeconds d;
  d.fromCurrentTime(provider);
  string dt = stringf("%04d-%02d-%02d %02d:%02d",
    d.year,
    d.month,
    d.day,
    d.hour,
    d.minute);

  this->insertString(dt);
}


string TextDocumentEditor::clipboardCopy()
{
  string sel = this->getSelectedText();

  // Un-highlight the selection, which is what emacs does and
  // I'm now used to.
  this->clearMark();

  return sel;
}


string TextDocumentEditor::clipboardCut()
{
  string sel = this->getSelectedText();
  this->deleteSelectionIf();
  return sel;
}


void TextDocumentEditor::clipboardPaste(char const *text, int textLen)
{
  this->insertText(text, textLen);
}


void TextDocumentEditor::debugPrint() const
{
  m_doc->getCore().dumpRepresentation();
  cout << "  cursor: " << m_cursor << endl;
  cout << "  markActive: " << m_markActive << endl;
  cout << "  mark: " << m_mark << endl;
  cout << "  firstVisible: " << m_firstVisible << endl;
  cout << "  lastVisible: " << m_lastVisible << endl;
}


// -------------------- CursorRestorer ------------------
CursorRestorer::CursorRestorer(TextDocumentEditor &e)
  : tde(&e),
    cursor(e.cursor()),
    markActive(e.markActive()),
    mark(markActive? e.mark() : TextCoord()),
    firstVisible(e.firstVisible())
{}

CursorRestorer::~CursorRestorer() NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  tde->setCursor(cursor);
  if (markActive) {
    tde->setMark(mark);
  }
  else {
    tde->clearMark();
  }
  tde->setFirstVisible(firstVisible);

  GENERIC_CATCH_END
}


// --------------------- UndoHistoryGrouper ---------------
UndoHistoryGrouper::UndoHistoryGrouper(TextDocumentEditor &e)
  : m_editor(&e)
{
  m_editor->beginUndoGroup();
}

UndoHistoryGrouper::~UndoHistoryGrouper() NOEXCEPT
{
  GENERIC_CATCH_BEGIN
  m_editor->endUndoGroup();
  GENERIC_CATCH_END
}


// EOF
