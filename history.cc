// history.cc
// code for history.h

#include "history.h"                   // this module

#include "byte-count.h"                // ByteCount, memcpyBC
#include "byte-difference.h"           // ByteDifference

#include "smbase/array.h"              // Array
#include "smbase/strutil.h"            // encodeWithEscapes

#include <stdio.h>                     // printf


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


// -------------------- HistoryElt ----------------------
HistoryElt::~HistoryElt()
{}


// ------------------------ HE_text ----------------------
HE_text::HE_text(TextMCoord tc_, bool i,
                 char const *t, ByteCount len)
  : tc(tc_),
    insertion(i),
    text(len.get())
{
  if (len > 0) {
    memcpyBC(text.getArrayNC(), t, len);
    text.setLength(len.get());
  }
}

HE_text::~HE_text()
{}


TextMCoord HE_text::apply(TextDocumentCore &buf, bool reverse) const
{
  return static_apply(buf, tc, insertion, text, reverse);
}

STATICDEF TextMCoord HE_text::static_apply(
  TextDocumentCore &buf, TextMCoord tc, bool insertion,
  ArrayStack<char> const &text, bool reverse)
{
  if (insertion) {
    HE_text::insert(buf, tc, text, reverse);
  }
  else {
    HE_text::insert(buf, tc, text, !reverse);
  }

  return tc;
}


static void deletionMismatch()
{
  THROW(XHistory("deletion text does not match buffer contents"));
}

// Insert (=forward) or delete (=reverse) some text at 'tc'.
STATICDEF void HE_text::insert(
  TextDocumentCore &buf, TextMCoord tc, ArrayStack<char> const &text,
  bool reverse)
{
  if (!buf.validCoord(tc)) {
    THROW(XHistory("coordinate is not within text area"));
  }

  if (!reverse) {
    // insertion
    // ==> committed

    // Left edge of the inserted text.
    TextMCoord begin = tc;

    // excess text on the original line that gets floated down
    // to after the cursor on the last line
    ArrayStack<char> excess(0);

    char const *p = text.getArray();
    char const *end = p + text.length();
    while (p < end) {
      // find next newline, or 'end'
      char const *nl = p;
      while (nl < end && *nl!='\n') {
        nl++;
      }

      // length of this segment
      ByteCount len(nl - p);

      // insert this text at line/col
      buf.insertText(tc, p, len);
      tc.m_byteIndex += len;

      // insert newline
      if (nl < end) {
        // if there is text beyond 'col' on 'line-1', then that text
        // gets floated down to the end of the insertion
        if (tc.m_line==begin.m_line &&     // optimization: can only happen on first line
            tc.m_byteIndex < buf.lineLengthByteIndex(tc.m_line)) {
          // this can only happen on the first line of the insertion
          // procedure, so check that we don't already have excess
          xassert(excess.length() == 0);

          // get the excess
          ByteCount excessLength(
            buf.lineLengthByteIndex(tc.m_line) - tc.m_byteIndex);
          buf.getPartialLine(tc, excess, excessLength);
          xassert(excess.length() == excessLength);

          // remove it from the buffer
          buf.deleteTextBytes(tc, excessLength);
        }

        ++tc.m_line;
        buf.insertLine(tc.m_line);
        tc.m_byteIndex.set(0);
        len++;   // so we skip '\n' too
      }

      // skip consumed text
      p += len;
    }
    xassert(p == end);

    // insert the floated excess text, if any
    if (excess.allocatedSize() > 0) {
      buf.insertText(tc, excess.getArray(), ByteCount(excess.length()));
    }
  }

  else {
    // deletion

    // location of the left edge of the text to delete
    TextMCoord begin = tc;

    // splice to perform at end?
    int pendingSplice = 0;

    // check correspondence between the text in the event record and
    // what's in the buffer, without modifying the buffer yet
    ArrayStack<char> actualText(text.length());
    if (!buf.getTextSpanningLines(tc, actualText,
                                  ByteCount(text.length()))) {
      deletionMismatch();      // span isn't valid
    }
    if (0!=memcmp(text.getArray(), actualText.getArray(), text.length())) {
      deletionMismatch();      // text doesn't match
    }

    // ==> committed

    // contents are known to match, so delete the text
    tc = begin;
    char const *p = text.getArray();
    char const *end = p + text.length();
    while (p < end) {
      // find next newline, or 'end'
      char const *nl = p;
      while (nl < end && *nl!='\n') {
        nl++;
      }

      // length of this segment
      ByteCount len(nl - p);

      // delete the segment
      buf.deleteTextBytes(tc, len);

      // bypass newline
      if (nl < end) {
        // we deleted all text on this line after 'col'
        xassert(buf.lineLengthBytes(tc.m_line) == tc.m_byteIndex);

        if (tc.m_byteIndex == 0) {
          // we're at the beginning of a line, and it is now empty,
          // so just delete this line
          buf.deleteLine(tc.m_line);
        }
        else {
          // move line/col to beginning of next line, so that from
          // now on we can work with whole deleted lines, but remember
          // that there's a pending line splice
          ++tc.m_line;
          tc.m_byteIndex.set(0);
          pendingSplice++;
        }
        len++;   // so we skip '\n' too
      }

      // skip consumed text
      p += len;
    }

    if (pendingSplice) {
      xassert(pendingSplice == 1);    // should only have one splice
      xassert(tc.m_byteIndex == 0);   // it's this entire line that goes

      // grab this line's contents
      ByteCount spliceLen = buf.lineLengthBytes(tc.m_line);
      ArrayStack<char> splice(spliceLen.get());
      buf.getPartialLine(tc, splice, spliceLen);

      // blow it away
      buf.deleteTextBytes(tc, spliceLen);
      buf.deleteLine(tc.m_line);

      // move up to end of previous line
      --tc.m_line;
      tc.m_byteIndex = buf.lineLengthByteIndex(tc.m_line);

      // append splice text
      buf.insertText(tc, splice.getArray(), spliceLen);
    }
  }
}


