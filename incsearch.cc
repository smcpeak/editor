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


void IncSearch::attach(Editor *newEd)
{
  if (ed) {
    // we're already attached, the user just pressed Ctrl-S while
    // we're searching, so search to next spot
    findString(curFlags | Buffer::FS_ADVANCE_ONCE);
    return;
  }

  AttachInputProxy::attach(newEd);

  beginLine = ed->cursorLine;
  beginCol = ed->cursorCol;

  beginFVLine = ed->firstVisibleLine;
  beginFVCol = ed->firstVisibleCol;

  curFlags = Buffer::FS_CASE_INSENSITIVE;

  text = "";

  curLine = beginLine;
  curCol = beginCol;
  
  match = true;

  if (status) {
    prevStatusText = status->text();
    status->setText(statusText());
  }
}


QString IncSearch::statusText() const
{
  stringBuilder sb;
  if (match) {
    sb << "search";
  }
  else {
    sb << "not found";
  }
  sb << ": \"" << text << "\"";

  if (curFlags & Buffer::FS_CASE_INSENSITIVE) {
    sb << " (insens)";
  }
  if (curFlags & Buffer::FS_BACKWARDS) {
    sb << " (back)";
  }

  return QString(sb);
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

  #if 0
  // some keys are ignored but shouldn't cause exit from i-search
  switch (k->key()) {
    case Qt::Key_Shift:
    case Qt::Key_Alt:
    case Qt::Key_Control:
      return false;
  }
  #endif // 0

  if (state == Qt::NoButton || state == Qt::ShiftButton) {
    switch (k->key()) {
      case Qt::Key_Escape:
        // return to original location
        ed->cursorLine = beginLine;
        ed->cursorCol = beginCol;
        ed->selectEnabled = false;

        ed->setView(beginFVLine, beginFVCol);
        ed->redraw();

        detach();
        return true;

      case Qt::Key_Enter:
      case Qt::Key_Return:
        // stop doing i-search
        detach();
        return true;

      case Qt::Key_Backspace:
        if (text.length() > 0) {               
          // remove final character
          text = string(text, text.length()-1);
          findString();     // adjust match
        }
        return true;

      case Qt::Key_Right:
        nextMatch();
        return true;

      case Qt::Key_Left:
        prevMatch();
        return true;

      default: {
        QString s = k->text();
        if (s.length() && s[0].isPrint()) {
          text &= (char const*)s;

          findString();
          return true;  // handled
        }
        break;
      }
    }
  }

  if (state == Qt::ControlButton) {
    switch (k->key()) {
      case Qt::Key_I:
        curFlags ^= Buffer::FS_CASE_INSENSITIVE;
        findString();
        return true;

      case Qt::Key_B:
        curFlags ^= Buffer::FS_BACKWARDS;
        findString();
        return true;
    }
  }

  // unknown key: don't let it through, but don't handle it either
  return true;
}


bool IncSearch::findString(Buffer::FindStringFlags flags)
{
  match = ed->buffer->findString(curLine, curCol, text, flags);
  if (match) {
    // move editor cursor to end of match
    ed->cursorLine = curLine;
    ed->cursorCol = curCol + text.length();

    // put selection start at beginning of match
    ed->selectLine = curLine;
    ed->selectCol = curCol;
    ed->selectEnabled = true;

    ed->scrollToCursor();
  }

  updateStatus();

  return match;
}


void IncSearch::updateStatus()
{
  if (status) {
    status->setText(statusText());
  }
}


bool IncSearch::nextMatch()
{
  return findString((curFlags | Buffer::FS_ADVANCE_ONCE) & ~Buffer::FS_BACKWARDS );
}


bool IncSearch::prevMatch()
{
  return findString(curFlags | Buffer::FS_ADVANCE_ONCE | Buffer::FS_BACKWARDS);
}
