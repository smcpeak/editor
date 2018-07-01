// td-editor.cc
// Code for td-editor.h.

#include "td-editor.h"                 // this module

#include "justify.h"                   // justifyNearLine

#include "array.h"                     // Array
#include "datetime.h"                  // DateTimeSeconds
#include "trace.h"                     // TRACE
#include "typ.h"                       // min, max


int TextDocumentEditor::s_objectCount = 0;


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
}


TextDocumentEditor::~TextDocumentEditor()
{
  TextDocumentEditor::s_objectCount--;
}


void TextDocumentEditor::selfCheck() const
{
  xassert(m_cursor.nonNegative());
  if (m_markActive) {
    xassert(m_mark.nonNegative());
  }
  xassert(m_firstVisible.nonNegative());
  xassert(m_firstVisible.line <= m_lastVisible.line);
  xassert(m_firstVisible.column <= m_lastVisible.column);
}


bool TextDocumentEditor::validCursor() const
{
  return m_doc->validCoord(cursor());
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
  m_mark.line = max(0, m_mark.line + deltaLine);
  m_mark.column = max(0, m_mark.column + deltaCol);
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
  this->setMark(TextCoord(this->cursor().line + 1, 0));
}


void TextDocumentEditor::getSelectRegion(
  TextCoord &selLow, TextCoord &selHigh) const
{
  if (!m_markActive) {
    selLow = selHigh = m_cursor;
  }
  else {
    if (m_cursor <= m_mark) {
      selLow = m_cursor;
      selHigh = m_mark;
    }
    else {
      selLow = m_mark;
      selHigh = m_cursor;
    }
  }
}


string TextDocumentEditor::getSelectedText() const
{
  if (!m_markActive) {
    return "";
  }
  else {
    TextCoord selLow, selHigh;
    this->getSelectRegion(selLow, selHigh);
    return this->getTextRange(selLow, selHigh);
  }
}


void TextDocumentEditor::normalizeCursorGTEMark()
{
  if (m_markActive &&
      m_mark > m_cursor) {
    TextCoord tmp(m_mark);
    m_mark = m_cursor;
    m_cursor = tmp;
  }
}


void TextDocumentEditor::setFirstVisible(TextCoord fv)
{
  xassert(fv.nonNegative());
  int h = m_lastVisible.line - m_firstVisible.line;
  int w = m_lastVisible.column - m_firstVisible.column;
  m_firstVisible = fv;
  m_lastVisible.line = fv.line + h;
  m_lastVisible.column = fv.column + w;
}


void TextDocumentEditor::moveFirstVisibleBy(int deltaLine, int deltaCol)
{
  int line = max(0, m_firstVisible.line + deltaLine);
  int col = max(0, m_firstVisible.column + deltaCol);
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
  int origVL = m_firstVisible.line;
  int origVC = m_firstVisible.column;
  this->moveFirstVisibleBy(deltaLine, deltaCol);

  // now move cursor by the amount that the viewport moved
  this->moveCursorBy(m_firstVisible.line - origVL,
                     m_firstVisible.column - origVC);

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
  m_lastVisible.line = m_firstVisible.line + lines - 1;
  m_lastVisible.column = m_firstVisible.column + columns - 1;
}


