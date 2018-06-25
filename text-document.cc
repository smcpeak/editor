// text-document.cc
// code for text-document.h

#include "text-document.h"             // this module

#include "autofile.h"                  // AutoFILE
#include "mysig.h"                     // printSegfaultAddrs


TextDocument::TextDocument()
  : buf(),
    history(),
    historyIndex(0),
    savedHistoryIndex(0),
    groupStack()
{}


TextDocument::~TextDocument()
{}



void TextDocument::clearHistory()
{
  historyIndex = 0;
  savedHistoryIndex = -1;     // no historyIndex is known to correspond to on-disk
  history.truncate(historyIndex);
  groupStack.clear();
}


void TextDocument::clearContentsAndHistory()
{
  clearHistory();
  clear(buf);
}


bool TextDocument::unsavedChanges() const
{
  return (savedHistoryIndex != historyIndex);
}


void TextDocument::readFile(char const *fname)
{
  ::readFile(buf, fname);              // might throw exception

  // clear after file's been successfully read
  clearHistory();
  noUnsavedChanges();
}


void TextDocument::insertAt(TextCoord tc, char const *text, int textLen)
{
  // Ignore insertions of nothing.
  if (textLen > 0) {
    HE_text *e = new HE_text(buf.cursor(),
                             true /*insertion*/,
                             text, textLen);
    e->apply(buf, false /*reverse*/);
    appendElement(e);
  }
}


void TextDocument::deleteAt(TextCoord tc, int count)
{
  if (count > 0) {
    HE_text *e = new HE_text(buf.cursor(),
                             false /*insertion*/,
                             NULL /*text*/, 0 /*textLen*/);
    e->computeText(buf, count);
    e->apply(buf, false /*reverse*/);
    appendElement(e);
  }
}


void TextDocument::moveCursor(bool relLine, int line, bool relCol, int col)
{
  if (relLine) {
    buf.line += line;
  }
  else {
    buf.line = line;
  }
  xassert(buf.line >= 0);

  if (relCol) {
    buf.col += col;
  }
  else {
    buf.col = col;
  }
  xassert(buf.col >= 0);
}


void TextDocument::insertLR(bool left, char const *text, int textLen)
{
  xassert(buf.validCursor());

  this->insertAt(buf.cursor(), text, textLen);

  if (!left) {
    // Put the cursor at the end of the inserted text.
    TextCoord tc = buf.cursor();
    bool ok = walkCursor(buf, tc, textLen);
    xassert(ok);
    buf.setCursor(tc);
  }
}


void TextDocument::deleteLR(bool left, int count)
{
  xassert(buf.validCursor());

  if (left) {
    // Move the cursor to the start of the text to delete.
    TextCoord tc = buf.cursor();
    bool ok = walkCursor(buf, tc, -count);
    xassert(ok);
    buf.setCursor(tc);
  }

  this->deleteAt(buf.cursor(), count);
}


void TextDocument::appendElement(HistoryElt *e)
{
  if (groupStack.isEmpty()) {
    // for now, adding a new element means truncating the history
    history.truncate(historyIndex);
    history.append(e);
    historyIndex++;
  }
  else {
    groupStack.top()->append(e);
  }
}


void TextDocument::beginGroup()
{
  groupStack.push(new HE_group);
}

void TextDocument::endGroup()
{
  HE_group *g = groupStack.pop();
  if (g->seqLength() >= 2) {
    appendElement(g);
  }
  else if (g->seqLength() == 1) {
    // Throw away the useless group container.
    appendElement(g->popLastElement());
    delete g;
  }
  else {
    delete g;    // empty sequence, just drop it
  }
}


void TextDocument::undo()
{
  xassert(canUndo() && !inGroup());

  historyIndex--;
  TextCoord tc = history.applyOne(buf, historyIndex, true /*reverse*/);
  buf.setCursor(tc);
}


void TextDocument::redo()
{
  xassert(canRedo() && !inGroup());

  TextCoord tc = history.applyOne(buf, historyIndex, false /*reverse*/);
  historyIndex++;
  buf.setCursor(tc);
}


void TextDocument::printHistory(stringBuilder &sb) const
{
  history.printWithMark(sb, 0 /*indent*/, historyIndex);
}

void TextDocument::printHistory() const
{
  stringBuilder sb;
  printHistory(sb);
  cout << sb;
}


void TextDocument::printHistoryStats() const
{
  HistoryStats stats;
  historyStats(stats);
  stats.printInfo();
}


// -------------------- test code -------------------
#ifdef TEST_TEXT_DOCUMENT

#include "datablok.h"   // DataBlock
#include "ckheap.h"     // malloc_stats

#include <unistd.h>     // unlink
#include <stdio.h>      // printf


void expect(TextDocument const &buf, int line, int col, char const *text)
{
  if (buf.line()==line &&
      buf.col()==col) {
    // ok
  }
  else {
    printf("expect %d:%d\n", line, col);
    printf("actual %d:%d\n", buf.line(), buf.col());
    fflush(stdout);
    xfailure("cursor location mismatch");
  }

  writeFile(buf.core(), "text-document.tmp");
  DataBlock block;
  block.readFromFile("text-document.tmp");

  // compare contents to what is expected
  if (0!=memcmp(text, block.getDataC(), block.getDataLen()) ||
      (int)strlen(text)!=(int)block.getDataLen()) {
    xfailure("text mismatch");
  }

}


void printHistory(TextDocument const &buf)
{
  stringBuilder sb;
  buf.printHistory(sb);
  cout << sb;
  cout.flush();
}


void chars(TextDocument &buf, char const *str)
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

  TextDocument buf;

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
  buf.moveCursor(true /*relLine*/, +1, true /*relCol*/, -2);
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
  buf.moveCursor(true /*relLine*/, +0, true /*relCol*/, +1);
  expect(buf, 2,10, "abce\n"
                    "This is the second line.\n"
                    "now on thi");

  buf.redo();
  buf.moveCursor(true /*relLine*/, +0, true /*relCol*/, +1);
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
  buf.moveCursor(true /*relLine*/, +0, true /*relCol*/, +6);
  expect(buf, 2,11, "abce\n"
                    "This is the second line.\n"
                    "now on thir");
  //printHistory(buf);

  buf.beginGroup();
  chars(buf, "abc");
  buf.endGroup();
  expect(buf, 2,14, "abce\n"
                    "This is the second line.\n"
                    "now on thirabc");

  buf.undo();
  expect(buf, 2,11, "abce\n"
                    "This is the second line.\n"
                    "now on thir");

  buf.beginGroup();
  chars(buf, "y");
  buf.endGroup();
  expect(buf, 2,12, "abce\n"
                    "This is the second line.\n"
                    "now on thiry");

  buf.undo();
  expect(buf, 2,11, "abce\n"
                    "This is the second line.\n"
                    "now on thir");

  buf.printHistoryStats();


  unlink("text-document.tmp");

  printf("text-document is ok\n");
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


#endif // TEST_TEXT_DOCUMENT
