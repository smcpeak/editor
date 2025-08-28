// td-editor.cc
// code for td-editor.h

#include "td-editor.h"                 // this module

// editor
#include "editor-strutil.h"            // cIdentifierAt
#include "justify.h"                   // justifyNearLine
#include "line-difference.h"           // LineDifference

// smbase
#include "smbase/array.h"              // Array
#include "smbase/chained-cond.h"       // smbase::cc::{le_le, lt_le}
#include "smbase/codepoint.h"          // isSpaceOrTab
#include "smbase/datetime.h"           // DateTimeSeconds
#include "smbase/objcount.h"           // CHECK_OBJECT_COUNT
#include "smbase/sm-swap.h"            // swap
#include "smbase/trace.h"              // TRACE

// libc++
#include <algorithm>                   // std::{min, max}

using namespace smbase;


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
    m_lastVisible(LineIndex(4), 9),

    m_tabWidth(8)
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

  m_doc->selfCheck();
}


void TextDocumentEditor::setTabWidth(int tabWidth)
{
  xassert(tabWidth > 0);
  m_tabWidth = tabWidth;
}


TextMCoord TextDocumentEditor::innerToMCoord(TextLCoord lc) const
{
  if (!( lc.m_line < this->numLines() )) {
    return this->endMCoord();
  }

  if (lc.m_column < 0) {
    return TextMCoord(lc.m_line, 0);
  }

  LineIterator it(*this, lc.m_line);
  for (; it.has() && it.columnOffset() < lc.m_column; it.advByte()) {
    // Nothing.
  }

  return TextMCoord(lc.m_line, it.byteOffset());
}

TextMCoord TextDocumentEditor::toMCoord(TextLCoord lc) const
{
  TextMCoord mc(this->innerToMCoord(lc));
  xassert(this->validMCoord(mc));
  return mc;
}


TextLCoord TextDocumentEditor::toLCoord(TextMCoord mc) const
{
  LineIterator it(*this, mc.m_line);
  for (; it.has() && it.byteOffset() < mc.m_byteIndex; it.advByte()) {
    // Nothing.
  }

  // The input byte index must have been valid.
  xassert(it.byteOffset() == mc.m_byteIndex);

  return TextLCoord(mc.m_line, it.columnOffset());
}


TextMCoordRange TextDocumentEditor::toMCoordRange(
  TextLCoordRange const &range) const
{
  return TextMCoordRange(
    this->toMCoord(range.m_start),
    this->toMCoord(range.m_end));
}


TextLCoordRange TextDocumentEditor::toLCoordRange(
  TextMCoordRange const &range) const
{
  return TextLCoordRange(
    this->toLCoord(range.m_start),
    this->toLCoord(range.m_end));
}


int TextDocumentEditor::lineLengthColumns(LineIndex line) const
{
  if (line < numLines()) {
    return this->lineEndLCoord(line).m_column;
  }
  else {
    return 0;
  }
}


TextLCoord TextDocumentEditor::lineEndLCoord(LineIndex line) const
{
  if (line < numLines()) {
    return this->toLCoord(m_doc->lineEndCoord(line));
  }
  else {
    return TextLCoord(line, 0);
  }
}


int TextDocumentEditor::maxLineLengthColumns() const
{
  // TODO: Layout.
  return m_doc->maxLineLengthBytes();
}


TextLCoord TextDocumentEditor::endLCoord() const
{
  return this->toLCoord(this->endMCoord());
}


bool TextDocumentEditor::cursorAtLineEnd() const
{
  return this->cursorOnModelCoord() &&
         this->toMCoord(m_cursor) == this->lineEndMCoord(m_cursor.m_line);
}


bool TextDocumentEditor::cursorAtEnd() const
{
  return m_cursor == this->endLCoord();
}


void TextDocumentEditor::setCursor(TextLCoord c)
{
  TRACE("TextDocumentEditor", "setCursor(" << c << ")");

  xassert(c.nonNegative());
  m_cursor = c;
}


