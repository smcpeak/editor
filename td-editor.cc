// td-editor.cc
// code for td-editor.h

#include "td-editor.h"                 // this module

// editor
#include "byte-count.h"                // ByteCount, sizeBC
#include "column-difference.h"         // ColumnDifference
#include "editor-strutil.h"            // cIdentifierAt
#include "justify.h"                   // justifyNearLine
#include "line-count.h"                // LineCount
#include "line-difference.h"           // LineDifference

// smbase
#include "smbase/array.h"              // Array
#include "smbase/chained-cond.h"       // smbase::cc::{le_le, lt_le}
#include "smbase/codepoint.h"          // isSpaceOrTab
#include "smbase/datetime.h"           // DateTimeSeconds
#include "smbase/objcount.h"           // CHECK_OBJECT_COUNT
#include "smbase/sm-swap.h"            // swap
#include "smbase/sm-trace.h"           // INIT_TRACE, etc.

// libc++
#include <algorithm>                   // std::{min, max}

using namespace smbase;


INIT_TRACE("td-editor");


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
    m_lastVisible(LineIndex(4), ColumnIndex(9)),

    m_tabWidth(8)
{
  xassert(doc);
  selfCheck();

  TextDocumentEditor::s_objectCount++;
  TRACE1(
    "created TDE at " << (void*)this <<
    ", doc=" << (void*)m_doc <<
    ", oc=" << s_objectCount);
}


TextDocumentEditor::~TextDocumentEditor()
{
  TextDocumentEditor::s_objectCount--;
  TRACE1(
    "destroyed TDE at " << (void*)this <<
    ", doc=" << (void*)m_doc <<
    ", oc=" << s_objectCount);
}


void TextDocumentEditor::selfCheck() const
{
  xassert(m_firstVisible.m_line <= m_lastVisible.m_line);
  xassert(m_firstVisible.m_column <= m_lastVisible.m_column);

  m_doc->selfCheck();
}


void TextDocumentEditor::setTabWidth(ColumnCount tabWidth)
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
    return TextMCoord(lc.m_line, ByteIndex(0));
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
  xassertPrecondition(validMCoord(mc));

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


TextLCoord TextDocumentEditor::toAdjustedLCoord(TextMCoord mc) const
{
  m_doc->adjustMCoord(mc /*INOUT*/);
  return toLCoord(mc);
}


ColumnCount TextDocumentEditor::lineLengthColumns(LineIndex line) const
{
  return lineLengthAsColumnIndex(line);
}


ColumnIndex TextDocumentEditor::lineLengthAsColumnIndex(
  LineIndex line) const
{
  if (line < numLines()) {
    return this->lineEndLCoord(line).m_column;
  }
  else {
    return ColumnIndex(0);
  }
}


ColumnCount TextDocumentEditor::cursorLineLengthColumns() const
{
  return lineLengthColumns(cursor().m_line);
}


ColumnIndex TextDocumentEditor::cursorLineLengthAsColumnIndex() const
{
  return lineLengthAsColumnIndex(cursor().m_line);
}


TextLCoord TextDocumentEditor::lineEndLCoord(LineIndex line) const
{
  if (line < numLines()) {
    return this->toLCoord(m_doc->lineEndCoord(line));
  }
  else {
    return TextLCoord(line, ColumnIndex(0));
  }
}


ColumnCount TextDocumentEditor::maxLineLengthColumns() const
{
  // TODO: BUG: Layout must be taken into account.
  //
  // Note the units mismatch!  That is because this is wrong, using
  // a byte count where a column count is advertised.
  return ColumnCount(m_doc->maxLineLengthBytes().get());
}


TextLCoord TextDocumentEditor::beginLCoord() const
{
  return TextLCoord(LineIndex(0), ColumnIndex(0));
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
  TRACE1("setCursor(" << c << ")");

  m_cursor = c;
}


void TextDocumentEditor::setMark(TextLCoord m)
{
  TRACE1("setMark(" << m << ")");

  m_mark = m;
  m_markActive = true;
}


void TextDocumentEditor::clearMark()
{
  TRACE1("clearMark()");

  m_mark = TextLCoord();
  m_markActive = false;
}


static void clampMove(
  TextLCoord &coord, LineDifference deltaLine, ColumnDifference deltaCol)
{
  coord.m_line.clampIncrease(deltaLine);
  coord.m_column.clampIncrease(deltaCol);
}


static TextLCoord clampMoved(
  TextLCoord coord, LineDifference deltaLine, ColumnDifference deltaCol)
{
  clampMove(coord, deltaLine, deltaCol);
  return coord;
}


