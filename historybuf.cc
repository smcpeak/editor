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


void HistoryBuffer::readFile(char const *fname)
{
  readFile(buf, fname);              // might throw exception         

  // clear after file's been successfully read
  clearHistory();
}


void HistoryBuffer::moveCursor(bool relLine, int line, bool relCol, int col)
{
  int origLine = relLine? -1 : buf.line;
  int origCol  = relCol?  -1 : buf.col;

  HistoryElt *e = new HE_cursor(origLine, line, origCol, col);
  e->apply(buf, false /*reverse*/);
  history.append(e);
}
                      

void HistoryBuffer::insertLR(bool left, char const *text, int textLen)
{ 
  HE_text *e = new HE_text(true /*insertion*/, left, 0 /*fill*/,
                           text, textLen);
  e->computeFill(buf);
  e->apply(buf, false /*reverse*/);
  history.append(e);
}


void HistoryBuffer::deleteLR(bool left, int count)
{
  HE_text *e = new HE_text(false /*insertion*/, left, 0 /*fill*/,
                           NULL /*text*/, 0 /*textLen*/);
  e->computeFill(buf);
  e->computeText(buf, count);
  e->apply(buf, false /*reverse*/);
  history.append(e);
}



void HistoryBuffer::undo()
{
  xassert(canUndo());
  
  time--;
  history.applyOne(buf, time, true /*reverse*/);
}


void HistoryBuffer::redo()
{
  xassert(canRedo());
  
  history.applyOne(buf, time, false /*reverse*/);
  time++;
}


// EOF