void TextDocumentEditor::setMark(TextLCoord m)
{
  xassert(m.nonNegative());
  m_mark = m;
  m_markActive = true;
}


// `v += delta`, but stop at 0.  This is intended to achieve logical
// symmetry with `LineIndex::clampIncrease`.
static void clampIncrease(int &v, int delta)
{
  v += delta;
  if (v < 0) {
    v = 0;
  }
}


static void clampMove(
  TextLCoord &coord, LineDifference deltaLine, int deltaCol)
{
  coord.m_line.clampIncrease(deltaLine);
  clampIncrease(coord.m_column, deltaCol);
}


static TextLCoord clampMoved(
  TextLCoord coord, LineDifference deltaLine, int deltaCol)
{
  clampMove(coord, deltaLine, deltaCol);
  return coord;
}


void TextDocumentEditor::moveMarkBy(
  LineDifference deltaLine, int deltaCol)
{
  xassert(m_markActive);

  clampMove(m_mark, deltaLine, deltaCol);
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
  this->setMark(TextLCoord(this->cursor().m_line.succ(), 0));
}


void TextDocumentEditor::selectEntireFile()
{
  this->setCursor(this->beginLCoord());
  this->setMark(this->endLCoord());
  this->scrollToCursor();
}


TextLCoordRange TextDocumentEditor::getSelectLayoutRange() const
{
  if (!m_markActive) {
    return TextLCoordRange(m_cursor, m_cursor);
  }
  else {
    return TextLCoordRange(m_cursor, m_mark).rectified();
  }
}


TextMCoordRange TextDocumentEditor::getSelectModelRange() const
{
  return this->toMCoordRange(this->getSelectLayoutRange());
}


void TextDocumentEditor::setSelectRange(TextLCoordRange const &range)
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
    return this->getTextForLRangeString(this->getSelectLayoutRange());
  }
}


string TextDocumentEditor::getSelectedOrIdentifier() const
{
  if (m_markActive) {
    return this->getSelectedText();
  }

  TextMCoord modelCursor(this->toMCoord(m_cursor));
  string text = this->getWholeLineString(modelCursor.m_line);
  return cIdentifierAt(text, modelCursor.m_byteIndex);
}