void TextDocumentEditor::moveMarkBy(
  LineDifference deltaLine, ColumnDifference deltaCol)
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
  this->setCursorColumn(ColumnIndex(0));

  // Make the selection end at the start of the next line.
  this->setMark(
    TextLCoord(this->cursor().m_line.succ(), ColumnIndex(0)));
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


ColumnCount TextDocumentEditor::visColumns() const
{
  return ColumnCount(
    m_lastVisible.m_column - m_firstVisible.m_column + ColumnCount(1));
}


void TextDocumentEditor::setFirstVisible(TextLCoord fv)
{
  LineDifference h = m_lastVisible.m_line - m_firstVisible.m_line;
  ColumnDifference w = m_lastVisible.m_column - m_firstVisible.m_column;
  m_firstVisible = fv;
  m_lastVisible.m_line = fv.m_line + h;
  m_lastVisible.m_column = fv.m_column + w;

  TRACE1(
    "setFirstVisible: fv=" << m_firstVisible <<
    " lv=" << m_lastVisible);
}


void TextDocumentEditor::moveFirstVisibleBy(
  LineDifference deltaLine, ColumnDifference deltaCol)
{
  setFirstVisible(clampMoved(m_firstVisible, deltaLine, deltaCol));
}


void TextDocumentEditor::moveFirstVisibleAndCursor(
  LineDifference deltaLine, ColumnDifference deltaCol)
{
  TRACE1("moveFirstVisibleAndCursor start: "
       "firstVis=" << m_firstVisible
    << ", cursor=" << m_cursor
    << ", deltaLine=" << deltaLine
    << ", deltaCol=" << deltaCol);

  // first make sure the view contains the cursor
  this->scrollToCursor();

  // move viewport, but remember original so we can tell
  // when there's truncation
  LineIndex origVL = m_firstVisible.m_line;
  ColumnIndex origVC = m_firstVisible.m_column;
  this->moveFirstVisibleBy(deltaLine, deltaCol);

  // now move cursor by the amount that the viewport moved
  this->moveCursorBy(m_firstVisible.m_line - origVL,
                     m_firstVisible.m_column - origVC);

  TRACE1("moveFirstVisibleAndCursor end: "
       "firstVis=" << m_firstVisible
    << ", cursor=" << m_cursor);
}


void TextDocumentEditor::moveFirstVisibleConfineCursor(
  LineDifference deltaLine, ColumnDifference deltaCol)
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


