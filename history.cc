// history.cc
// code for history.h

#include "history.h"       // this module
#include "array.h"         // Array
#include "strutil.h"       // encodeWithEscapes


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
STATICDEF void HE_cursor::move1dim(int &value, int orig, int update, bool reverse)
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

STATICDEF void HE_cursor::static_apply
  (CursorBuffer &buf, int origLine, int line,
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


STATICDEF void HE_cursor::print1dim(stringBuilder &sb, int orig, int val)
{
  if (orig == -1) {
    // relative
    if (val >= 0) {
      sb << "+" << val;
    }
    else {
      sb << val;
    }
  }
  else {
    // absolute
    sb << orig << "->" << val;
  }
}

void HE_cursor::print(stringBuilder &sb, int indent) const
{
  sb.indent(indent);
  sb << "move(";
  print1dim(sb, origLine, line);
  sb << ", ";
  print1dim(sb, origCol, col);
  sb << ");\n";
}


// ------------------------ HE_text ----------------------
HE_text::HE_text(bool i, bool l,
                 char const *t, int len)
  : insertion(i), left(l), textLen(len)
{
  if (textLen > 0) {
    text = new char[textLen];
    memcpy(text, t, textLen);
  }
  else {
    text = NULL;
  }
}

HE_text::~HE_text()
{
  if (text) {
    delete[] text;
  }
}



void HE_text::apply(CursorBuffer &buf, bool reverse)
{
  static_apply(buf, insertion, left, text, textLen, reverse);
}

STATICDEF void HE_text::static_apply(
  CursorBuffer &buf, bool insertion, bool left,
  char const *text, int textLen, bool reverse)
{
  if (insertion) {
    insert(buf, text, textLen, left, reverse);
  }
  else {
    insert(buf, text, textLen, !left, !reverse);
  }
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

    // excess text on the original line that gets floated down
    // to after the cursor on the last line
    GrowArray<char> excess(0);

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
      buf.insertText(line, col, p, len);
      col += len;

      // insert newline
      if (nl < end) {
        // if there is text beyond 'col' on 'line-1', then that text
        // gets floated down to the end of the insertion
        if (line==buf.line &&     // optimization: can only happen on first line
            col < buf.lineLength(line)) {
          // this can only happen on the first line of the insertion
          // procedure, so check that we don't already have excess
          xassert(excess.size()==0);

          // get the excess
          excess.setSize(buf.lineLength(line) - col);
          buf.getLine(line, col, excess.getArrayNC(), excess.size());

          // remove it from the buffer
          buf.deleteText(line, col, excess.size());
        }

        line++;
        buf.insertLine(line);
        col = 0;
        len++;   // so we skip '\n' too
      }

      // skip consumed text
      p += len;
    }
    xassert(p == end);

    // insert the floated excess text, if any
    if (excess.size() > 0) {
      buf.insertText(line, col, excess.getArray(), excess.size());
    }

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

    // splice to perform at end?
    int pendingSplice = 0;

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
    if (0!=memcmp(text, (char const*)actualText, textLen)) {
      deletionMismatch();      // text doesn't match
    }

    // ==> committed

    // contents are known to match, so delete the text
    line = beginLine;
    col = beginCol;
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
          // move line/col to beginning of next line, so that from
          // now on we can work with whole deleted lines, but remember
          // that there's a pending line splice
          line++;
          col=0;
          pendingSplice++;
        }
        len++;   // so we skip '\n' too
      }

      // skip consumed text
      p += len;
    }

    if (pendingSplice) {
      xassert(pendingSplice == 1);       // should only have one splice
      xassert(col == 0);                 // it's this entire line that goes
      
      // grab this line's contents
      int spliceLen = buf.lineLength(line);
      Array<char> splice(spliceLen);
      buf.getLine(line, col, splice, spliceLen);
      
      // blow it away
      buf.deleteText(line, col, spliceLen);
      buf.deleteLine(line);
      
      // move up to end of previous line
      line--;
      col = buf.lineLength(line);
      
      // append splice text
      buf.insertText(line, col, splice, spliceLen);
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


void HE_text::print(stringBuilder &sb, int indent) const
{
  sb.indent(indent);
  sb << (left? "left" : "right") 
     << (insertion? "Ins" : "Del") 
     << "(" << encodeWithEscapes(text, textLen) << ");\n";
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


HistoryElt *HE_group::stealFirstElt()
{
  return seq.remove(0);
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
  return seq.get(start+offset);
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


void HE_group::printWithMark(stringBuilder &sb, int indent, int n) const
{
  sb.indent(indent) << "group {\n";
  for (int i=0; i < seqLength(); i++) {
    if (i==n) {
      // print mark in left margin
      sb << "->";
      seq.getC(i)->print(sb, indent);
    }
    else {
      seq.getC(i)->print(sb, indent+2);
    }
  }
  sb.indent(indent) << "}";
  
  //if (n >= 0) {
  //  sb << " /""*time=" << n << "*/";
  //} 

  sb << "\n";
}


void HE_group::print(stringBuilder &sb, int indent) const
{
  printWithMark(sb, indent, -1 /*mark*/);
}


// EOF