void HE_text::computeText(TextDocumentCore const &buf, ByteCount count)
{
  xassert(insertion == false);
  xassert(text.isEmpty());
  xassert(buf.validCoord(tc));

  text.setAllocatedSize(count.get());
  if (!buf.getTextSpanningLines(tc, text, count)) {
    xfailure("deletion span is not entirely within defined text area");
  }
}


void HE_text::print(std::ostream &sb, int indent) const
{
  sb << std::string(indent, ' ')
     << (insertion? "Ins" : "Del")
     << "(" << tc << ", \""
     << encodeWithEscapes(text.getArray(), text.length())
     << "\");\n";
}


void HE_text::stats(HistoryStats &stats) const
{
  stats.records++;
  stats.memUsage += sizeof(*this);
  stats.mallocObjects++;
  if (text.allocatedSize()) {
    stats.memUsage += text.allocatedSize();
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
  seq.push(e);
}


HistoryElt *HE_group::popLastElement()
{
  return seq.pop();
}


void HE_group::squeezeReserved()
{
  seq.consolidate();
}


void HE_group::truncate(int newLength)
{
  xassert(0 <= newLength && newLength <= seqLength());

  while (newLength < seqLength()) {
    delete seq.pop();
  }
}


TextMCoord HE_group::applySeqElt(TextDocumentCore &buf, int start, int end, int offset,
                                 bool reverseIndex, bool reverseOperation) const
{
  if (reverseIndex) {
    offset = (end-start) - offset - 1;
  }
  return applyOne(buf, start+offset, reverseOperation);
}

TextMCoord HE_group::applySeq(TextDocumentCore &buf, int start, int end, bool reverse) const
{
  int i = 0;
  try {
    TextMCoord leftEdge = buf.endCoord();
    for ( ; i < (end-start); i++) {
      TextMCoord tc = applySeqElt(buf, start, end, i, reverse, reverse);
      if (tc < leftEdge) {
        leftEdge = tc;
      }
    }
    return leftEdge;
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


TextMCoord HE_group::applyOne(TextDocumentCore &buf, int index, bool reverse) const
{
  HistoryElt const *e = seq[index];
  return e->apply(buf, reverse);
}


TextMCoord HE_group::apply(TextDocumentCore &buf, bool reverse) const
{
  return applySeq(buf, 0, seqLength(), reverse);
}


void HE_group::printWithMark(std::ostream &sb, int indent, int n) const
{
  sb << std::string(indent, ' ') << "group {\n";
  int i;
  for (i=0; i < seqLength(); i++) {
    HistoryElt const *e = seq[i];

    if (i==n) {
      // print mark
      sb << "--->\n";
    }
    e->print(sb, indent+2);
  }

  if (i==n) {
    // print mark
    sb << "--->\n";
  }

  sb << std::string(indent, ' ') << "}";

  //if (n >= 0) {
  //  sb << " /""*time=" << n << "*/";
  //}

  sb << "\n";
}


void HE_group::print(std::ostream &sb, int indent) const
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
  stats.memUsage += seq.allocatedSize() * sizeof(HistoryElt*);
  if (seq.allocatedSize() > 0) {
    stats.mallocObjects++;
  }
  stats.reservedSpace +=
    (seq.allocatedSize() - seq.length()) * sizeof(HistoryElt*);

  // for 'seq' contents
  for (int i=0; i < seq.length(); i++) {
    seq[i]->stats(stats);
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
