// td-editor.cc
// Code for td-editor.h.

#include "td-editor.h"                 // this module


TextDocumentEditor::TextDocumentEditor(TextDocument *doc)
  : m_doc(doc),
    m_cursor(),
    m_markActive(false),
    m_mark(),
    m_firstVisible(),

    // This size isn't intended to be user-visible since the client
    // code ought to set the size.  But if they do not, 20x60 will
    // be distinct and recognizable while also being serviceable.
    m_lastVisible(19, 59)
{
  selfCheck();
}


TextDocumentEditor::~TextDocumentEditor()
{}


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
  return doc()->validCoord(cursor());
}


bool TextDocumentEditor::cursorAtEnd() const
{
  return this->m_cursor == endCoord(doc()->getCore());
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


void TextDocumentEditor::setFirstVisible(TextCoord fv)
{
  xassert(fv.nonNegative());
  int h = m_lastVisible.line - m_firstVisible.line;
  int w = m_lastVisible.column - m_firstVisible.column;
  m_firstVisible = fv;
  m_lastVisible.line = fv.line + h;
  m_lastVisible.column = fv.line + w;
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
  // TODO: I would like to enable this assertion.  That is dependent on
  // eliminating the awkward selLowLine, etc., since they call this
  // without checking markActive().
  //xassert(markActive());
  return m_mark;
}


void TextDocumentEditor::insertLR(bool left, char const *text, int textLen)
{
  xassert(validCursor());

  doc()->insertAt(cursor(), text, textLen);

  if (!left) {
    // Put the cursor at the end of the inserted text.
    TextCoord tc = cursor();
    bool ok = walkCursor(doc()->getCore(), tc, textLen);
    xassert(ok);
    setCursor(tc);
  }
}


void TextDocumentEditor::deleteLR(bool left, int count)
{
  xassert(validCursor());

  if (left) {
    // Move the cursor to the start of the text to delete.
    TextCoord tc = cursor();
    bool ok = walkCursor(doc()->getCore(), tc, -count);
    xassert(ok);
    setCursor(tc);
  }

  doc()->deleteAt(cursor(), count);
}


void TextDocumentEditor::undo()
{
  setCursor(doc()->undo());
}


void TextDocumentEditor::redo()
{
  setCursor(doc()->redo());
}


STATICDEF void TextDocumentEditor::assertNonNegative(int line, int col)
{
  xassert(line >= 0);
  xassert(col >= 0);
}


// ---------------- TextDocumentEditor: queries ------------------

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


void TextDocumentEditor::getLineLoose(int line, int col, char *dest, int destLen) const
{
  assertNonNegative(line, col);

  // how much of the source is in the defined region?
  int undef = destLen;
  int def = 0;

  if (line < numLines() &&
      col < lineLength(line)) {
    //       <----- lineLength -------->
    // line: [-------------------------][..spaces...]
    //       <------ col -----><----- destlen ------>
    //                         <- def -><-- undef -->
    // dest:                   [--------------------]

    undef = max(0, (col+destLen) - lineLength(line));
    def = max(0, destLen - undef);
  }

  // initial part in defined region
  if (def) {
    doc()->getLine(line, col, dest, def);
  }

  // spaces past defined region
  memset(dest+def, ' ', undef);
}


string TextDocumentEditor::getTextRange(int line1, int col1, int line2, int col2) const
{
  assertNonNegative(line1, col1);
  assertNonNegative(line2, col2);

  // this function uses the line1==line2 case as a base case of a two
  // level recursion; it's not terribly efficient

  if (line1 == line2) {
    // extracting text from a single line
    xassert(col1 <= col2);
    int len = col2-col1;

    // It is not very efficient to allocate two buffers, one here and
    // one inside the string object, but the std::string API doesn't
    // offer a way to do it directly, so I need to refactor my APIs if
    // I want to avoid the extra allocation.
    char *buf = new char[len+1];

    buf[len] = 0;              // NUL terminator
    getLineLoose(line1, col1, buf, len);
    string ret(buf);
    delete[] buf;
    return ret;
  }

  xassert(line1 < line2);

  // build up returned string
  stringBuilder sb;

  // final fragment of line1
  sb = getTextRange(line1, col1, line1, max(col1, lineLengthLoose(line1)));

  // full lines between line1 and line2
  for (int i=line1+1; i < line2; i++) {
    sb << "\n";
    sb << getTextRange(i, 0, i, lineLengthLoose(i));
  }

  // initial fragment of line2
  sb << "\n";
  sb << getTextRange(line2, 0, line2, col2);

  return sb;
}


string TextDocumentEditor::getWordAfter(int line, int col) const
{
  stringBuilder sb;

  if (!( 0 <= line && line < numLines() )) {
    return "";
  }

  bool seenWordChar = false;
  while (col < lineLength(line)) {
    char ch = getTextRange(line, col, line, col+1)[0];
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
    col++;
  }

  return sb;
}


void TextDocumentEditor::getLastPos(int &line, int &col) const
{
  line = numLines()-1;
  if (line >= 0) {
    col = lineLength(line);
  }
  else {
    col = 0;
  }
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


bool TextDocumentEditor::findString(int &userLine, int &userCol, char const *text,
                        FindStringFlags flags) const
{
  TextCoord tc(userLine, userCol);
  int textLen = strlen(text);

  // This is questionable because it can lead to finding matches that
  // appear before the specified search location.  I originally added
  // this line in f0169061da, when I added undo/redo support, which
  // suggests it was needed to deal with cases arising from replaying
  // history elements.  Probably there is a better solution.
  truncateCursor(doc()->getCore(), tc);

  if (flags & FS_ADVANCE_ONCE) {
    walkCursor(doc()->getCore(), tc,
               flags&FS_BACKWARDS? -1 : +1);
  }

  // contents of current line, in a growable buffer
  GrowArray<char> contents(10);

  while (0 <= tc.line && tc.line < numLines()) {
    // get line contents
    int lineLen = lineLength(tc.line);
    contents.ensureIndexDoubler(lineLen);
    doc()->getLine(tc.line, 0, contents.getDangerousWritableArray(), lineLen);

    // search for 'text' using naive algorithm, starting at 'tc.column'
    while (0 <= tc.column && tc.column+textLen <= lineLen) {
      bool found =
        (flags & FS_CASE_INSENSITIVE) ?
          (0==strncasecmp(contents.getArray()+tc.column, text, textLen)) :
          (0==strncmp(contents.getArray()+tc.column, text, textLen))     ;

      if (found) {
        // found match
        userLine = tc.line;
        userCol = tc.column;
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
  deltaLine = max(deltaLine, -line());
  deltaCol = max(deltaCol, -col());

  if (deltaLine || deltaCol) {
    moveCursor(true /*relLine*/, deltaLine,
               true /*relCol*/, deltaCol);
  }
}

void TextDocumentEditor::moveAbsCursor(int newLine, int newCol)
{
  moveCursor(false /*relLine*/, newLine,
             false /*relCol*/, newCol);
}


void TextDocumentEditor::moveAbsColumn(int newCol)
{
  moveCursor(true /*relLine*/, 0,
             false /*relCol*/, newCol);
}


void TextDocumentEditor::moveRelCursorTo(int newLine, int newCol)
{
  moveRelCursor(newLine-line(), newCol-col());
}

void TextDocumentEditor::moveToNextLineStart()
{
  moveCursor(true /*relLine*/, +1,
             false /*relCol*/, 0);
}

void TextDocumentEditor::moveToPrevLineEnd()
{
  moveCursor(true /*relLine*/, -1,
             false /*relCol*/, lineLength(line()-1));
}


void TextDocumentEditor::selectCursorLine()
{
  // Move the cursor to the start of its line.
  this->moveAbsColumn(0);

  // Make the selection end at the start of the next line.
  this->setMark(TextCoord(this->cursor().line + 1, 0));
}


void TextDocumentEditor::advanceWithWrap(bool backwards)
{
  if (!backwards) {
    if (0 <= line() &&
        line() < numLines() &&
        col() < lineLength(line())) {
      moveRelCursor(0, 1);
    }
    else {
      moveToNextLineStart();
    }
  }

  else {
    if (0 <= line() &&
        line() < numLines() &&
        col() >= 0) {
      moveRelCursor(0, -1);
    }
    else if (line() > 0) {
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
  computeSpaceFill(doc()->getCore(), cursor(), rowfill, colfill);

  if (rowfill==0 && colfill==0) {
    return;     // nothing to do
  }

  int origLine = line();
  int origCol = col();

  // move back to defined area, and end of that line
  //moveRelCursor(-rowfill, -colfill);    // wrong
  moveCursor(true /*relLine*/, -rowfill,
             false /*relCol*/, lineLength(origLine-rowfill));
  xassert(validCursor());

  // add newlines
  while (rowfill--) {
    insertText("\n");
  }

  // add spaces
  while (colfill--) {
    insertSpace();
  }

  // should have ended up in the same place we started
  xassert(origLine==line() && origCol==col());
}


void TextDocumentEditor::insertText(char const *text)
{
  insertLR(false /*left*/, text, strlen(text));
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
  int overEdge = col() - lineLengthLoose(line());
  if (overEdge > 0) {
    // move back to the end of this line
    moveRelCursor(0, -overEdge);
  }

  fillToCursor();      // might add newlines up to this point
  insertText("\n");
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
    insertText(line, lineLength(line), temp, len);

    // now remove the next line
    deleteText(line+1, 0 /*col*/, len);
    deleteLine(line+1);
  }
}
#endif // 0


void TextDocumentEditor::deleteTextRange(int line1, int col1, int line2, int col2)
{
  assertNonNegative(line1, col1);
  assertNonNegative(line2, col2);

  TextCoord tc1(line1, col1);
  TextCoord tc2(line2, col2);
  xassert(tc1 <= tc2);

  // truncate the endpoints
  truncateCursor(doc()->getCore(), tc1);
  truncateCursor(doc()->getCore(), tc2);

  // go to line2/col2, which is probably where the cursor already is
  moveRelCursorTo(tc2.line, tc2.column);

  // compute # of chars in span
  int length = computeSpanLength(doc()->getCore(), tc1, tc2);

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
    moveRelCursorTo(line, 0);

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


// -------------------- CursorRestorer ------------------
CursorRestorer::CursorRestorer(TextDocumentEditor &d)
  : doc(d),
    orig(d.cursor())
{}

CursorRestorer::~CursorRestorer()
{
  doc.moveRelCursorTo(orig.line, orig.column);
}


// EOF