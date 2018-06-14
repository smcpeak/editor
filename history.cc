// history.cc
// code for history.h

#include "history.h"       // this module
#include "array.h"         // Array
#include "strutil.h"       // encodeWithEscapes

#include <stdio.h>         // printf


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
      THROW(XHistory("relative update yields negative result"));
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
      THROW(XHistory("absolute update has wrong original value"));
    }
    xassert(update >= 0);
    // ==> committed
    value = update;
  }
}


bool HE_cursor::apply(CursorBuffer &buf, bool reverse)
{
  return static_apply(buf, origLine, line, origCol, col, reverse);
}

STATICDEF bool HE_cursor::static_apply
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
  return false;     // cursor movement doesn't change buffer contents
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


void HE_cursor::stats(HistoryStats &stats) const
{
  stats.records++;
  stats.memUsage += sizeof(*this);
  stats.mallocObjects++;
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



bool HE_text::apply(CursorBuffer &buf, bool reverse)
{
  return static_apply(buf, insertion, left, text, textLen, reverse);
}

STATICDEF bool HE_text::static_apply(
  CursorBuffer &buf, bool insertion, bool left,
  char const *text, int textLen, bool reverse)
{
  if (insertion) {
    insert(buf, text, textLen, left, reverse);
  }
  else {
    insert(buf, text, textLen, !left, !reverse);
  }                  

  return textLen > 0;
}


static void deletionMismatch()
{
  THROW(XHistory("deletion text does not match buffer contents"));
}

// insert (=forward) or delete (=reverse) some text at the cursor
STATICDEF void HE_text::insert(
  CursorBuffer &buf, char const *text, int textLen,
  bool left, bool reverse)
{
  // cursor should now be within the text area
  if (!buf.cursorInDefined()) {
    THROW(XHistory("cursor is not within text area"));
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
      buf.insertText(line, col, splice.ptrC(), spliceLen);
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
     << "(\"" << encodeWithEscapes(text, textLen) << "\");\n";
}


void HE_text::stats(HistoryStats &stats) const
{
  stats.records++;
  stats.memUsage += sizeof(*this);
  stats.mallocObjects++;
  if (text) {
    stats.memUsage += textLen;
    stats.mallocObjects++;
  }
}


// ----------------------- HE_group -----------------------
HE_group::HE_group()
  : seq()
{}

HE_group::~HE_group()
{
  clear();
}


void HE_group::append(HistoryElt *e)
{
  HistoryEltCode code = encode(e);
  seq.insert(seqLength(), code);
}


HE_group::HistoryEltCode HE_group::encode(HistoryElt * /*owner*/ e)
{
  // General 32-bit bit pattern of a HistoryEltCode:
  //                                                    least sig. bit
  //    -------------- d --------------                              |
  //   /                               \ e f                         V
  //  +---------------+---------------+---------------+---------------+
  //  | | | | | | | | | | | | | | | | | | | | | | | | | | | | | | | | |
  //  +---------------+---------------+---------------+---------------+
  //   \             / \             / \             / \           / z
  //   ^----- a -----   ----- b -----   ----- c -----   -- opcode -
  //   |
  //   most sig. bit
  //
  // If z=0, then the entire bit pattern is interpreted as a HistoryElt*,
  // and the referent object is owned by the HE_group.  Otherwise:
  //
  // If opcode=0, it's a single-character insertion or deletion.
  //   d = the character code, as a 17-bit UNICODE value
  //   e = 1=ins, 0=del
  //   f = 1=left, 0=right
  //
  // If opcode=1, it's a rel/rel cursor movement.
  //   a = dline (signed 8-bit value)
  //   b = dcol
  //
  // If opcode=2, it's a rel/abs cursor movement.
  //   a = dline
  //   b = origCol (unsigned 8-bit value)
  //   c = newCol
  //
  // Of course, we only use the specialized encodings when the history
  // element values fit into the bits to which they've been assigned.

  // NOTE: Since I only read and write entire uintptr_t words from memory,
  // the endianness of the machine is irrelevant.

  // Also note:  Though I casually refer to the size of 'int' as being
  // 32 bits, in fact it could be 64 (or more) and this code should
  // still work.  However, it will *not* work if ints are smaller than
  // 32 bits.

  #define RETURN(val)                 \
    HistoryEltCode retcode = val;     \
    delete e;                         \
    return retcode /* user ; */

  switch (e->getTag()) {
    case HE_CURSOR: {
      HE_cursor *c = static_cast<HE_cursor*>(e);
      if (c->origLine == -1 &&
          -128 <= c->line && c->line <= 127) {
        if (c->origCol == -1 &&
            -128 <= c->col && c->col <= 127) {
          // encode with opcode 1
          RETURN(((c->line & 0xFF) << 24) |
                 ((c->col & 0xFF) << 16) |
                 (1/*opcode*/ << 1) |
                 1/*z*/);
        }

        if (c->origCol != -1 &&
            0 <= c->origCol && c->origCol <= 255 &&
            0 <= c->col && c->col <= 255) {
          // code with opcode 2
          RETURN(((c->line & 0xFF) << 24) |
                 ((c->origCol & 0xFF) << 16) |
                 ((c->col & 0xFF) << 8) |
                 (2/*opcode*/ << 1) |
                 1/*z*/);
        }
      }

      break;
    }

    case HE_TEXT: {
      HE_text *t = static_cast<HE_text*>(e);
      if (t->textLen == 1) {
        // encode with opcode 0
        int charCode = (unsigned char)t->text[0];   // don't sign-extend
        RETURN((charCode << 15) |
               (((int)t->insertion) << 14) |
               (((int)t->left) << 13) |
               /*opcode is 0*/
               1/*z*/);
      }

      break;
    }

    case HE_GROUP: {
      HE_group *g = static_cast<HE_group*>(e);
      if (g->seqLength() == 0) {
        // nothing in the sequence; I expect this to be rather common,
        // since I'm planning on opening/closing groups for every UI
        // action, many of which won't actually try to add a history
        // element
        //RETURN(0);   // special code to caller
        
        // update: I have to add exactly one element to the sequence
        // because the caller believes the sequence has increased in
        // length by exactly one.  So the caller takes care of this
        // particular optimization.
      }

      if (g->seqLength() == 1) {
        // only one element; I expect this to also be common, because
        // the majority of buffer modifications should end up as
        // singleton groups, again due to making a group for every UI
        // action
        RETURN(g->stealFirstEltCode());
      }

      // more than one element, will append it as a group
      g->squeezeReserved();

      break;
    }

    default: ;     // silence warning
  }

  // unknown type, or doesn't fit into 32-bit encoding
  return (HistoryEltCode)e;
}

                                
// take the low 8 bits of 'val', sign-extend them to 32, and
// return that; this assumes that 'char' is exactly 8 bits
static int se8bit(unsigned long val)
{
  char c = (char)(val & 0xFF);    // signed 8-bit qty
  return (int)c;                  // sign-extended
}

HistoryElt *HE_group::decode(HistoryEltCode code, bool &allocated) const
{
  if (!(code & 1)) {
    // ordinary pointer
    allocated = false;
    return (HistoryElt*)code;
  }

  // encoding, will make an object to represent it
  allocated = true;

  // get opcode
  switch ((code >> 1) & 0x7F) {
    default:
      xfailure("bad opcode in history element code");

    case 0: {     // single-char ins/del
      int charCodeInt = (code >> 15);
      char charCodeChar = (char)(unsigned char)charCodeInt;
      return new HE_text((code >> 14) & 1,         // insertion
                         (code >> 13) & 1,         // left
                         &charCodeChar,            // text
                         1);                       // textLen
    }

    case 1: {     // rel/rel cursor
      return new HE_cursor(-1,                     // origLine
                           se8bit(code >> 24),     // dline
                           -1,                     // origCol
                           se8bit(code >> 16));    // dcol
    }

    case 2: {     // rel/abs cursor
      return new HE_cursor(-1,                     // origLine
                           se8bit(code >> 24),     // dline
                           (code >> 16) & 0xFF,    // origCol
                           (code >> 8) & 0xFF);    // newCol
    }
  }
}


HE_group::HistoryEltCode HE_group::stealFirstEltCode()
{
  return seq.remove(0);
}


void HE_group::squeezeReserved()
{
  seq.squeezeGap();
}


void HE_group::truncate(int newLength)
{
  xassert(0 <= newLength && newLength <= seqLength());
  
  while (newLength < seqLength()) {
    HistoryEltCode code = seq.remove(newLength);
    
    if (code & 1) {
      // it's not a pointer, nothing to do
    }
    else {
      // it's a pointer, so deallocate
      delete (HistoryElt*)code;
    }
  }
}


bool HE_group::applySeqElt(CursorBuffer &buf, int start, int end, int offset,
                           bool reverseIndex, bool reverseOperation)
{
  if (reverseIndex) {
    offset = (end-start) - offset - 1;
  }
  return applyOne(buf, start+offset, reverseOperation);
}

bool HE_group::applySeq(CursorBuffer &buf, int start, int end, bool reverse)
{
  int i;
  try {
    bool modifies = false;
    for (i=0; i < (end-start); i++) {
      modifies = applySeqElt(buf, start, end, i, reverse, reverse) || modifies;
    }
    return modifies;
  }
  catch (XHistory &x) {
    // the (start+i)th element failed; roll back all preceding ones
    try {
      for (int j=i-1; j>=0; j--) {
        applySeqElt(buf, start, end, j, reverse, !reverse);
      }
    }
    catch (XHistory &y) {
      rollbackMismatch();
    }
    throw;
  }
}


bool HE_group::applyOne(CursorBuffer &buf, int index, bool reverse)
{
  bool allocated;
  HistoryElt *e = decode(seq.get(index), allocated);
  bool mod = e->apply(buf, reverse);
  if (allocated) {
    delete e;
  }          
  return mod;
}


bool HE_group::apply(CursorBuffer &buf, bool reverse)
{
  return applySeq(buf, 0, seqLength(), reverse);
}


void HE_group::printWithMark(stringBuilder &sb, int indent, int n) const
{
  sb.indent(indent) << "group {\n";
  int i;
  for (i=0; i < seqLength(); i++) {
    bool allocated;
    HistoryElt *e = decode(seq.get(i), allocated);

    if (i==n) {
      // print mark
      sb << "--->\n";
    }
    e->print(sb, indent+2);

    if (allocated) {
      delete e;
    }
  }

  if (i==n) {
    // print mark
    sb << "--->\n";
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


void HE_group::stats(HistoryStats &stats) const
{
  stats.groups++;

  // for me
  stats.memUsage += sizeof(*this);
  stats.mallocObjects++;

  // for 'seq' storage
  int L, G, R;
  seq.getInternals(L, G, R);
  stats.memUsage += (L+R) * sizeof(HistoryElt*);
  if (L+G+R > 0) {
    stats.mallocObjects++;
  }
  stats.reservedSpace += G * sizeof(HistoryElt*);

  // for 'seq' contents
  for (int i=0; i < seq.length(); i++) {
    HistoryEltCode code = seq.get(i);
    if (code & 1) {
      // not a pointer, no memory usage beyond that already accounted
      // for when 'seq' was counted
      stats.records++;
    }
    else {
      // pointer
      ((HistoryElt*)code)->stats(stats);
    }
  }
}


// -------------------- HistoryStats --------------------
HistoryStats::HistoryStats()
  : records(0),
    groups(0),
    memUsage(0),
    mallocObjects(0),
    reservedSpace(0)
{}


int HistoryStats::totalUsage() const
{
  return memUsage + mallocObjects*sizeof(int) + reservedSpace;
}


void HistoryStats::printInfo() const
{
  printf("history stats:\n");
  printf("  records      : %d\n", records);
  printf("  groups       : %d\n", groups);
  printf("  memUsage     : %d\n", memUsage);
  printf("  mallocObjects: %d\n", mallocObjects);
  printf("  reservedSpace: %d\n", reservedSpace);
  printf("  totalUsage() : %d\n", totalUsage());
  fflush(stdout);
}


// EOF
