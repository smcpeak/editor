// incsearch.cc
// code for incsearch.h

#include "incsearch.h"     // this module
#include "editor.h"        // Editor

#include <qevent.h>        // QKeyEvent
#include <qlabel.h>        // QLabel


IncSearch::IncSearch(QLabel *s)
  : status(s)
{
  // other fields init'd in attach()
}

IncSearch::~IncSearch()
{}


void IncSearch::attach(Editor *ed)
{
  AttachInputProxy::attach(ed);

  beginLine = ed->cursorLine;
  beginCol = ed->cursorCol;

  beginFVLine = ed->firstVisibleLine;
  beginFVCol = ed->firstVisibleCol;

  text = "";

  curLine = beginLine;
  curCol = beginCol;

  if (status) {
    prevStatusText = status->text();
    status->setText("search: \"\"");
  }
}


void IncSearch::detach()
{
  AttachInputProxy::detach();
  
  if (status) {
    status->setText(prevStatusText);
  }
}


bool IncSearch::keyPressEvent(QKeyEvent *k)
{
  int state = k->state() & Qt::KeyButtonMask;

  // some keys are ignored but shouldn't cause exit from i-search
  switch (k->key()) {
    case Qt::Key_Shift:
    case Qt::Key_Alt:
    case Qt::Key_Control:
      return false;
 }

  if (state == Qt::NoButton || state == Qt::ShiftButton) {
    if (k->key() == Qt::Key_Escape) {
      // return to original location
      ed->cursorLine = beginLine;
      ed->cursorCol = beginCol;

      ed->setView(beginFVLine, beginFVCol);

      detach();
      return true;
    }

    QString s = k->text();
    if (s.length() && s[0].isPrint()) {
      text &= (char const*)s;

      if (ed->buffer->findString(curLine, curCol, text)) {
        // move editor cursor to end of match
        ed->cursorLine = curLine;
        ed->cursorCol = curCol + text.length();

        // put selection start at beginning of match
        ed->selectLine = curLine;
        ed->selectCol = curCol;
        ed->selectEnabled = true;

        ed->scrollToCursor();

        if (status) {
          status->setText(QString(stringc
            << "search: \"" << text << "\""));
        }
      }
      else {
        // leave editor cursor and selection alone

        if (status) {
          status->setText(QString(stringc
            << "not found: \"" << text << "\""));
        }
      }

      return true;  // handled
    }
  }
              
  // unknown key: stop doing i-search
  detach();

  return false;
}
