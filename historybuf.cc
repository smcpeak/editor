// historybuf.cc
// code for historybuf.h

#include "historybuf.h"       // this module
#include "autofile.h"         // AutoFILE
#include "mysig.h"            // printSegfaultAddrs


HistoryBuffer::HistoryBuffer()
  : buf(),
    history(),
    time(0),
    groupStack()
{}


HistoryBuffer::~HistoryBuffer()
{}



void HistoryBuffer::clearHistory()
{
  time = 0;
  history.truncate(time);
  groupStack.clear();
}


void HistoryBuffer::clearContentsAndHistory()
{
  clearHistory();
  clear(buf);
}


void HistoryBuffer::readFile(char const *fname)
{
  ::readFile(buf, fname);              // might throw exception

  // clear after file's been successfully read
  clearHistory();
}


void HistoryBuffer::moveCursor(bool relLine, int line, bool relCol, int col)
{
  int origLine = relLine? -1 : buf.line;
  int origCol  = relCol?  -1 : buf.col;

  HistoryElt *e = new HE_cursor(origLine, line, origCol, col);
  e->apply(buf, false /*reverse*/);
  appendElement(e);
}


void HistoryBuffer::insertLR(bool left, char const *text, int textLen)
{
  xassert(buf.cursorInDefined());

  HE_text *e = new HE_text(true /*insertion*/, left,
                           text, textLen);
  e->apply(buf, false /*reverse*/);
  appendElement(e);
}


void HistoryBuffer::deleteLR(bool left, int count)
{
  xassert(buf.cursorInDefined());

  HE_text *e = new HE_text(false /*insertion*/, left,
                           NULL /*text*/, 0 /*textLen*/);
  e->computeText(buf, count);
  e->apply(buf, false /*reverse*/);
  appendElement(e);
}

                    
void HistoryBuffer::appendElement(HistoryElt *e)
{
  if (groupStack.isEmpty()) {
    // for now, adding a new element means truncating the history
    history.truncate(time);
    history.append(e);
    time++;
  }
  else {
    groupStack.top()->append(e);
  }
}


void HistoryBuffer::beginGroup()
{
  groupStack.push(new HE_group);
}

void HistoryBuffer::endGroup()
{
  HE_group *g = groupStack.pop();
  appendElement(g);
}


void HistoryBuffer::undo()
{
  xassert(canUndo() && !inGroup());

  time--;
  history.applyOne(buf, time, true /*reverse*/);
}


void HistoryBuffer::redo()
{
  xassert(canRedo() && !inGroup());
  
  history.applyOne(buf, time, false /*reverse*/);
  time++;
}


void HistoryBuffer::printHistory(stringBuilder &sb) const
{
  history.printWithMark(sb, 0 /*indent*/, time);
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
  printSegfaultAddrs();

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
              
  {
    HistoryStats stats;
    buf.historyStats(stats);
    stats.printInfo();
  }


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
