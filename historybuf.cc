// historybuf.cc
// code for historybuf.h

#include "historybuf.h"       // this module
#include "autofile.h"         // AutoFILE
#include "mysig.h"            // printSegfaultAddrs


HistoryBuffer::HistoryBuffer()
  : buf(),
    history(),
    historyIndex(0),
    savedHistoryIndex(0),
    groupStack(),
    groupEltModifies()
{}


HistoryBuffer::~HistoryBuffer()
{}



void HistoryBuffer::clearHistory()
{
  historyIndex = 0;
  savedHistoryIndex = -1;     // no historyIndex is known to correspond to on-disk
  history.truncate(historyIndex);
  groupStack.clear();
}


void HistoryBuffer::clearContentsAndHistory()
{
  clearHistory();
  clear(buf);
}


bool HistoryBuffer::unsavedChanges() const
{
  if (savedHistoryIndex != historyIndex) {
    return true;
  }

  // if there are unclosed groups, check with them; semantically they
  // come after 'historyIndex' but before 'historyIndex+1', and we
  // already know that 'savedHistoryIndex' equals 'historyIndex'
  for (int i=0; i < groupEltModifies.length(); i++) {
    if (groupEltModifies[i]) {
      return true;
    }
  }

  return false;
}


void HistoryBuffer::readFile(char const *fname)
{
  ::readFile(buf, fname);              // might throw exception

  // clear after file's been successfully read
  clearHistory();
  noUnsavedChanges();
}


void HistoryBuffer::moveCursor(bool relLine, int line, bool relCol, int col)
{
  int origLine = relLine? -1 : buf.line;
  int origCol  = relCol?  -1 : buf.col;

  HistoryElt *e = new HE_cursor(origLine, line, origCol, col);
  bool mod = e->apply(buf, false /*reverse*/);
  appendElement(e, mod);
}


void HistoryBuffer::insertLR(bool left, char const *text, int textLen)
{
  xassert(buf.cursorInDefined());

  HE_text *e = new HE_text(true /*insertion*/, left,
                           text, textLen);
  bool mod = e->apply(buf, false /*reverse*/);
  appendElement(e, mod);
}


void HistoryBuffer::deleteLR(bool left, int count)
{
  xassert(buf.cursorInDefined());

  HE_text *e = new HE_text(false /*insertion*/, left,
                           NULL /*text*/, 0 /*textLen*/);
  e->computeText(buf, count);
  bool mod = e->apply(buf, false /*reverse*/);
  appendElement(e, mod);
}


void HistoryBuffer::appendElement(HistoryElt *e, bool modified)
{
  if (groupStack.isEmpty()) {
    // for now, adding a new element means truncating the history
    history.truncate(historyIndex);
    history.append(e);

    if (savedHistoryIndex==historyIndex && !modified) {
      savedHistoryIndex++;
    }
    historyIndex++;
  }
  else {
    groupStack.top()->append(e);
    groupEltModifies.top() = groupEltModifies.top() || modified;
  }
}


void HistoryBuffer::beginGroup()
{
  groupStack.push(new HE_group);
  groupEltModifies.push(false);
}

void HistoryBuffer::endGroup()
{
  HE_group *g = groupStack.pop();
  bool modifies = groupEltModifies.pop();
  if (g->seqLength() > 0) {
    appendElement(g, modifies);
  }
  else {
    xassert(!modifies);
    delete g;    // empty sequence, just drop it
  }
}


void HistoryBuffer::undo()
{
  xassert(canUndo() && !inGroup());

  historyIndex--;
  if (!history.applyOne(buf, historyIndex, true /*reverse*/) &&
      savedHistoryIndex-1 == historyIndex) {
    savedHistoryIndex--;
  }
}


void HistoryBuffer::redo()
{
  xassert(canRedo() && !inGroup());

  if (!history.applyOne(buf, historyIndex, false /*reverse*/) &&
      savedHistoryIndex == historyIndex) {
    savedHistoryIndex++;
  }
  historyIndex++;
}


