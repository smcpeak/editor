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

  if (ed->selectEnabled) {
    // initialize the search string with the selection
    ed->normalizeSelect();

    curLine = ed->selLowLine;
    curCol = ed->selLowCol;
    if (ed->selLowLine == ed->selHighLine) {
      // expected case
      text = ed->buffer->getTextRange(ed->selLowLine, ed->selLowCol, 
        ed->selHighLine, ed->selHighCol);
    }
    else {
      // truncate to one line..
      text = ed->buffer->getTextRange(ed->selLowLine, ed->selLowCol,
        ed->selLowLine, ed->buffer->lineLength(ed->selLowLine));
    }
  }

  else {
    // empty initial search string (user can add at cursor with ctrl-W)
    text = "";

    curLine = beginLine;
    curCol = beginCol;
  }

  // in either case this should succeed...
  findString();
}


static void addStatusFlag(stringBuilder &sb, char const *label, bool on)
{
  sb << "  " << label << "(" << (on? "x" : " ") << ")";
}

QString IncSearch::statusText() const
{
  stringBuilder sb;
  sb << "I-search:  F1=help";

  addStatusFlag(sb, "^I=insens", (curFlags & Buffer::FS_CASE_INSENSITIVE));
  addStatusFlag(sb, "^B=back", (curFlags & Buffer::FS_BACKWARDS));

  return QString(sb);
}


void IncSearch::detach()
{
  ed->hideInfo();
  if (status) {
    status->setText(prevStatusText);
  }

  AttachInputProxy::detach();
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

      case Qt::Key_Home:     
        // make as much of the left side visible as possible
        ed->setFirstVisibleCol(0);
        ed->scrollToCursor();                               
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

      case Qt::Key_W:
        // grab chars from cursor up to end of next word
        // or end of line
        text &= ed->buffer->getWordAfter(ed->cursorLine, ed->cursorCol);
        findString();
        return true;

      #if 0   // they don't work that well b/c of bad interaction with selection..
      case Qt::Key_Left:
      case Qt::Key_Right:
      case Qt::Key_Up:
      case Qt::Key_Down:
      case Qt::Key_W:       // and this is a conflicting binding...
      case Qt::Key_Z:
        // pass these through to editor, so it can scroll
        return false;
      #endif // 0

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

    ed->scrollToCursor(3);
  }

  updateStatus();

  return match;
}


void IncSearch::updateStatus()
{
  if (status) {
    status->setText(statusText());
  }
  
  if (!match) {
    string message = stringc << "not found: \"" << text << "\"";
    
    // suppose I did a wrap around, would I then find a match, other
    // than the one I'm (possibly) on now?
    int line=0, col=0;
    if (curFlags & Buffer::FS_BACKWARDS) {
      ed->buffer->getLastPos(line, col);
    }

    if (ed->buffer->findString(line, col, text, curFlags) &&
        line!=curLine && col!=curCol) {
      // yes, wrapping finds another
      message &= " (can wrap)";
    }

    ed->showInfo(message);
  }
  else {
    ed->hideInfo();
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