// For a particular dimension, return the new start coordinate
// of the viewport.
static int stcHelper(int firstVis, int lastVis, int cur, int gap)
{
  bool center = false;
  if (gap == -1) {
    center = true;
    gap = 0;
  }

  int width = lastVis - firstVis + 1;

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

void TextDocumentEditor::scrollToCursor(int edgeGap)
{
  int fvline = stcHelper(this->firstVisible().line,
                         this->lastVisible().line,
                         this->cursor().line,
                         edgeGap);

  int fvcol = stcHelper(this->firstVisible().column,
                        this->lastVisible().column,
                        this->cursor().column,
                        edgeGap);

  setFirstVisible(TextCoord(fvline, fvcol));
}


void TextDocumentEditor::centerVisibleOnCursorLine()
{
  int newfv = max(0, m_cursor.line - this->visLines()/2);
  this->setFirstVisible(TextCoord(newfv, 0));
  this->scrollToCursor();
}


void TextDocumentEditor::moveCursor(bool relLine, int line, bool relCol, int col)
{
  if (relLine) {
    m_cursor.line += line;
  }
  else {
    m_cursor.line = line;
  }
  xassert(m_cursor.line >= 0);

  if (relCol) {
    m_cursor.column += col;
  }
  else {
    m_cursor.column = col;
  }
  xassert(m_cursor.column >= 0);
}


TextCoord TextDocumentEditor::mark() const
{
  xassert(markActive());
  return m_mark;
}


void TextDocumentEditor::insertText(char const *text, int textLen)
{
  xassert(validCursor());

  m_doc->insertAt(cursor(), text, textLen);

  // Put the cursor at the end of the inserted text.
  this->walkCursor(textLen);

  scrollToCursor();
}


void TextDocumentEditor::insertString(string text)
{
  // This does not actually prevent problems with embedded NULs
  // because I am still using my own string class which does not
  // handle them.  But at some point I will switch to std::string,
  // and then this will work as intended.
  this->insertText(text.c_str(), text.length());
}


void TextDocumentEditor::deleteLR(bool left, int count)
{
  xassert(validCursor());

  if (left) {
    // Move the cursor to the start of the text to delete.
    this->walkCursor(-count);
  }

  m_doc->deleteAt(cursor(), count);
}


void TextDocumentEditor::deleteSelection()
{
  xassert(m_markActive);

  TextCoord selLow, selHigh;
  this->getSelectRegion(selLow, selHigh);
  this->deleteTextRange(selLow, selHigh);
  this->clearMark();
  this->scrollToCursor();
}


void TextDocumentEditor::backspaceFunction()
{
  if (m_markActive) {
    this->deleteSelection();
  }
  else if (m_cursor.column == 0) {
    if (m_cursor.line == 0) {
      // BOF, do nothing.
    }
    else if (m_cursor.line > m_doc->numLines() - 1) {
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
  else if (m_cursor.column > this->cursorLineLength()) {
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
  else if (m_cursor >= m_doc->endCoord()) {
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
  this->setCursor(m_doc->undo());
  this->clearMark();
  this->scrollToCursor();
}


void TextDocumentEditor::redo()
{
  this->setCursor(m_doc->redo());
  this->clearMark();
  this->scrollToCursor();
}


int TextDocumentEditor::lineLengthLoose(int line) const
{
  xassert(line >= 0);
  if (line < numLines()) {
    return lineLength(line);
  }
  else {
    return 0;
  }
}


TextCoord TextDocumentEditor::lineEndCoord(int line) const
{
  return TextCoord(line, lineLengthLoose(line));
}


void TextDocumentEditor::truncateCoord(TextCoord &tc) const
{
  tc.line = max(0, tc.line);
  tc.column = max(0, tc.column);

  tc.line = min(tc.line, this->numLines() - 1); // numLines>=1 always
  tc.column = min(tc.column, this->lineLength(tc.line));
}


bool TextDocumentEditor::walkCoord(TextCoord &tc, int len) const
{
  xassert(this->validCoord(tc));

  for (; len > 0; len--) {
    if (tc.column == this->lineLength(tc.line)) {
      // cycle to next line
      tc.line++;
      if (tc.line >= this->numLines()) {
        return false;      // beyond EOF
      }
      tc.column=0;
    }
    else {
      tc.column++;
    }
  }

  for (; len < 0; len++) {
    if (tc.column == 0) {
      // cycle up to end of preceding line
      tc.line--;
      if (tc.line < 0) {
        return false;      // before BOF
      }
      tc.column = this->lineLength(tc.line);
    }
    else {
      tc.column--;
    }
  }

  return true;
}


void TextDocumentEditor::getLineLoose(TextCoord tc, char *dest, int destLen) const
{
  xassert(tc.nonNegative());

  // how much of the source is in the defined region?
  int undef = destLen;
  int def = 0;

  if (tc.line < numLines() &&
      tc.column < lineLength(tc.line)) {
    //       <----- lineLength -------->
    // line: [-------------------------][..spaces...]
    //       <------ col -----><----- destlen ------>
    //                         <- def -><-- undef -->
    // dest:                   [--------------------]

    undef = max(0, (tc.column+destLen) - lineLength(tc.line));
    def = max(0, destLen - undef);
  }

  // initial part in defined region
  if (def) {
    m_doc->getLine(tc, dest, def);
  }

  // spaces past defined region
  memset(dest+def, ' ', undef);
}


string TextDocumentEditor::getTextRange(TextCoord tc1, TextCoord tc2) const
{
  xassert(tc1.nonNegative());
  xassert(tc2.nonNegative());

  // this function uses the line1==line2 case as a base case of a two
  // level recursion; it's not terribly efficient

  if (tc1.line == tc2.line) {
    // extracting text from a single line
    xassert(tc1.column <= tc2.column);
    int len = tc2.column-tc1.column;

    // It is not very efficient to allocate two buffers, one here and
    // one inside the string object, but the std::string API doesn't
    // offer a way to do it directly, so I need to refactor my APIs if
    // I want to avoid the extra allocation.
    Array<char> buf(len+1);

    buf[len] = 0;              // NUL terminator
    getLineLoose(TextCoord(tc1.line, tc1.column), buf, len);
    string ret(buf.ptrC());    // Explicitly calling 'ptrC' is only needed for Eclipse...

    return ret;
  }

  xassert(tc1.line < tc2.line);

  // build up returned string
  stringBuilder sb;

  // final fragment of line1
  sb = getTextRange(tc1,
    TextCoord(tc1.line, max(tc1.column, lineLengthLoose(tc1.line))));

  // full lines between line1 and line2
  for (int i=tc1.line+1; i < tc2.line; i++) {
    sb << "\n";
    sb << getTextRange(TextCoord(i, 0), lineEndCoord(i));
  }

  // initial fragment of line2
  sb << "\n";
  sb << getTextRange(TextCoord(tc2.line, 0), tc2);

  return sb;
}


string TextDocumentEditor::getWholeLine(int line) const
{
  xassert(line >= 0);
  if (line < m_doc->numLines()) {
    return getTextRange(TextCoord(line, 0),
                        TextCoord(line, m_doc->lineLength(line)));
  }
  else {
    return string("");
  }
}


string TextDocumentEditor::getWordAfter(TextCoord tc) const
{
  stringBuilder sb;

  if (!( 0 <= tc.line && tc.line < numLines() )) {
    return "";
  }

  bool seenWordChar = false;
  while (tc.column < lineLength(tc.line)) {
    char ch = getTextRange(tc, TextCoord(tc.line, tc.column+1))[0];
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
    tc.column++;
  }

  return sb;
}


int TextDocumentEditor::countLeadingSpaceChars(int line) const
{
  string contents = getWholeLine(line);
  char const *p = contents.c_str();
  while (*p && isspace(*p)) {
    p++;
  }
  return p - contents.c_str();
}


int TextDocumentEditor::getIndentation(int line) const
{
  string contents = getWholeLine(line);
  for (char const *p = contents.c_str(); *p; p++) {
    if (!isspace(*p)) {
      // found non-ws char
      return p - contents.c_str();
    }
  }
  return -1;   // no non-ws
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


bool TextDocumentEditor::findString(TextCoord /*INOUT*/ &tc, char const *text,
                                    FindStringFlags flags) const
{
  int textLen = strlen(text);

  // This is questionable because it can lead to finding matches that
  // appear before the specified search location.  I originally added
  // this line in f0169061da, when I added undo/redo support, which
  // suggests it was needed to deal with cases arising from replaying
  // history elements.  Probably there is a better solution.
  this->truncateCoord(tc);

  if (flags & FS_ADVANCE_ONCE) {
    this->walkCoord(tc, flags&FS_BACKWARDS? -1 : +1);
  }

  // contents of current line, in a growable buffer
  GrowArray<char> contents(10);

  while (0 <= tc.line && tc.line < numLines()) {
    // get line contents
    int lineLen = lineLength(tc.line);
    contents.ensureIndexDoubler(lineLen);
    m_doc->getLine(TextCoord(tc.line, 0), contents.getDangerousWritableArray(), lineLen);

    // search for 'text' using naive algorithm, starting at 'tc.column'
    while (0 <= tc.column && tc.column+textLen <= lineLen) {
      bool found =
        (flags & FS_CASE_INSENSITIVE) ?
          (0==strncasecmp(contents.getArray()+tc.column, text, textLen)) :
          (0==strncmp(contents.getArray()+tc.column, text, textLen))     ;

      if (found) {
        // found match
        return true;
      }

      if (flags & FS_BACKWARDS) {
        tc.column--;
      }
      else {
        tc.column++;
      }
    }

    if (flags & FS_ONE_LINE) {
      break;
    }

    // wrap to next line
    if (flags & FS_BACKWARDS) {
      tc.line--;
      if (tc.line >= 0) {
        tc.column = lineLength(tc.line)-textLen;
      }
    }
    else {
      tc.column = 0;
      tc.line++;
    }
  }

  return false;
}


// ---------------- TextDocumentEditor: modifications ------------------

void TextDocumentEditor::moveCursorBy(int deltaLine, int deltaCol)
{
  // prevent moving into negative territory
  deltaLine = max(deltaLine, - cursor().line);
  deltaCol = max(deltaCol, - cursor().column);

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
  int prevLine = max(0, cursor().line - 1);
  moveCursor(false /*relLine*/, prevLine,
             false /*relCol*/, lineLengthLoose(prevLine));
}


void TextDocumentEditor::moveCursorToTop()
{
  setCursor(TextCoord(0, 0));
  scrollToCursor();
}

void TextDocumentEditor::moveCursorToBottom()
{
  setCursor(TextCoord(m_doc->numLines() - 1, 0));
  scrollToCursor();
}


void TextDocumentEditor::advanceWithWrap(bool backwards)
{
  int line = cursor().line;
  int col = cursor().column;

  if (!backwards) {
    if (0 <= line &&
        line < numLines() &&
        col < cursorLineLength()) {
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
  m_cursor.line =
    max(m_firstVisible.line,
      min(m_lastVisible.line,   m_cursor.line));

  m_cursor.column =
    max(m_firstVisible.column,
      min(m_lastVisible.column, m_cursor.column));
}


void TextDocumentEditor::walkCursor(int distance)
{
  TextCoord tc = m_cursor;
  bool ok = this->walkCoord(tc, distance);
  xassert(ok);
  m_cursor = tc;
}


void TextDocumentEditor::fillToCursor()
{
  int rowfill, colfill;
  computeSpaceFill(this->core(), cursor(), rowfill, colfill);

  if (rowfill==0 && colfill==0) {
    return;     // nothing to do
  }

  // The cursor itself should automatically end up where it started,
  // but during that process we might trigger a scroll action.  The
  // restorer will ensure that too is undone.
  CursorRestorer restorer(*this);

  TextCoord orig = cursor();

  // move back to defined area, and end of that line
  moveCursor(true /*relLine*/, -rowfill,
             false /*relCol*/, lineLength(orig.line-rowfill));
  xassert(validCursor());

  // add newlines
  while (rowfill--) {
    insertNulTermText("\n");
  }

  // add spaces
  while (colfill--) {
    insertSpace();
  }

  // should have ended up in the same place we started
  xassert(orig == cursor());
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
  int overEdge = cursor().column - cursorLineLength();
  if (overEdge > 0) {
    // move back to the end of this line
    moveCursorBy(0, -overEdge);
  }

  fillToCursor();      // might add newlines up to this point
  insertNulTermText("\n");
}


void TextDocumentEditor::insertNewlineAutoIndent()
{
  // The code below this assumes cursor > mark if mark is active.
  this->normalizeCursorGTEMark();

  // Will we be carrying text forward onto the new line?
  bool hadCharsToRight = (m_cursor.column < this->cursorLineLength());

  // typing replaces selection
  this->deleteSelectionIf();

  // Actually insert the newline character.  This will scroll
  // to the left edge too.
  this->insertNewline();

  // auto-indent
  int ind = this->getAboveIndentation(m_cursor.line - 1);
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


void TextDocumentEditor::deleteTextRange(TextCoord tc1, TextCoord tc2)
{
  xassert(tc1.nonNegative());
  xassert(tc2.nonNegative());
  xassert(tc1 <= tc2);

  // truncate the endpoints
  this->truncateCoord(tc1);
  this->truncateCoord(tc2);

  // go to line2/col2, which is probably where the cursor already is
  setCursor(tc2);

  // compute # of chars in span
  int length = computeSpanLength(this->core(), tc1, tc2);

  // delete them as a left deletion; the idea is I suspect the
  // original and final cursor are line2/col2, in which case the
  // cursor movement can be elided (by automatic history compression)
  deleteLR(true /*left*/, length);

  // the cursor automatically ends up at line1/col1, as our spec
  // demands
}


void TextDocumentEditor::indentLines(int start, int lines, int ind)
{
  CursorRestorer cr(*this);

  for (int line=start; line < start+lines &&
                       line < this->numLines(); line++) {
    this->setCursor(TextCoord(line, 0));

    if (ind > 0) {
      if (this->lineLength(line) == 0) {
        // Do not add spaces to a blank line.
      }
      else {
        for (int i=0; i<ind; i++) {
          this->insertSpace();
        }
      }
    }

    else {
      int lineInd = this->countLeadingSpaceChars(line);
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

  TextCoord selLow, selHigh;
  this->getSelectRegion(selLow, selHigh);

  int endLine = (selHigh.column==0? selHigh.line-1 : selHigh.line);
  this->indentLines(selLow.line, endLine-selLow.line+1, amt);

  return true;
}


bool TextDocumentEditor::justifyNearCursor(int desiredWidth)
{
  bool ret = justifyNearLine(*this, m_cursor.line, desiredWidth);
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

  this->deleteSelectionIf();
  this->insertString(dt);
  this->scrollToCursor();
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
  this->fillToCursor();
  this->deleteSelectionIf();
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
  : tde(e),
    cursor(e.cursor()),
    markActive(e.markActive()),
    mark(markActive? e.mark() : TextCoord()),
    firstVisible(e.firstVisible())
{}

CursorRestorer::~CursorRestorer()
{
  tde.setCursor(cursor);
  if (markActive) {
    tde.setMark(mark);
  }
  else {
    tde.clearMark();
  }
  tde.setFirstVisible(firstVisible);
}


// EOF
