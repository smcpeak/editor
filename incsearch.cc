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
    bool prevMatch = match;
    if (!findString(curFlags | Buffer::FS_ADVANCE_ONCE) &&
        !prevMatch) {
      // can't find now, and weren't on a match before, so wrap
      int line, col;
      if (tryWrapSearch(line, col)) {
        curLine = line;
        curCol = col;
        findString();    // update visuals to reflect new match
      }
    }
    return;
  }

  AttachInputProxy::attach(newEd);

  if (status) {
    prevStatusText = status->text();
  }

  beginLine = ed->cursorLine;
  beginCol = ed->cursorCol;

  beginFVLine = ed->firstVisibleLine;
  beginFVCol = ed->firstVisibleCol;

  curFlags = Buffer::FS_CASE_INSENSITIVE;
  
  mode = M_SEARCH;

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
  
  switch (mode) {
    case M_SEARCH:
      sb << "I-search:  F1=help";
      addStatusFlag(sb, "^I=insens", (curFlags & Buffer::FS_CASE_INSENSITIVE));
      addStatusFlag(sb, "^B=back", (curFlags & Buffer::FS_BACKWARDS));
      break;

    case M_GET_REPLACEMENT:
      sb << "Type replacement text";
      break;

    case M_REPLACE:
      sb << "Replace?  y/n  q=quit  !=all";
      break;
  }


  return QString(sb);
}


void IncSearch::detach()
{
  ed->hideInfo();

  if (mode == M_GET_REPLACEMENT) {
    putBackMatchText();
  }

  // leave the hitText alone, user can press Esc to eliminate it

  if (status) {
    status->setText(prevStatusText);
  }

  AttachInputProxy::detach();
}


bool IncSearch::keyPressEvent(QKeyEvent *k)
{
  int state = k->state() & Qt::KeyButtonMask;

  switch (mode) {
    default: xfailure("bad mode");
    case M_SEARCH:          return searchKeyMap(k, state);
    case M_GET_REPLACEMENT: return getReplacementKeyMap(k, state);
    case M_REPLACE:         return replaceKeyMap(k, state);
  }
}


bool IncSearch::searchKeyMap(QKeyEvent *k, int state)
{
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
        ed->hitText = "";

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

      case Qt::Key_R:
        // transition into M_GET_REPLACEMENT
        if (text.length() == 0) {
          return true;    // nop
        }

        if (!findString()) {
          return true;    // nop: no matches
        }                                   

        // remember what is selected
        removedText = ed->getSelectedText();

        // remove the selected occurrence of the match string
        ed->cursorCol -= text.length();
        ed->deleteAtCursor(text.length());

        // change mode
        setMode(M_GET_REPLACEMENT);
        replaceText = "";
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

  ed->hitText = text;

  // the only flag I want the editor using for hit text, for now,
  // is the case sensitivity flag
  ed->hitTextFlags = curFlags & Buffer::FS_CASE_INSENSITIVE;

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
    int line, col;
    if (tryWrapSearch(line, col)) {
      message &= " (can wrap)";
    }

    ed->showInfo(message);
  }
  else {
    ed->hideInfo();
  }
}


bool IncSearch::tryWrapSearch(int &line, int &col) const
{
  // wrap
  line=0;
  col=0;
  if (curFlags & Buffer::FS_BACKWARDS) {
    ed->buffer->getLastPos(line, col);
  }

  // search
  if (ed->buffer->findString(line, col, text, curFlags) &&
      !(line==curLine && col==curCol)) {
    // yes, wrapping finds another
    return true;
  }
  else {
    return false;
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


void IncSearch::setMode(Mode m)
{
  mode = m;
  updateStatus();
}


// ------------------ M_GET_REPLACEMENT ------------------
// undo the effect of transitioning from M_SEARCH
void IncSearch::putBackMatchText()
{
  // put the matched text back
  ed->editDelete();       // delete replacement text
  ed->insertAtCursor(removedText);
  findString();
}


bool IncSearch::getReplacementKeyMap(QKeyEvent *k, int state)
{
  if (state == Qt::NoButton || state == Qt::ShiftButton) {
    switch (k->key()) {
      case Qt::Key_Escape:
        // go back to M_SEARCH
        putBackMatchText();
        setMode(M_SEARCH);
        return true;

      case Qt::Key_Return:
      case Qt::Key_Enter:
        // move forward to M_REPLACE

        // we just did one implicit replacement, the one at the
        // initial match; skip over it so we don't attempt to
        // replace text in the replacement
        curCol += replaceText.length();

        // find the next match
        setMode(M_REPLACE);
        if (!findString()) {
          // last replacement, we're done
          detach();
        }
        return true;

      case Qt::Key_Backspace:
        if (replaceText.length() > 0) {
          ed->cursorCol--;
          ed->deleteAtCursor(1);

          replaceText = string(replaceText, replaceText.length()-1);
        }
        return true;

      default: {
        QString s = k->text();
        if (s.length() && s[0].isPrint()) {
          char const *p = s;

          ed->insertAtCursor(p);

          replaceText &= p;

          return true;  // handled
        }
        break;
      }
    }
  }

  return true;    // handled..
}


// --------------------- M_REPLACE ----------------------
bool IncSearch::replace()
{
  // remove match text
  ed->cursorCol -= text.length();
  ed->deleteAtCursor(text.length());

  // insert replacement text
  ed->insertAtCursor(replaceText);

  // find next match
  curCol += replaceText.length();
  if (!findString()) {
    // done
    ed->redraw();
    detach();
    return false;
  }
  else {
    return true;
  }
}


bool IncSearch::replaceKeyMap(QKeyEvent *k, int state)
{
  if (state == Qt::NoButton || state == Qt::ShiftButton) {
    switch (k->key()) {
      case Qt::Key_Return:
      case Qt::Key_Enter:
      case Qt::Key_Y:
        replace();
        return true;

      case Qt::Key_N:
        // find next match
        if (!findString(curFlags | Buffer::FS_ADVANCE_ONCE)) {
          detach();
        }
        return true;

      case Qt::Key_Q:
        detach();
        return true;

      case Qt::Key_Exclam:
        while (replace())
          {}
        return true;

      case Qt::Key_Left:
        prevMatch();
        return true;

      case Qt::Key_Right:
        nextMatch();
        return true;
    }
  }
  
  return true;
}


// EOF
