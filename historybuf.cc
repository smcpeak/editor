// historybuf.cc
// code for historybuf.h

#include "historybuf.h"       // this module
#include "autofile.h"         // AutoFILE


HistoryBuffer::HistoryBuffer()
  : buf(),
    history(),
    time(0)
{}


HistoryBuffer::~HistoryBuffer()
{}



void HistoryBuffer::clearHistory()
{
  time = 0;
  history.truncate(time);
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
    history.append(e);
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
  
  if (g->seqLength() == 0) {
    // nothing in the sequence; I expect this to be rather common,
    // since I'm planning on opening/closing groups for every UI
    // action, many of which won't actually try to add a history
    // element
    delete g;
  }

  else if (g->seqLength() == 1) {
    // only one element; I expect this to also be common, because
    // the majority of buffer modifications should end up as
    // singleton groups, again due to making a group for every UI
    // action
    HistoryElt *e = g->stealFirstElt();
    delete g;
    appendElement(e);
  }

  else {
    // more than one element, append it as a group
    appendElement(g);
  }
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


// EOF
