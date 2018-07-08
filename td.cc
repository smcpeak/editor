// td.cc
// code for td.h

#include "td.h"                        // this module

#include "autofile.h"                  // AutoFILE
#include "mysig.h"                     // printSegfaultAddrs
#include "trace.h"                     // TRACE


int TextDocument::s_objectCount = 0;


TextDocument::TextDocument()
  : core(),
    history(),
    historyIndex(0),
    savedHistoryIndex(0),
    groupStack()
{
  s_objectCount++;
  TRACE("TextDocument",
    "created TD at " << (void*)this <<
    ", oc=" << s_objectCount);
}


TextDocument::~TextDocument()
{
  s_objectCount--;
  TRACE("TextDocument",
    "destroyed TD at " << (void*)this <<
    ", oc=" << s_objectCount);
}


void TextDocument::clearHistory()
{
  historyIndex = 0;
  savedHistoryIndex = -1;     // no historyIndex is known to correspond to on-disk
  history.truncate(historyIndex);
  groupStack.clear();

  this->core.notifyUnsavedChangesChange(this);
}


void TextDocument::clearContentsAndHistory()
{
  clearHistory();
  core.clear();
}


bool TextDocument::unsavedChanges() const
{
  if (savedHistoryIndex == historyIndex) {
    // It seems there are no unsaved changes, but we also need to check
    // the group stack.  If the group stack is non-empty, then there
    // are changes that haven't been combined and added to the normal
    // history yet.
    if (!this->groupStack.isEmpty()) {
      // Really I should inspect every element of the stack, but my
      // ObjStack class does not allow that.  Anyway, I happen to know
      // that we never push more than one element onto it.
      HE_group const *g = this->groupStack.topC();
      if (g->seqLength() > 0) {
        return true;
      }
      else {
        // The editor widget creates an undo group for every keystroke,
        // even cursor movement.  But those do not add anything to the
        // group, and hence there are no unsaved changes.
      }
    }
    return false;
  }
  else {
    return true;
  }
}


void TextDocument::noUnsavedChanges()
{
  this->savedHistoryIndex = this->historyIndex;

  // This method is called rarely, there is no problem with sending the
  // notification unconditionally.
  this->core.notifyUnsavedChangesChange(this);
}


void TextDocument::readFile(string const &fname)
{
  // This might throw an exception, but if so, 'core' will be
  // left unmodified.
  this->core.readFile(fname.c_str());

  // Clear history after file has been successfully read.
  this->clearHistory();
  this->noUnsavedChanges();
}


void TextDocument::writeFile(string const &fname) const
{
  this->core.writeFile(fname.c_str());
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


void TextDocument::bumpHistoryIndex(int inc)
{
  bool equalBefore = (this->historyIndex == this->savedHistoryIndex);

  this->historyIndex += inc;

  bool equalAfter = (this->historyIndex == this->savedHistoryIndex);

  // This is called fairly frequently, so we try to only send the
  // notification when it might matter.
  if (equalBefore != equalAfter) {
    this->core.notifyUnsavedChangesChange(this);
  }
}


void TextDocument::appendElement(HistoryElt *e)
{
  if (groupStack.isEmpty()) {
    // for now, adding a new element means truncating the history
    history.truncate(historyIndex);
    history.append(e);
    this->bumpHistoryIndex(+1);
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

  this->bumpHistoryIndex(-1);
  return history.applyOne(core, historyIndex, true /*reverse*/);
}


TextCoord TextDocument::redo()
{
  xassert(canRedo() && !inUndoGroup());

  TextCoord tc = history.applyOne(core, historyIndex, false /*reverse*/);
  this->bumpHistoryIndex(+1);
  return tc;
}


void TextDocument::addObserver(TextDocumentObserver *observer)
{
  this->core.addObserver(observer);
}

void TextDocument::removeObserver(TextDocumentObserver *observer)
{
  this->core.removeObserver(observer);
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