void TextDocumentEditor::setVisibleSize(int lines, ColumnCount columns)
{
  // The size must always be positive, i.e., at least one line and one
  // column must be visible.
  lines = std::max(1, lines);
  columns.clampLower(ColumnIndex(1));

  this->setLastVisible(TextLCoord(
    LineIndex(m_firstVisible.m_line.get() + lines - 1),
    m_firstVisible.m_column + columns.pred()
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

  int fvcol = stcHelper(this->firstVisible().m_column.get(),
                        this->lastVisible().m_column.get(),
                        tc.m_column.get(),
                        edgeGap);

  setFirstVisible(TextLCoord(LineIndex(fvline), ColumnIndex(fvcol)));
}


void TextDocumentEditor::centerVisibleOnCursorLine()
{
  int newfv = std::max(0, m_cursor.m_line.get() - this->visLines()/2);
  this->setFirstVisible(TextLCoord(LineIndex(newfv), ColumnIndex(0)));
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
    m_cursor.m_column += ColumnDifference(col);
  }
  else {
    m_cursor.m_column = ColumnIndex(col);
  }
  xassert(m_cursor.m_column >= 0);

  TRACE1(
    "moveCursor(" << relLine << ", " << line << ", " <<
    relCol << ", " << col << "): m_cursor = " << m_cursor);
}


TextLCoord TextDocumentEditor::mark() const
{
  xassert(markActive());
  return m_mark;
}


void TextDocumentEditor::insertText(char const *text, ByteCount textLen,
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
  this->insertText(text.c_str(), sizeBC(text), flags);
}


void TextDocumentEditor::deleteLRColumns(
  bool left, ColumnCount columnCount)
{
  TextLCoord start(this->cursor());

  TextLCoord end(start);
  this->walkLCoordColumns(end, left? -columnCount : +columnCount);

  TextLCoordRange range(start, end);
  range.rectify();

  this->deleteTextLRange(range);
}


void TextDocumentEditor::deleteLRBytes(bool left, ByteCount byteCount)
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
  this->deleteLRBytes(left, ByteCount(characterCount /*units mismatch!*/));
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
      this->moveCursorBy(LineDifference(-1), ColumnDifference(0));
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
    this->moveCursorBy(LineDifference(0), ColumnDifference(-1));
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


void TextDocumentEditor::walkLCoordColumns(
  TextLCoord &tc /*INOUT*/, ColumnDifference len) const
{
  for (; len > 0; len--) {
    if (tc.m_column >= this->lineLengthColumns(tc.m_line)) {
      // cycle to next line
      ++tc.m_line;
      tc.m_column = ColumnIndex(0);
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
      tc.m_column = this->lineLengthAsColumnIndex(tc.m_line);
    }
    else {
      tc.m_column--;
    }
  }
}


void TextDocumentEditor::walkLCoordBytes(
  TextLCoord &lc /*INOUT*/, ByteDifference len) const
{
  TextMCoord mc(this->toMCoord(lc));
  this->walkMCoordBytes(mc, len);
  lc = this->toLCoord(mc);
}


void TextDocumentEditor::walkMCoordBytes(
  TextMCoord &mc /*INOUT*/, ByteDifference len) const
{
  for (; len > 0; len--) {
    if (mc.m_byteIndex >= this->lineLengthBytes(mc.m_line)) {
      // cycle to next line
      ++mc.m_line;
      mc.m_byteIndex.set(0);
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
      mc.m_byteIndex = this->lineLengthByteIndex(mc.m_line);
    }
    else {
      mc.m_byteIndex--;
    }
  }
}


ColumnIndex TextDocumentEditor::layoutColumnAfter(
  ColumnIndex col, int c) const
{
  xassert(c != '\n');

  ++col;
  if (c == '\t') {
    // Round 0-based column up to the next multiple of 'm_tabWidth'.
    col = col.roundUpToMultipleOf(m_tabWidth);
  }
  return col;
}


void TextDocumentEditor::getLineLayout(TextLCoord lc,
  ArrayStack<char> &dest, ColumnCount destLen) const
{
  LineIterator it(*this, lc.m_line);
  for (; it.has() && it.columnOffset() < lc.m_column; it.advByte()) {
    // Nothing.
  }

  ColumnCount writtenLen(0);
  for (; it.has() && writtenLen < destLen; it.advByte()) {
    // Fill with spaces to get to the current column.
    while (lc.m_column + writtenLen < it.columnOffset()) {
      dest.push(' ');
      ++writtenLen;
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
  ColumnCount remainderLen(destLen - writtenLen);
  memset(
    dest.ptrToPushedMultipleAlt(remainderLen.get()),
    ' ',
    remainderLen.get());
}


void TextDocumentEditor::getTextForLRange(TextLCoordRange const &range,
  ArrayStack<char> /*INOUT*/ &dest) const
{
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
    getTextForLRange(tc, TextLCoord(tc.m_line, tc.m_column.succ()), text);

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
    ColumnIndex spanStartColumn = layoutIterator.columnOffset();

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

    ColumnIndex spanEndColumn = layoutIterator.columnOffset();

    // Add the layout span (if it is not empty).
    if (spanEndColumn > spanStartColumn) {
      layoutCategories.append(
        iter.category,
        // Here, we are passing a column count.
        ByteOrColumnCount((spanEndColumn - spanStartColumn).get()));
    }
  }
}


ByteCount TextDocumentEditor::countLeadingSpacesTabs(LineIndex line) const
{
  if (!m_doc->validLine(line)) {
    return ByteCount(0);
  }
  else {
    return m_doc->countLeadingSpacesTabs(line);
  }
}


ColumnCount TextDocumentEditor::countTrailingSpacesTabsColumns(
  LineIndex line) const
{
  if (!m_doc->validLine(line)) {
    return ColumnCount(0);
  }
  else {
    // Get a count of trailing WS *bytes*.
    ByteCount trailBytes = m_doc->countTrailingSpacesTabs(line);

    // Convert that to a count of trailing *columns*.
    if (trailBytes) {
      // This is somewhat inefficient...
      TextMCoord mcEnd(m_doc->lineEndCoord(line));
      TextLCoord lcEnd(this->toLCoord(mcEnd));

      TextMCoord mcBeforeWS(mcEnd);
      mcBeforeWS.m_byteIndex -= trailBytes;
      xassert(mcBeforeWS.m_byteIndex >= 0);
      TextLCoord lcBeforeWS(this->toLCoord(mcBeforeWS));

      ColumnCount ret(lcEnd.m_column - lcBeforeWS.m_column);
      xassert(ret > 0);
      return ret;
    }
    else {
      return ColumnCount(0);
    }
  }
}


std::optional<ColumnCount> TextDocumentEditor::getIndentationColumns(
  LineIndex line, string /*OUT*/ &indText) const
{
  if (!m_doc->validLine(line)) {
    return std::nullopt;
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
      return std::nullopt;
    }
    indText = sb.str();
    return it.columnOffset();
  }
}


ColumnCount TextDocumentEditor::getAboveIndentationColumns(
  LineIndex line, string /*OUT*/ &indText) const
{
  while (true) {
    if (line.isZero()) {
      break;
    }
    --line;

    if (std::optional<ColumnCount> ind =
          this->getIndentationColumns(line, indText)) {
      return *ind;
    }
  }
  indText = "";
  return ColumnCount(0);
}


// ---------------- TextDocumentEditor: modifications ------------------

void TextDocumentEditor::moveCursorBy(
  LineDifference deltaLine, ColumnDifference deltaCol)
{
  // prevent moving into negative territory
  deltaLine.clampLower( - cursor().m_line);
  deltaCol.clampLower( - cursor().m_column);

  if (deltaLine || deltaCol) {
    moveCursor(true /*relLine*/, deltaLine.get(),
               true /*relCol*/, deltaCol.get());
  }
}


void TextDocumentEditor::setCursorColumn(ColumnIndex newCol)
{
  moveCursor(true /*relLine*/, 0,
             false /*relCol*/, newCol.get());
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
             false /*relCol*/, lineLengthColumns(LineIndex(prevLine)).get());
}


