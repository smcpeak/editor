// history.cc
// code for history.h


static void rollbackMismatch()
{
  xfailure("correspondence mismatch during rollback!");
}

// operations that should not fail, because they were just applied
// successfully in the opposite direction, are enclosed in this
#define ROLLBACK(stmt)            \
  try {                           \
    stmt;                         \
  }                               \
  catch (XHistory &rollFail) {    \
    rollbackMismatch();           \
  }


// ----------------- CursorBuffer -----------------------
CursorBuffer::CursorBuffer()
  : BufferCore(),
    line(0),
    col(0)
{}  


// -------------------- HistoryElt ----------------------
HistoryElt::~HistoryElt()
{}


// --------------------- HE_cursor ----------------------
STATICDEF void HE_text::move1dim(int &value, int orig, int update, bool reverse)
{
  if (orig == -1) {
    // relative update
    if (reverse) {
      update = -update;
    }
    if (value + update < 0) {
      throw XHistory("relative update yields negative result");
    }
    // ==> committed
    value += update;
  }

  else {
    // absolute update
    if (reverse) {
      int temp = orig;
      orig = update;      // swap
      update = temp;
    }

    if (orig != value) {
      throw XHistory("absolute update has wrong original value");
    }
    xassert(update >= 0);
    // ==> committed
    value = update;
  }
}


void HE_cursor::apply(CursorBuffer &buf, bool reverse)
{
  static_apply(buf, origLine, line, origCol, col, reverse);
}

STATICDEF void HE_cursor::move(CursorBuffer &buf, int origLine, int line,
                               int origCol, int col, bool reverse)
{
  move1dim(buf.line, origLine, line, reverse);
  try {
    move1dim(buf.col, origCol, col, reverse);
  }
  catch (XHistory &x) {
    // undo the line movement
    ROLLBACK( move1dim(buf.line, origLine, line, !reverse) );
    throw;
  }
}


// ------------------------ HE_text ----------------------
HE_text::HE_text(bool i, bool l, int f,
                 char const *t, int len);
  : insertion(i), left(l), fill(f), textLen(len)
{
  text = new char[textLen];
  memcpy(text, t, textLen);
}

HE_text::~HE_text()
{
  delete text;
}



void HE_text::apply(CursorBuffer &buf, bool reverse)
{
  static_apply(buf, insertion, left, fill, text, textLen, reverse);
}

STATICDEF void HE_text::static_apply(
  CursorBuffer &buf, bool insertion, bool left, int fill, 
  char const *text, int textLen, bool reverse)
{
  if (!fill) {
    // this is the common case, and is much simpler than what follows
    if (insertion) {
      insert(buf, text, textLen, left, reverse);
    }
    else {
      insert(buf, text, textLen, !left, !reverse);
    }
  }

  // the logic here is a little complicated because this operation is
  // essentially the composition of two simpler ones (fill and
  // insert/delete), and I need to make sure that the buffer isn't
  // modified if an XHistory gets thrown, which means a successful
  // first operation might have to be undone if the second one fails

  // also, since the inverse of a LEFT insertion is a RIGHT deletion,
  // and vice-versa, 'left' must be effectively XOR'd with whether
  // we're doing a deletion

  if (!reverse) {
    // forward insertion or deletion
    fill(buf, fill, false /*reverse*/);    // might throw, that's ok
    try {
      if (insertion) {
        insert(buf, text, textLen, left, false /*reverse*/);
      }
      else {
        // 'left' deletion == reverse NOT('left') insertion
        insert(buf, text, textLen, !left, true /*reverse*/);
      }
    }
    catch (XHistory &x) {
      // undo the fill
      ROLLBACK( fill(buf, fill, true /*reverse*/) );
      throw;
    }
  }

  else {
    if (insertion) {                   
      // reverse 'left' insertion
      insert(buf, text, textLen, left, true /*reverse*/);
    }
    else {
      // reverse 'left' deletion == forward NOT('left') insertion
      insert(buf, text, textLen, !left, false /*reverse*/);
    }
    try {
      fill(buf, fill, true /*reverse*/);
    }
    catch (XHistory &x) {
      // undo the insertion/deletion
      if (insertion) {
        ROLLBACK( insert(buf, text, textLen, left, false /*reverse*/) );
      }
      else {
        ROLLBACK( insert(buf, text, textLen, !left, true /*reverse*/) );
      }
      throw;
    }
  }
}


