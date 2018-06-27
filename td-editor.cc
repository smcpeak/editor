// td-editor.cc
// Code for td-editor.h.

#include "td-editor.h"                 // this module


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
  return this->m_cursor == this->endCoord();
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


void TextDocumentEditor::getSelectRegionForCursor(TextCoord cursor,
  TextCoord &selLow, TextCoord &selHigh) const
{
  if (!this->markActive()) {
    selLow = selHigh = cursor;
  }
  else {
    TextCoord const m = this->mark();
    if (cursor <= m) {
      selLow = cursor;
      selHigh = m;
    }
    else {
      selLow = m;
      selHigh = cursor;
    }
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
  TextCoord tc = cursor();
  bool ok = walkCursor(this->core(), tc, textLen);
  xassert(ok);
  setCursor(tc);

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
    TextCoord tc = cursor();
    bool ok = walkCursor(this->core(), tc, -count);
    xassert(ok);
    setCursor(tc);
  }

  m_doc->deleteAt(cursor(), count);
}


void TextDocumentEditor::undo()
{
  setCursor(m_doc->undo());
}


void TextDocumentEditor::redo()
{
  setCursor(m_doc->redo());
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
    char *buf = new char[len+1];

    buf[len] = 0;              // NUL terminator
    getLineLoose(TextCoord(tc1.line, tc1.column), buf, len);
    string ret(buf);
    delete[] buf;
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
  truncateCursor(this->core(), tc);

  if (flags & FS_ADVANCE_ONCE) {
    walkCursor(this->core(), tc,
               flags&FS_BACKWARDS? -1 : +1);
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

void TextDocumentEditor::moveRelCursor(int deltaLine, int deltaCol)
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


void TextDocumentEditor::selectCursorLine()
{
  // Move the cursor to the start of its line.
  this->setCursorColumn(0);

  // Make the selection end at the start of the next line.
  this->setMark(TextCoord(this->cursor().line + 1, 0));
}


void TextDocumentEditor::advanceWithWrap(bool backwards)
{
  int line = cursor().line;
  int col = cursor().column;

  if (!backwards) {
    if (0 <= line &&
        line < numLines() &&
        col < cursorLineLength()) {
      moveRelCursor(0, 1);
    }
    else {
      moveToNextLineStart();
    }
  }

  else {
    if (0 <= line &&
        line < numLines() &&
        col > 0) {
      moveRelCursor(0, -1);
    }
    else if (line > 0) {
      moveToPrevLineEnd();
    }
    else {
      // cursor at buffer start.. do nothing, I guess
    }
  }
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
    moveRelCursor(0, -overEdge);
  }

  fillToCursor();      // might add newlines up to this point
  insertNulTermText("\n");
}


#if 0     // old?
void TextDocumentEditor::spliceNextLine(int line)
{
  xassert(0 <= line && line < numLines());

  // splice this line with the next
  if (line+1 < numLines()) {
    // append the contents of the next line
    int len = lineLength(line+1);
    Array<char> temp(len);
    getLine(line+1, 0 /*col*/, temp, len);
    insertNulTermText(line, lineLength(line), temp, len);

    // now remove the next line
    deleteText(line+1, 0 /*col*/, len);
    deleteLine(line+1);
  }
}
#endif // 0


void TextDocumentEditor::deleteTextRange(TextCoord tc1, TextCoord tc2)
{
  xassert(tc1.nonNegative());
  xassert(tc2.nonNegative());
  xassert(tc1 <= tc2);

  // truncate the endpoints
  truncateCursor(this->core(), tc1);
  truncateCursor(this->core(), tc2);

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
  if (start >= numLines() ||   // entire range beyond defined area
      lines <= 0 ||            // empty range
      ind == 0) {              // no actual change to the lines
    return;
  }

  CursorRestorer cr(*this);

  for (int line=start; line < start+lines &&
                       line < numLines(); line++) {
    setCursor(TextCoord(line, 0));

    if (ind > 0) {
      for (int i=0; i<ind; i++) {
        insertSpace();
      }
    }

    else {
      int lineInd = getIndentation(line);
      for (int i=0; i<(-ind) && i<lineInd; i++) {
        deleteChar();
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
  endLine = min(endLine, this->numLines()-1);
  this->indentLines(selLow.line, endLine-selLow.line+1, amt);

  return true;
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