void HistoryBuffer::printHistory(stringBuilder &sb) const
{
  history.printWithMark(sb, 0 /*indent*/, historyIndex);
}

void HistoryBuffer::printHistory() const
{
  stringBuilder sb;
  printHistory(sb);
  cout << sb;
}


void HistoryBuffer::printHistoryStats() const
{
  HistoryStats stats;
  historyStats(stats);
  stats.printInfo();
}


// -------------------- test code -------------------
#ifdef TEST_HISTORYBUF

#include "datablok.h"   // DataBlock
#include "ckheap.h"     // malloc_stats

#include <unistd.h>     // unlink
#include <stdio.h>      // printf


void expect(HistoryBuffer const &buf, int line, int col, char const *text)
{
  if (buf.line()==line &&
      buf.col()==col) {
    // ok
  }
  else {
    printf("expect %d:%d\n", line, col);
    printf("actual %d:%d\n", buf.line(), buf.col());
    xfailure("cursor location mismatch");
  }

  writeFile(buf.core(), "historybuf.tmp");
  DataBlock block;
  block.readFromFile("historybuf.tmp");

  // compare contents to what is expected
  if (0!=memcmp(text, block.getDataC(), block.getDataLen()) ||
      (int)strlen(text)!=(int)block.getDataLen()) {
    xfailure("text mismatch");
  }

}


void printHistory(HistoryBuffer const &buf)
{
  stringBuilder sb;
  buf.printHistory(sb);
  cout << sb;
}


void chars(HistoryBuffer &buf, char const *str)
{
  while (*str) {
    buf.insertLR(false /*left*/, str, 1);
    str++;
  }
}


void entry()
{
  // This isn't implemented in smbase for mingw.
  //printSegfaultAddrs();

  HistoryBuffer buf;

  chars(buf, "abcd");
  //printHistory(buf);
  expect(buf, 0,4, "abcd");

  buf.undo();
  //printHistory(buf);
  expect(buf, 0,3, "abc");

  chars(buf, "e");
  //printHistory(buf);
  expect(buf, 0,4, "abce");

  chars(buf, "\nThis is the second line.\n");
  expect(buf, 2,0, "abce\n"
                   "This is the second line.\n"
                   "");

  buf.moveCursor(true /*relLine*/, -1, true /*relCol*/, 2);
  chars(buf, "z");
  expect(buf, 1,3, "abce\n"
                   "Thzis is the second line.\n"
                   "");

  buf.undo();
  buf.undo();
  chars(buf, "now on third");
  expect(buf, 2,12, "abce\n"
                    "This is the second line.\n"
                    "now on third");

  buf.undo();
  buf.undo();
  buf.undo();
  expect(buf, 2,9,  "abce\n"
                    "This is the second line.\n"
                    "now on th");

  buf.redo();
  expect(buf, 2,10, "abce\n"
                    "This is the second line.\n"
                    "now on thi");

  buf.redo();
  expect(buf, 2,11, "abce\n"
                    "This is the second line.\n"
                    "now on thir");

  buf.deleteLR(true /*left*/, 6);
  expect(buf, 2,5,  "abce\n"
                    "This is the second line.\n"
                    "now o");

  chars(buf, "z");
  expect(buf, 2,6,  "abce\n"
                    "This is the second line.\n"
                    "now oz");

  buf.undo();
  buf.undo();
  expect(buf, 2,11, "abce\n"
                    "This is the second line.\n"
                    "now on thir");
  //printHistory(buf);

  buf.printHistoryStats();


  unlink("historybuf.tmp");

  printf("historybuf is ok\n");
}


int main()
{
  try {
    entry();
    malloc_stats();
    return 0;
  }
  catch (xBase &x) {
    cout << x << endl;
    return 4;
  }
}


#endif // TEST_HISTORYBUF