STATICDEF void HE_text::fill(CursorBuffer &buf, int fill, bool reverse)
{
  if (!fill) { return; }
  xassert(fill > 0);

  // NOTE: since fill and un-fill are rare, I don't worry about
  // making them fast

  if (!reverse) {
    // fill to cursor
    int rowfill, int colfill;
    computeBothFill(buf, rowfill, colfill);

    if (fill < rowfill+colfill) {
      throw XHistory("insufficient fill chars");
    }
    else if (fill > rowfill+colfill) {
      throw XHistory("too many fill chars");
    }

    // ==> committed
    while (rowfill > 0) {
      buf.insertLine(buf.numLines());
      rowfill--;
    }
    fillRight(buf, colfill);
  }

  else {
    // un-fill

    // we have to be at the end of the cursor's line
    if (buf.col != buf.lineLength(buf.line)) {
      throw XHistory("unfill: cursor is not at EOL");
    }

    if (buf.lineLength(buf.line) >= fill) {
      // case 1: just remove spaces from the end of some line

      // check that it's all spaces
      if (!onlySpaces(buf, fill)) {
        throw XHistory("unfill: found non-space char at EOL");
      }

      // ==> committed
      
      // remove them
      buf.deleteText(buf.line, buf.col - fill, fill);
    }

    else {
      // case 2: remove all of last line, plus some lines at EOF
      if (buf.line != buf.numLines()-1) {
        throw XHistory("unfill: fill exceeds line length, but cursor not at EOF");
      }

      int colGap = buf.lineLength(buf.line);
      int lineGap = fill - colGap;

      xassert(colGap >= 0);
      xassert(lineGap > 0);

      // check that last line is entirely spaces
      if (!onlySpaces(buf, colGap)) {
        throw XHistory("unfill: found non-space char at EOL");
      }

      // check that lines in the gap, other than the last (that's why
      // the loop starts at 1 and not 0), are all empty
      int i;
      for (i=1; i<lineGap; i++) {
        if (buf.lineLength(buf.line-i) != 0) {
          throw XHistory("unfill: non-empty fill line at EOF");
        }
      }

      // ==> committed

      // remove contents of last line
      buf.deleteText(buf.line, buf.col - colGap, colGap);

      // remove last 'lineGap' lines
      for (i=0; i<lineGap; i++) {
        buf.deleteLine(buf.line - i);
      }
    }
  }
}


// add spaces to the end of the line, so that the cursor is
// referring to the edge of defined text
STATICDEF void HE_text::fillRight(CursorBuffer &buf, int fill)
{
  while (fill > 0) {
    buf.insertText(line, buf.col - fill, " ", 1);
    fill--;
  }
}


// check that there are only spaces in the 'amt' characters
// to the left of the cursor
STATICDEF bool HE_text::onlySpaces(CursorBuffer &buf, int amt)
{
  for (int i=0; i < amt; i++) {
    char ch;
    buf.getLine(buf.line, buf.col-i-1, &ch, 1);
    if (ch != ' ') {
      return false;
    }
  }
  return true;
}


void HE_text::computeFill(CursorBuffer &buf)
{
  int rowfill, int colfill;
  computeBothFill(buf, rowfill, colfill);
  fill = rowfill+colfill;
}


static void deletionMismatch()
{
  throw XHistory("deletion text does not match buffer contents");
}

