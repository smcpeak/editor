// td.cc
// code for td.h

#include "td.h"                        // this module

#include "autofile.h"                  // AutoFILE
#include "mysig.h"                     // printSegfaultAddrs


TextDocument::TextDocument()
  : core(),
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
  clear(core);
}


bool TextDocument::unsavedChanges() const
{
  return (savedHistoryIndex != historyIndex);
}


void TextDocument::readFile(char const *fname)
{
  ::readFile(core, fname);              // might throw exception

  // clear after file's been successfully read
  clearHistory();
  noUnsavedChanges();
}


void TextDocument::insertAt(TextCoord tc, char const *text, int textLen)
{
  // Ignore insertions of nothing.
  if (textLen > 0) {
    HE_text *e = new HE_text(tc,
                             true /*insertion*/,
                             text, textLen);
    e->apply(core, false /*reverse*/);
    appendElement(e);
  }
}


void TextDocument::deleteAt(TextCoord tc, int count)
{
  if (count > 0) {
    HE_text *e = new HE_text(tc,
                             false /*insertion*/,
                             NULL /*text*/, 0 /*textLen*/);
    e->computeText(core, count);
    e->apply(core, false /*reverse*/);
    appendElement(e);
  }
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


void TextDocument::beginUndoGroup()
{
  groupStack.push(new HE_group);
}

void TextDocument::endUndoGroup()
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


TextCoord TextDocument::undo()
{
  xassert(canUndo() && !inUndoGroup());

  historyIndex--;
  return history.applyOne(core, historyIndex, true /*reverse*/);
}


TextCoord TextDocument::redo()
{
  xassert(canRedo() && !inUndoGroup());

  TextCoord tc = history.applyOne(core, historyIndex, false /*reverse*/);
  historyIndex++;
  return tc;
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
  cout.flush();
}


void TextDocument::printHistoryStats() const
{
  HistoryStats stats;
  historyStats(stats);
  stats.printInfo();
}


// EOF
