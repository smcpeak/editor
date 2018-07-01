// incsearch.cc
// code for incsearch.h

#include "incsearch.h"     // this module

#include "editor-widget.h" // EditorWidget
#include "pixmaps.h"       // Pixmaps
#include "qtguiutil.h"     // toString(QKeyEvent)
#include "status.h"        // StatusDisplay
#include "test.h"          // PVAL
#include "trace.h"         // TRACE

#include <qevent.h>        // QKeyEvent
#include <qlabel.h>        // QLabel


IncSearch::IncSearch(StatusDisplay *s)
  : status(s),

    // All of the following values are unimportant because they are
    // overwritten in attach().  The only reason I initialize here is
    // to pacify an Eclipse warning that I do not want to disable.
    prevStatusText(),
    beginLine(0),
    beginCol(0),
    beginFVLine(0),
    beginFVCol(0),
    curFlags(TextDocumentEditor::FS_NONE),
    text(),
    curLine(0),
    curCol(0),
    match(false),
    mode(M_SEARCH),
    removedText(),
    replaceText()
{}

IncSearch::~IncSearch()
{}


void IncSearch::attach(EditorWidget *newEd)
{
  if (ed) {
    // we're already attached, the user just pressed Ctrl-S while
    // we're searching, so search to next spot
    bool prevMatch = match;
    if (!findString(curFlags | TextDocumentEditor::FS_ADVANCE_ONCE) &&
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
    prevStatusText = status->status->text();
  }

  beginLine = ed->cursorLine();
  beginCol = ed->cursorCol();

  beginFVLine = ed->firstVisibleLine();
  beginFVCol = ed->firstVisibleCol();

  curFlags = TextDocumentEditor::FS_CASE_INSENSITIVE;

  mode = M_SEARCH;

  if (ed->selectEnabled()) {
    // Initialize the search string with the selection.
    TextCoord selLow, selHigh;
    ed->m_editor->getSelectRegion(selLow, selHigh);
    curLine = selLow.line;
    curCol = selLow.column;
    if (selLow.line == selHigh.line) {
      // expected case
      text = ed->m_editor->getTextRange(selLow, selHigh);
    }
    else {
      // truncate to one line...
      text = ed->m_editor->getTextRange(selLow,
        ed->m_editor->lineEndCoord(selLow.line));
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
      addStatusFlag(sb, "^I=insens", (curFlags & TextDocumentEditor::FS_CASE_INSENSITIVE));
      addStatusFlag(sb, "^B=back", (curFlags & TextDocumentEditor::FS_BACKWARDS));
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
    status->status->setText(prevStatusText);
  }

  AttachInputProxy::detach();
}


bool IncSearch::keyPressEvent(QKeyEvent *k)
{
  Qt::KeyboardModifiers modifiers = k->modifiers();

  switch (mode) {
    default: xfailure("bad mode");
    case M_SEARCH:          return searchKeyMap(k, modifiers);
    case M_GET_REPLACEMENT: return getReplacementKeyMap(k, modifiers);
    case M_REPLACE:         return replaceKeyMap(k, modifiers);
  }
}


bool IncSearch::pseudoKeyPress(InputPseudoKey pkey)
{
  switch (mode) {
    default: xfailure("bad mode");
    case M_SEARCH:          return searchPseudoKey(pkey);
    case M_GET_REPLACEMENT: return getReplacementPseudoKey(pkey);
    case M_REPLACE:         return replacePseudoKey(pkey);
  }
}


bool IncSearch::searchKeyMap(QKeyEvent *k, Qt::KeyboardModifiers state)
{
  // Some keys are ignored but shouldn't cause exit from i-search.
  switch (k->key()) {
    case Qt::Key_Shift:
    case Qt::Key_Alt:
    case Qt::Key_Control:
      return false;
  }

  if (state == Qt::NoModifier || state == Qt::ShiftModifier) {
    switch (k->key()) {
      case Qt::Key_Enter:
      case Qt::Key_Return:
        // stop doing i-search
        TRACE("incsearch", "stopping due to Enter");
        detach();
        return true;

      case Qt::Key_Backspace:
        if (text.length() > 0) {
          // remove final character
          text = string(text.c_str(), text.length()-1);
          if (text.length() > 0) {
            // This is not right, or at least may not be what the
            // user expects, since it does not completely undo the
            // effect of typing.  For example, if the buffer contains
            // "a ab", and the user types Ctrl+S, A, B, Backspace,
            // one might expect to end up at the same place as after
            // Ctrl+S, A, but it does not: the latter highlights the
            // first "a" while the former highlights the second.
            //
            // I speculate that I might get what I want by maintaining
            // a stack of previous positions, pushing when the user
            // types and popping on Backspace.  But then what do I do
            // about Left and Right?  The stack effectively forgets
            // them after Backspace.
            findString();     // adjust match
          }
          else {
            // return to the search start position
            resetToSearchStart();
          }
        }
        return true;

      #if 0
      case Qt::Key_Right:
        nextMatch();
        return true;

      case Qt::Key_Left:
        prevMatch();
        return true;

      case Qt::Key_Down:
        ed->moveView(+1, 0);
        ed->redraw();
        return true;

      case Qt::Key_Up:
        ed->moveView(-1, 0);
        ed->redraw();
        return true;

      case Qt::Key_Home:
        // make as much of the left side visible as possible
        ed->setFirstVisibleCol(0);
        ed->scrollToCursor();
        return true;
      #endif // 0

      default: {
        QString s = k->text();
        if (s.length() && s[0].isPrint()) {
          text &= s.toUtf8().constData();

          findString();
          return true;  // handled
        }
        break;
      }
    }
  }

  if (state == Qt::ControlModifier) {
    switch (k->key()) {
      case Qt::Key_I:
        curFlags ^= TextDocumentEditor::FS_CASE_INSENSITIVE;
        findString();
        return true;

      case Qt::Key_B:
        curFlags ^= TextDocumentEditor::FS_BACKWARDS;
        findString();
        return true;

      case Qt::Key_W:
        // grab chars from cursor up to end of next word
        // or end of line
        text &= ed->m_editor->getWordAfter(ed->m_editor->cursor());
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
        removedText = ed->m_editor->getSelectedText();

        // remove the selected occurrence of the match string
        ed->m_editor->deleteSelection();

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
      #endif // 0

      // even though it's a little inconsistent that Ctrl-W extends the
      // selection while Ctrl-Z scrolls, it's the latter functionality
      // that I need most often....
      case Qt::Key_Z:
      case Qt::Key_L:
        // pass these through to editor, so it can scroll
        return false;
    }
  }

  // Ctrl+S itself is handled by the menu hotkey.
  if (state == (Qt::ShiftModifier | Qt::ControlModifier)) {
    prevMatch();
    return true;
  }

  // Unknown key.  Stop the search and indicate it is unhandled so the
  // surrounding editor can handle it.
  TRACE("incsearch", "detaching due to unknown key: " << toString(*k));
  detach();
  return false;
}


bool IncSearch::searchPseudoKey(InputPseudoKey pkey)
{
  switch (pkey) {
    case IPK_CANCEL:
      // return to original location
      resetToSearchStart();

      detach();
      return true;

    default:
      return false;
  }
}


void IncSearch::resetToSearchStart()
{
  this->curLine = this->beginLine;
  this->curCol = this->beginCol;
  ed->cursorTo(TextCoord(beginLine, beginCol));
  ed->setFirstVisible(TextCoord(beginFVLine, beginFVCol));
  ed->clearMark();
  ed->m_hitText = "";
  ed->redraw();
  this->updateStatus();
}


bool IncSearch::findString(TextDocumentEditor::FindStringFlags flags)
{
  {
    TextCoord tc(curLine, curCol);
    match = ed->m_editor->findString(tc, toCStr(text), flags);
    curLine = tc.line;
    curCol = tc.column;
  }
  if (match) {
    // move editor cursor to end of match
    ed->m_editor->setCursor(TextCoord(curLine, curCol + text.length()));

    // put selection start at beginning of match
    ed->m_editor->setMark(TextCoord(curLine, curCol));

    ed->m_editor->scrollToCursor(-1 /*center*/);
  }

  ed->m_hitText = text;

  // the only flag I want the editor using for hit text, for now,
  // is the case sensitivity flag
  ed->m_hitTextFlags = curFlags & TextDocumentEditor::FS_CASE_INSENSITIVE;

  ed->redraw();

  updateStatus();

  return match;
}


void IncSearch::updateStatus()
{
  if (tracingSys("incsearch")) {
    // This serves as a crude way to debug state transitions in this
    // module since nearly everything does 'updateStatus' at the end.
    cout << "---------\n";
    PVAL(beginLine);
    PVAL(beginCol);
    PVAL(beginFVLine);
    PVAL(beginFVCol);
    PVAL(text);
    PVAL(curLine);
    PVAL(curCol);
    PVAL(match);
  }

  if (status) {
    status->status->setText(statusText());

    TRACE("mode", "setting search-mode pixmap");
    status->mode->setPixmap(
      mode==M_SEARCH?          pixmaps->search :
      mode==M_GET_REPLACEMENT? pixmaps->getReplace :
                               pixmaps->replace);
  }

  if (!match) {
    string message = stringc << "not found: \"" << text << "\"";

    // suppose I did a wrap around, would I then find a match, other
    // than the one I'm (possibly) on now?
    int line, col;
    if (tryWrapSearch(line, col)) {
      message &= " (can wrap)";
    }

    ed->showInfo(toCStr(message));
  }
  else {
    ed->hideInfo();
  }
}


bool IncSearch::tryWrapSearch(int &line, int &col) const
{
  // wrap
  TextCoord tc(0,0);
  if (curFlags & TextDocumentEditor::FS_BACKWARDS) {
    tc = ed->m_editor->endCoord();
  }

  // search
  if (ed->m_editor->findString(tc, toCStr(text), curFlags) &&
      !(tc.line==curLine && tc.column==curCol)) {
    // yes, wrapping finds another
    line = tc.line;
    col = tc.column;
    return true;
  }
  else {
    return false;
  }
}


bool IncSearch::nextMatch()
{
  return findString((curFlags | TextDocumentEditor::FS_ADVANCE_ONCE) & ~TextDocumentEditor::FS_BACKWARDS );
}


bool IncSearch::prevMatch()
{
  return findString(curFlags | TextDocumentEditor::FS_ADVANCE_ONCE | TextDocumentEditor::FS_BACKWARDS);
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
  ed->m_editor->insertString(removedText);
  findString();
}


bool IncSearch::getReplacementKeyMap(QKeyEvent *k, Qt::KeyboardModifiers state)
{
  if (state == Qt::NoModifier || state == Qt::ShiftModifier) {
    switch (k->key()) {
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
          ed->m_editor->deleteLR(true /*left*/, 1);

          replaceText = string(toCStr(replaceText), replaceText.length()-1);
        }
        return true;

      default: {
        QString s = k->text();
        if (s.length() && s[0].isPrint()) {
          QByteArray utf8(s.toUtf8());
          char const *p = utf8.constData();

          ed->m_editor->insertText(utf8.constData(), utf8.size());

          // This does not handle NULs properly...
          replaceText &= p;

          return true;  // handled
        }
        break;
      }
    }
  }

  return true;    // handled..
}


bool IncSearch::getReplacementPseudoKey(InputPseudoKey pkey)
{
  switch (pkey) {
    case IPK_CANCEL:
      // go back to M_SEARCH
      putBackMatchText();
      setMode(M_SEARCH);
      return true;

    default:
      return false;
  }
}


// --------------------- M_REPLACE ----------------------
bool IncSearch::replace()
{
  // remove match text
  ed->m_editor->deleteLR(true /*left*/, text.length());

  // insert replacement text
  ed->m_editor->insertString(replaceText);

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


bool IncSearch::replaceKeyMap(QKeyEvent *k, Qt::KeyboardModifiers state)
{
  if (state == Qt::NoModifier || state == Qt::ShiftModifier) {
    switch (k->key()) {
      case Qt::Key_Return:
      case Qt::Key_Enter:
      case Qt::Key_Y:
        replace();
        return true;

      case Qt::Key_N:
        // find next match
        if (!findString(curFlags | TextDocumentEditor::FS_ADVANCE_ONCE)) {
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


bool IncSearch::replacePseudoKey(InputPseudoKey pkey)
{
  return false;
}


// EOF