// insert (=forward) or delete (=reverse) some text at the cursor
STATICDEF void HE_text::insert(
  CursorBuffer &buf, char const *text, int textLen, 
  bool left, bool reverse)
{
  // cursor should now be within the text area
  if (!buf.cursorInDefined()) {
    throw XHistory("cursor is not within text area");
  }

  if (!reverse) {
    // insertion
    // ==> committed
    int line = buf.line;
    int col = buf.col;

    char const *p = text;
    char const *end = text+textLen;
    while (p < end) {
      // find next newline, or 'end'
      char const *nl = p;
      while (nl < end && *nl!='\n') {
        nl++;
      }

      // length of this segment
      int len = nl-p;

      // insert this text at line/col
      insertText(line, col, p, len);
      col += len;

      // insert newline
      if (nl < end) {
        line++;
        col = 0;
        buf.insertLine(line);
        len++;   // so we skip '\n' too
      }

      // skip consumed text
      p += len;
    }
    xassert(p == end);

    if (!left) {
      // put cursor at end of inserted text
      buf.line = line;
      buf.col = col;
    }
  }

  else {
    // deletion
    
    // location of the left edge of the text to delete; this will be
    // changed below if we're actually doing a right insertion (I
    // cannot use buf.line/col for this purpose, since I don't want to
    // modify them until after I'm committed to performing the
    // operation)
    int beginLine = buf.line;
    int beginCol = buf.col;

    // cursor for the purposes of this function
    int line = beginLine;
    int col = beginCol;

    // a reverse left insertion is a right deletion.. so XOR it again..
    left = !left;

    if (left) {
      // left deletion: handle this by walking the cursor backwards
      // the length of the expected deletion text to find the location
      // of the start of that text in the buffer
      if (!walkBackwards(buf, line, col, textLen)) {
        deletionMismatch();
      }

      // this is the actual beginning
      beginLine = line;
      beginCol = col;
    }

    // from here on we're doing a right deletion, regardless of
    // what 'left' says

    // check correspondence between the text in the event record and
    // what's in the buffer, without modifying the buffer yet
    Array<char> actualText(textLen);
    if (!getTextSpan(buf, line, col, actualText, textLen)) {
      deletionMismatch();      // span isn't valid
    }
    if (0!=memcmp(text, actualText, textLen)) {
      deletionMismatch();      // text doesn't match
    }

    // ==> committed

    // contents are known to match, so delete the text
    line = beginLine;
    col = beginCol;
    p = text;
    while (p < end) {
      // find next newline, or 'end'
      char const *nl = p;
      while (nl < end && *nl!='\n') {
        nl++;
      }

      // length of this segment
      int len = nl-p;

      // delete the segment
      buf.deleteText(line, col, len);

      // bypass newline
      if (nl < end) {
        // we deleted all text on this line after 'col'
        xassert(buf.lineLength(line) == col);

        if (col == 0) {
          // we're at the beginning of a line, and it is now empty,
          // so just delete this line
          buf.deleteLine(line);
        }
        else {
          // move line/col to beginning of next line
          line++;
          col=0;
        }
        len++;   // so we skip '\n' too
      }

      // skip consumed text
      p += len;
    }

    // update buffer cursor; this only changes it if we were doing a
    // *left* deletion
    buf.line = beginLine;
    buf.col = beginCol;
    xassert(buf.cursorInDefined());
  }
}


void HE_text::computeText(CursorBuffer const &buf, int count)
{
  xassert(insertion == false);
  xassert(text == NULL);
  xassert(buf.cursorInDefined());

  int line = buf.line;
  int col = buf.col;

  if (left) {
    if (!walkBackwards(buf, line, col, count)) {
      xfailure("deletion span is not entirely within defined text area");
    }
  }

  text = new char[count];
  if (!getTextSpan(buf, line, col, text, count)) {
    delete[] text;
    text = NULL;
    xfailure("deletion span is not entirely within defined text area");
  }
  textLen = count;
}


// ----------------------- HE_group -----------------------
HE_group::HE_group()
  : seq()
{}

HE_group::~HE_group()
{
  seq.clear();    // happens automatically, but no harm in doing explicitly also
}


void HE_group::append(HistoryElt *e)
{
  seq.insert(seqLength(), e);
}


void HE_group::truncate(int newLength)
{
  xassert(0 <= newLength && newLength <= seqLength());
  
  while (newLength < seqLength()) {
    seq.deleteElt(newLength);
  }
}


HistoryElt *HE_group::elt(int start, int end, bool reverse, int offset)
{
  if (reverse) {
    offset = (end-start) - offset - 1;
  }
  return seq.get(start+index);
}


void HE_group::applySeq(CursorBuffer &buf, int start, int end, bool reverse)
{
  int i;
  try {
    for (i=0; i < (end-start); i++) {
      elt(start, end, reverse, i)->apply(buf, reverse);
    }
  }
  catch (XHistory &x) {
    // the (start+i)th element failed; roll back all preceding ones
    try {
      for (int j=i-1; j>=0; j--) {
        elt(start, end, reverse, j)->apply(buf, !reverse);
      }
    }
    catch (XHistory &y) {
      rollbackMismatch();
    }
    throw;
  }
}


void HE_group::applyOne(CursorBuffer &buf, int index, bool reverse)
{
  seq.get(index)->apply(buf, reverse);
}


void HE_group::apply(CursorBuffer &buf, bool reverse)
{
  applySeq(buf, 0, seqLength(), reverse);
}


// EOF