void TextDocumentEditor::swapCursorAndMark()
{
  if (m_markActive) {
    TextLCoord tmp(m_mark);
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


void TextDocumentEditor::setFirstVisible(TextLCoord fv)
{
  xassert(fv.nonNegative());
  LineDifference h = m_lastVisible.m_line - m_firstVisible.m_line;
  int w = m_lastVisible.m_column - m_firstVisible.m_column;
  m_firstVisible = fv;
  m_lastVisible.m_line = fv.m_line + h;
  m_lastVisible.m_column = fv.m_column + w;

  TRACE("TextDocumentEditor",
    "setFirstVisible: fv=" << m_firstVisible <<
    " lv=" << m_lastVisible);
}


void TextDocumentEditor::moveFirstVisibleBy(
  LineDifference deltaLine, int deltaCol)
{
  setFirstVisible(clampMoved(m_firstVisible, deltaLine, deltaCol));
}


void TextDocumentEditor::moveFirstVisibleAndCursor(
  LineDifference deltaLine, int deltaCol)
{
  TRACE("moveFirstVisibleAndCursor",
        "start: firstVis=" << m_firstVisible
     << ", cursor=" << m_cursor
     << ", deltaLine=" << deltaLine
     << ", deltaCol=" << deltaCol);

  // first make sure the view contains the cursor
  this->scrollToCursor();

  // move viewport, but remember original so we can tell
  // when there's truncation
  LineIndex origVL = m_firstVisible.m_line;
  int origVC = m_firstVisible.m_column;
  this->moveFirstVisibleBy(deltaLine, deltaCol);

  // now move cursor by the amount that the viewport moved
  this->moveCursorBy(m_firstVisible.m_line - origVL,
                     m_firstVisible.m_column - origVC);

  TRACE("moveFirstVisibleAndCursor",
        "end: firstVis=" << m_firstVisible <<
        ", cursor=" << m_cursor);
}


void TextDocumentEditor::moveFirstVisibleConfineCursor(
  LineDifference deltaLine, int deltaCol)
{
  moveFirstVisibleBy(deltaLine, deltaCol);
  confineCursorToVisible();
}


void TextDocumentEditor::setLastVisible(TextLCoord lv)
{
  // If the user resizes the window down to nothing, we might calculate
  // a visible region with zero width.  Require it to be positive, i.e.,
  // that last >= first.
  m_lastVisible.m_line = std::max(lv.m_line, m_firstVisible.m_line);
  m_lastVisible.m_column = std::max(lv.m_column, m_firstVisible.m_column);
}


void TextDocumentEditor::setVisibleSize(int lines, int columns)
{
  // The size must always be positive, i.e., at least one line and one
  // column must be visible.
  lines = std::max(1, lines);
  columns = std::max(1, columns);

  this->setLastVisible(TextLCoord(
    LineIndex(m_firstVisible.m_line.get() + lines - 1),
    m_firstVisible.m_column + columns - 1
  ));
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
    return std::max(0, cur - width/2);
  }

  bool changed = false;
  if (cur-gap < firstVis) {
    firstVis = std::max(0, cur-gap);
    changed = true;
  }
  else if (cur+gap > lastVis) {
    firstVis += cur+gap - lastVis;
    changed = true;
  }

  if (changed && center) {
    // we had to adjust the viewport; make it actually centered
    firstVis = std::max(0, cur - width/2);
  }

  return firstVis;
}

void TextDocumentEditor::scrollToCoord(TextLCoord tc, int edgeGap)
{
  int fvline = stcHelper(this->firstVisible().m_line.get(),
                         this->lastVisible().m_line.get(),
                         tc.m_line.get(),
                         edgeGap);

  int fvcol = stcHelper(this->firstVisible().m_column,
                        this->lastVisible().m_column,
                        tc.m_column,
                        edgeGap);

  setFirstVisible(TextLCoord(LineIndex(fvline), fvcol));
}


void TextDocumentEditor::centerVisibleOnCursorLine()
{
  int newfv = std::max(0, m_cursor.m_line.get() - this->visLines()/2);
  this->setFirstVisible(TextLCoord(LineIndex(newfv), 0));
  this->scrollToCursor();
}


void TextDocumentEditor::scrollToCursorIfBarelyOffscreen(
  LineDifference howFar, int edgeGap)
{
  // Check that the cursor is within horizontal bounds.
  if (!cc::le_le(m_firstVisible.m_column,
                 m_cursor.m_column,
                 m_lastVisible.m_column)) {
    return;
  }

  // Check that the vertical coordinate is in the designated area.
  if (!cc::lt_le(m_lastVisible.m_line,
                 m_cursor.m_line,
                 m_lastVisible.m_line + howFar)) {
    return;
  }

  // Scroll.
  scrollToCursor(edgeGap);
}


void TextDocumentEditor::moveCursor(bool relLine, int line, bool relCol, int col)
{
  if (relLine) {
    m_cursor.m_line += LineDifference(line);
  }
  else {
    m_cursor.m_line = LineIndex(line);
  }

  if (relCol) {
    m_cursor.m_column += col;
  }
  else {
    m_cursor.m_column = col;
  }
  xassert(m_cursor.m_column >= 0);

  TRACE("TextDocumentEditor",
    "moveCursor(" << relLine << ", " << line << ", " <<
    relCol << ", " << col << "): m_cursor = " << m_cursor);
}


TextLCoord TextDocumentEditor::mark() const
{
  xassert(markActive());
  return m_mark;
}


void TextDocumentEditor::insertText(char const *text, int textLen,
                                    InsertTextFlags flags)
{
  // The entire process of insertion should create one undo record.
  TDE_HistoryGrouper grouper(*this);

  this->deleteSelectionIf();
  this->fillToCursor();

  TextLCoord origCursor = this->cursor();

  m_doc->insertAt(this->toMCoord(origCursor), text, textLen);

  if (!( flags & ITF_CURSOR_AT_START )) {
    // Put the cursor at the end of the inserted text.
    this->walkCursorBytes(textLen);

    // Optionally put the mark at the start.
    if (flags & ITF_SELECT_AFTERWARD) {
      this->setMark(origCursor);
    }
  }

  this->scrollToCursor();
}


void TextDocumentEditor::insertString(string const &text,
                                      InsertTextFlags flags)
{
  // This does not actually prevent problems with embedded NULs
  // because I am still using my own string class which does not
  // handle them.  But at some point I will switch to std::string,
  // and then this will work as intended.
  this->insertText(text.c_str(), text.length(), flags);
}


void TextDocumentEditor::deleteLRColumns(bool left, int columnCount)
{
  TextLCoord start(this->cursor());

  TextLCoord end(start);
  this->walkLCoordColumns(end, left? -columnCount : +columnCount);

  TextLCoordRange range(start, end);
  range.rectify();

  this->deleteTextLRange(range);
}


void TextDocumentEditor::deleteLRBytes(bool left, int byteCount)
{
  TextMCoord start(this->toMCoord(this->cursor()));

  TextMCoord end(start);
  this->walkMCoordBytes(end, left? -byteCount : +byteCount);

  TextMCoordRange range(start, end);
  range.rectify();

  this->deleteTextMRange(range);
}


void TextDocumentEditor::deleteLRAbsCharacters(bool left, int characterCount)
{
  // TODO UTF-8: Do this right.
  this->deleteLRBytes(left, characterCount);
}


void TextDocumentEditor::deleteSelection()
{
  xassert(m_markActive);

  TextLCoordRange range(this->getSelectLayoutRange());
  if (range.m_start < this->endLCoord()) {
    this->fillToCoord(range.m_start);
  }

  this->deleteTextLRange(range);
  this->clearMark();
  this->scrollToCursor();
}


void TextDocumentEditor::backspaceFunction()
{
  if (m_markActive) {
    this->deleteSelection();
  }
  else if (m_cursor.m_column == 0) {
    if (m_cursor.m_line.isZero()) {
      // BOF, do nothing.
    }
    else if (m_cursor.m_line > m_doc->lastLineIndex()) {
      // Move cursor up non-destructively.
      this->moveCursorBy(LineDifference(-1), 0);
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
    this->moveCursorBy(LineDifference(0), -1);
  }
  else {
    // Remove the character to the left of the cursor.
    this->deleteLRAbsCharacters(true /*left*/, 1);
  }

  this->scrollToCursor();
}


void TextDocumentEditor::deleteKeyFunction()
{
  if (m_markActive) {
    this->deleteSelection();
  }
  else if (m_cursor >= this->endLCoord()) {
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


void TextDocumentEditor::walkLCoordColumns(TextLCoord &tc, int len) const
{
  xassert(tc.nonNegative());

  for (; len > 0; len--) {
    if (tc.m_column >= this->lineLengthColumns(tc.m_line)) {
      // cycle to next line
      ++tc.m_line;
      tc.m_column = 0;
    }
    else {
      tc.m_column++;
    }
  }

  for (; len < 0; len++) {
    if (tc.m_column == 0) {
      // cycle up to end of preceding line
      if (tc.m_line.isZero()) {
        return;      // Stop at BOF.
      }
      --tc.m_line;
      tc.m_column = this->lineLengthColumns(tc.m_line);
    }
    else {
      tc.m_column--;
    }
  }
}


void TextDocumentEditor::walkLCoordBytes(TextLCoord &lc, int len) const
{
  TextMCoord mc(this->toMCoord(lc));
  this->walkMCoordBytes(mc, len);
  lc = this->toLCoord(mc);
}


void TextDocumentEditor::walkMCoordBytes(TextMCoord &mc, int len) const
{
  for (; len > 0; len--) {
    if (mc.m_byteIndex >= this->lineLengthBytes(mc.m_line)) {
      // cycle to next line
      ++mc.m_line;
      mc.m_byteIndex = 0;
    }
    else {
      mc.m_byteIndex++;
    }
  }

  for (; len < 0; len++) {
    if (mc.m_byteIndex == 0) {
      // cycle up to end of preceding line
      if (mc.m_line.isZero()) {
        return;      // Stop at BOF.
      }
      --mc.m_line;
      mc.m_byteIndex = this->lineLengthBytes(mc.m_line);
    }
    else {
      mc.m_byteIndex--;
    }
  }
}


static int roundUp(int n, int unit)
{
  xassert(unit != 0);
  return ((n + unit - 1) / unit) * unit;
}


int TextDocumentEditor::layoutColumnAfter(int col, int c) const
{
  xassert(c != '\n');

  col++;
  if (c == '\t') {
    // Round 0-based column up to the next multiple of 'm_tabWidth'.
    col = roundUp(col, m_tabWidth);
  }
  return col;
}


void TextDocumentEditor::getLineLayout(TextLCoord lc,
  ArrayStack<char> &dest, int destLen) const
{
  xassert(lc.nonNegative());

  LineIterator it(*this, lc.m_line);
  for (; it.has() && it.columnOffset() < lc.m_column; it.advByte()) {
    // Nothing.
  }

  int writtenLen = 0;
  for (; it.has() && writtenLen < destLen; it.advByte()) {
    // Fill with spaces to get to the current column.
    while (lc.m_column + writtenLen < it.columnOffset()) {
      dest.push(' ');
      writtenLen++;
      if (writtenLen >= destLen) {
        xassert(writtenLen == destLen);
        break;     // Will break out of both loops.
      }
    }
    if (writtenLen >= destLen) {
      break;
    }

    // Add the current byte.
    dest.push(it.byteAt());
    writtenLen++;
  }

  // Fill the remainder with spaces.
  xassert(writtenLen <= destLen);
  int remainderLen = destLen - writtenLen;
  memset(dest.ptrToPushedMultipleAlt(remainderLen), ' ', remainderLen);
}


void TextDocumentEditor::getTextForLRange(TextLCoordRange const &range,
  ArrayStack<char> /*INOUT*/ &dest) const
{
  xassert(range.nonNegative());
  xassert(range.isRectified());

  m_doc->getTextForRange(this->toMCoordRange(range), dest);
}


string TextDocumentEditor::getTextForLRangeString(TextLCoordRange const &range) const
{
  ArrayStack<char> array;
  this->getTextForLRange(range, array);
  return toString(array);
}


void TextDocumentEditor::getWholeLine(LineIndex line,
  ArrayStack<char> /*INOUT*/ &dest) const
{
  if (line < m_doc->numLines()) {
    m_doc->getWholeLine(line, dest);
  }
  else {
    // Not appending anything is equivalent to appending "".
  }
}


string TextDocumentEditor::getWholeLineString(LineIndex line) const
{
  ArrayStack<char> text;
  this->getWholeLine(line, text);
  return toString(text);
}


// Are the bytes in 'text' a "word character" for the purposes of
// Ctrl+S, Ctrl+W?
static bool isWordCharText(ArrayStack<char> const &text)
{
  if (text.isEmpty()) {
    return false;
  }

  // For classification, look only at the first byte.  At least for
  // now, I only consider ASCII characters to be parts of "words".
  return (isalnum(text[0]) || text[0]=='_');
}


string TextDocumentEditor::getWordAfter(TextLCoord tc) const
{
  std::ostringstream sb;

  if (!( tc.m_line < numLines() )) {
    return "";
  }

  ArrayStack<char> text;

  bool seenWordChar = false;
  while (tc.m_column < lineLengthColumns(tc.m_line)) {
    // Get one column's worth of bytes.
    text.clear();
    getTextForLRange(tc, TextLCoord(tc.m_line, tc.m_column+1), text);

    bool iwc = isWordCharText(text);
    if (iwc || !seenWordChar) {
      // For Ctrl+S, Ctrl+W, add all bytes before or in the next word.
      for (int i=0; i < text.length(); i++) {
        sb << text[i];
      }

      if (iwc) {
        seenWordChar = true;
      }
    }
    else {
      // done, this is the end of the word
      break;
    }
    tc.m_column++;
  }

  return sb.str();
}


void TextDocumentEditor::modelToLayoutSpans(LineIndex line,
  LineCategories /*OUT*/ &layoutCategories,
  LineCategories /*IN*/ const &modelCategories)
{
  // Blank out the destination spans, taking the opportunity to set
  // the end category.
  layoutCategories.clear(modelCategories.endCategory);

  // We will work our way through the line in both model space and
  // layout space.
  LineIterator layoutIterator(*this, line);

  // Walk the input model coordinate spans.
  for (LineCategoryIter iter(modelCategories); !iter.atEnd(); iter.nextRun()) {
    int spanStartColumn = layoutIterator.columnOffset();

    // Iterate over 'iter.length' bytes.
    for (int i=0; i < iter.length; i++) {
      if (layoutIterator.has()) {
        layoutIterator.advByte();
      }
      else {
        // This happens because I show a synthetic newline character to
        // the highlighter at the end of each line, so it returns a span
        // for it.  We'll just ignore the span, since it will seem to
        // cover zero columns.  (This is kind of ugly...)
      }
    }

    int spanEndColumn = layoutIterator.columnOffset();

    // Add the layout span (if it is not empty).
    if (spanEndColumn > spanStartColumn) {
      layoutCategories.append(iter.category, spanEndColumn - spanStartColumn);
    }
  }
}


int TextDocumentEditor::countLeadingSpacesTabs(LineIndex line) const
{
  if (!m_doc->validLine(line)) {
    return 0;
  }
  else {
    return m_doc->countLeadingSpacesTabs(line);
  }
}


int TextDocumentEditor::countTrailingSpacesTabsColumns(LineIndex line) const
{
  if (!m_doc->validLine(line)) {
    return 0;
  }
  else {
    int trailBytes = m_doc->countTrailingSpacesTabs(line);
    if (trailBytes) {
      // This is somewhat inefficient...
      TextMCoord mcEnd(m_doc->lineEndCoord(line));
      TextLCoord lcEnd(this->toLCoord(mcEnd));

      TextMCoord mcBeforeWS(mcEnd);
      mcBeforeWS.m_byteIndex -= trailBytes;
      xassert(mcBeforeWS.m_byteIndex >= 0);
      TextLCoord lcBeforeWS(this->toLCoord(mcBeforeWS));

      int ret = lcEnd.m_column - lcBeforeWS.m_column;
      xassert(ret > 0);
      return ret;
    }
    else {
      return 0;
    }
  }
}


int TextDocumentEditor::getIndentationColumns(LineIndex line,
  string /*OUT*/ &indText) const
{
  if (!m_doc->validLine(line)) {
    return -1;
  }
  else {
    std::ostringstream sb;
    TextDocumentEditor::LineIterator it(*this, line);
    for (; it.has(); it.advByte()) {
      int c = it.byteAt();
      if (isSpaceOrTab(c)) {
        sb << (char)c;
      }
      else {
        break;
      }
    }
    if (!it.has()) {
      // Line is entirely whitespace, ignore it for indentation
      // determination.
      return -1;
    }
    indText = sb.str();
    return it.columnOffset();
  }
}


int TextDocumentEditor::getAboveIndentationColumns(LineIndex line,
  string /*OUT*/ &indText) const
{
  while (true) {
    if (line.isZero()) {
      break;
    }
    --line;

    int ind = this->getIndentationColumns(line, indText);
    if (ind >= 0) {
      return ind;
    }
  }
  indText = "";
  return 0;
}


// ---------------- TextDocumentEditor: modifications ------------------

void TextDocumentEditor::moveCursorBy(
  LineDifference deltaLine, int deltaCol)
{
  // prevent moving into negative territory
  deltaLine.clampLower( - cursor().m_line.get());
  deltaCol = std::max(deltaCol, - cursor().m_column);

  if (deltaLine || deltaCol) {
    moveCursor(true /*relLine*/, deltaLine.get(),
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
  int prevLine = std::max(0, cursor().m_line.get() - 1);
  moveCursor(false /*relLine*/, prevLine,
             false /*relCol*/, lineLengthColumns(LineIndex(prevLine)));
}


void TextDocumentEditor::moveCursorToTop()
{
  setCursor(this->beginLCoord());
  scrollToCursor();
}

void TextDocumentEditor::moveCursorToBottom()
{
  setCursor(TextLCoord(m_doc->lastLineIndex(), 0));
  scrollToCursor();
}


void TextDocumentEditor::advanceWithWrap(bool backwards)
{
  LineIndex line = cursor().m_line;
  int col = cursor().m_column;

  if (!backwards) {
    if (line < numLines() &&
        col < cursorLineLengthColumns()) {
      moveCursorBy(LineDifference(0), 1);
    }
    else {
      moveToNextLineStart();
    }
  }

  else {
    if (line < numLines() &&
        col > 0) {
      moveCursorBy(LineDifference(0), -1);
    }
    else if (!line.isZero()) {
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
    std::max(m_firstVisible.m_line,
      std::min(m_lastVisible.m_line,   m_cursor.m_line));

  m_cursor.m_column =
    std::max(m_firstVisible.m_column,
      std::min(m_lastVisible.m_column, m_cursor.m_column));
}


void TextDocumentEditor::walkCursorBytes(int distance)
{
  this->walkLCoordBytes(m_cursor, distance);
}


bool TextDocumentEditor::cursorOnModelCoord() const
{
  TextLCoord lc(this->toLCoord(this->cursorAsModelCoord()));
  return lc == this->cursor();
}


TextMCoord TextDocumentEditor::cursorAsModelCoord() const
{
  return this->toMCoord(this->cursor());
}


void TextDocumentEditor::fillToCoord(TextLCoord const &tc)
{
  // Text to add in order to fill to the target coordinate.
  std::ostringstream textToAdd;

  // Layout lines added by 'textToAdd'.
  int textToAddLines = 0;

  // Plan to add blank lines to the end of the model until the target
  // coordinate is within an existing line.
  while (!( tc.m_line < this->numLines() + textToAddLines )) {
    textToAdd << '\n';
    textToAddLines++;
  }

  // Layout columns used by 'textToAdd'.
  int textToAddCols = 0;

  // How long is it currently?
  int curLen = this->lineLengthColumns(tc.m_line);
  if (curLen == 0) {
    // We are adding space to a blank line.  Look at the preceding
    // non-blank line to get its indentation, and use as much of that as
    // possible and as needed, with the effect that we continue the
    // prevailing indentation style.
    string indText;
    this->getAboveIndentationColumns(tc.m_line, indText /*OUT*/);

    // Process each character in 'indText'.
    size_t indTextLen = indText.length();
    for (size_t i=0; i < indTextLen; i++) {
      int c = (unsigned char)(indText[i]);

      int colAfter = layoutColumnAfter(textToAddCols, c);
      if (colAfter <= tc.m_column) {
        // Adding 'c' brings us closer to the target column without
        // going over.
        textToAdd << (char)c;
        textToAddCols = colAfter;
      }
      else {
        break;
      }
    }
  }

  // Use spaces to make up the remaining distance to the target
  // column.
  while (curLen + textToAddCols < tc.m_column) {
    textToAdd << ' ';
    textToAddCols++;
  }

  if (textToAddLines==0 && textToAddCols==0) {
    return;     // nothing to do
  }

  // Restore cursor and scroll state afterwards.
  CursorRestorer restorer(*this);

  // Move cursor the end of the 'tc' line if it exists in the model, or
  // the end of the last line otherwise.
  LineIndex lineToEdit = std::min(tc.m_line, m_doc->lastLineIndex());
  moveCursor(false /*relLine*/, lineToEdit.get(),
             false /*relCol*/, lineLengthColumns(lineToEdit));

  // Do not delete things implicitly here due to a selection!
  clearMark();

  // Add the computed text.
  insertString(textToAdd.str());

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
    moveCursorBy(LineDifference(0), -overEdge);
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

  // Auto-indent.
  string indText;
  int indCols = this->getAboveIndentationColumns(m_cursor.m_line,
                                                 indText /*OUT*/);
  if (hadCharsToRight) {
    // Insert indentation so the carried forward text starts
    // in the auto-indent column.
    this->insertString(indText);
  }
  else {
    // Move the cursor to the auto-indent column but do not
    // fill with spaces.  This way I can press Enter more
    // than once without adding lots of spaces.
    this->moveCursorBy(LineDifference(0), indCols);
  }

  this->scrollToCursor();
}


void TextDocumentEditor::deleteTextLRange(TextLCoordRange const &range)
{
  xassert(range.isRectified());
  xassert(range.nonNegative());

  m_doc->deleteTextRange(this->toMCoordRange(range));

  // Set cursor per spec.
  this->setCursor(range.m_start);
  this->clearMark();
}


void TextDocumentEditor::deleteTextMRange(TextMCoordRange const &range)
{
  xassert(range.isRectified());

  m_doc->deleteTextRange(range);

  // Set cursor per spec.
  this->setCursor(this->toLCoord(range.m_start));
  this->clearMark();
}


void TextDocumentEditor::indentLines(LineIndex start, LineCount lines, int ind)
{
  TDE_HistoryGrouper grouper(*this);
  CursorRestorer cr(*this);

  // Don't let the selection interfere with the text insertions below.
  this->clearMark();

  for (LineIndex line=start; line < start+lines &&
                             line < this->numLines(); ++line) {
    this->setCursor(TextLCoord(line, 0));

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

  TextLCoordRange range = this->getSelectLayoutRange();

  // If no characters on the last line are selected, then do not modify
  // that line.
  LineIndex endLine =
    (range.m_end.m_column==0?
       range.m_end.m_line.pred() :
       range.m_end.m_line);

  this->indentLines(
    range.m_start.m_line,
    LineCount(endLine - range.m_start.m_line + 1),
    amt);

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


void TextDocumentEditor::clipboardPaste(char const *text, int textLen,
                                        bool cursorToStart)
{
  this->insertText(text, textLen,
    cursorToStart? ITF_CURSOR_AT_START : ITF_NONE);
}


void TextDocumentEditor::debugPrint() const
{
  m_doc->getCore().dumpRepresentation();
  cout << "  cursor: " << m_cursor << endl;
  cout << "  markActive: " << m_markActive << endl;
  cout << "  mark: " << m_mark << endl;
  cout << "  firstVisible: " << m_firstVisible << endl;
  cout << "  lastVisible: " << m_lastVisible << endl;
  cout << "  tabWidth: " << m_tabWidth << endl;
}


// --------------- TextDocumentEditor::LineIterator ---------------
TextDocumentEditor::LineIterator::LineIterator(
  TextDocumentEditor const &tde, LineIndex line)
:
  m_tde(tde),
  m_iter(*(tde.getDocument()), line),
  m_column(0)
{}


void TextDocumentEditor::LineIterator::advByte()
{
  m_column = m_tde.layoutColumnAfter(m_column, m_iter.byteAt());
  m_iter.advByte();
}


// -------------------- CursorRestorer ------------------
CursorRestorer::CursorRestorer(TextDocumentEditor &e)
  : tde(&e),
    cursor(e.cursor()),
    markActive(e.markActive()),
    mark(markActive? e.mark() : TextLCoord()),
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


// --------------------- TDE_HistoryGrouper ---------------
TDE_HistoryGrouper::TDE_HistoryGrouper(TextDocumentEditor &e)
  : m_editor(&e)
{
  m_editor->beginUndoGroup();
}

TDE_HistoryGrouper::~TDE_HistoryGrouper() NOEXCEPT
{
  GENERIC_CATCH_BEGIN
  m_editor->endUndoGroup();
  GENERIC_CATCH_END
}


// EOF