void TextDocumentEditor::moveCursorToTop()
{
  setCursor(this->beginLCoord());
  scrollToCursor();
}

void TextDocumentEditor::moveCursorToBottom()
{
  setCursor(TextLCoord(m_doc->lastLineIndex(), ColumnIndex(0)));
  scrollToCursor();
}


void TextDocumentEditor::advanceWithWrap(bool backwards)
{
  LineIndex line = cursor().m_line;
  ColumnIndex col = cursor().m_column;

  if (!backwards) {
    if (line < numLines() &&
        col < cursorLineLengthColumns()) {
      moveCursorBy(LineDifference(0), ColumnDifference(1));
    }
    else {
      moveToNextLineStart();
    }
  }

  else {
    if (line < numLines() &&
        col > 0) {
      moveCursorBy(LineDifference(0), ColumnDifference(-1));
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


void TextDocumentEditor::walkCursorBytes(ByteDifference distance)
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
  LineCount textToAddLines(0);

  // Plan to add blank lines to the end of the model until the target
  // coordinate is within an existing line.
  while (!( tc.m_line < this->numLines() + textToAddLines )) {
    textToAdd << '\n';
    ++textToAddLines;
  }

  // Layout columns used by 'textToAdd'.
  ColumnCount textToAddCols(0);

  // How long is the target line currently?
  ColumnCount curLen = this->lineLengthColumns(tc.m_line);
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

      // The conversion of `textToAddCols` to `ColumnIndex` is justified
      // by the fact that `curLen` is 0, so everything in `textToAdd`
      // (ignoring the initial newlines) will start in the first column
      // of the target line.
      ColumnIndex colAfter =
        layoutColumnAfter(ColumnIndex(textToAddCols), c);

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
    ++textToAddCols;
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
             false /*relCol*/, lineLengthColumns(lineToEdit).get());

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
  ColumnDifference overEdge =
    cursor().m_column - cursorLineLengthColumns();
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
  ColumnCount indCols =
    this->getAboveIndentationColumns(m_cursor.m_line,
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


void TextDocumentEditor::indentLines(
  LineIndex start, LineCount lines, ColumnDifference ind)
{
  TDE_HistoryGrouper grouper(*this);
  CursorRestorer cr(*this);

  // Don't let the selection interfere with the text insertions below.
  this->clearMark();

  for (LineIndex line=start; line < start+lines &&
                             line < this->numLines(); ++line) {
    this->setCursor(TextLCoord(line, ColumnIndex(0)));

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
      ByteCount lineInd = this->countLeadingSpacesTabs(line);
      for (int i=0; i<(-ind) && i<lineInd; i++) {
        this->deleteChar();
      }
    }
  }
}


bool TextDocumentEditor::blockIndent(ColumnDifference amt)
{
  if (!this->markActive()) {
    return false;
  }

  TextLCoordRange range = this->getSelectLayoutRange();

  // If no characters on the last line are selected, then do not modify
  // that line.
  LineIndex endLine =
    (range.m_end.m_column==0?
       range.m_end.m_line.predClamped() :
       range.m_end.m_line);

  this->indentLines(
    range.m_start.m_line,
    LineCount(endLine - range.m_start.m_line + 1),
    amt);

  return true;
}


bool TextDocumentEditor::justifyNearCursor(ColumnCount desiredWidth)
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


void TextDocumentEditor::clipboardPaste(
  char const *text, ByteCount textLen, bool cursorToStart)
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
