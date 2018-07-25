// td.cc
// code for td.h

#include "td.h"                        // this module

// smbase
#include "autofile.h"                  // AutoFILE
#include "macros.h"                    // DEFINE_ENUMERATION_TO_STRING
#include "mysig.h"                     // printSegfaultAddrs
#include "objcount.h"                  // CHECK_OBJECT_COUNT
#include "trace.h"                     // TRACE


DEFINE_ENUMERATION_TO_STRING(
  DocumentProcessStatus,
  NUM_DOCUMENT_PROCESS_STATUSES,
  (
    "DPS_NONE",
    "DPS_RUNNING",
    "DPS_FINISHED"
  )
)


// ------------------------- TextDocument ---------------------------
int TextDocument::s_objectCount = 0;

CHECK_OBJECT_COUNT(TextDocument);


TextDocument::TextDocument()
  : m_core(),
    m_history(),
    m_historyIndex(0),
    m_savedHistoryIndex(0),
    m_groupStack(),
    m_documentProcessStatus(DPS_NONE),
    m_readOnly(false)
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
  m_historyIndex = 0;
  m_savedHistoryIndex = -1;     // no historyIndex is known to correspond to on-disk
  m_history.truncate(m_historyIndex);
  m_groupStack.clear();

  this->m_core.notifyUnsavedChangesChange(this);
}


void TextDocument::clearContentsAndHistory()
{
  clearHistory();
  m_core.clear();
}


bool TextDocument::unsavedChanges() const
{
  if (m_documentProcessStatus == DPS_RUNNING) {
    return false;
  }

  if (m_savedHistoryIndex == m_historyIndex) {
    // It seems there are no unsaved changes, but we also need to check
    // the group stack.  If the group stack is non-empty, then there
    // are changes that haven't been combined and added to the normal
    // history yet.
    if (!this->m_groupStack.isEmpty()) {
      // Really I should inspect every element of the stack, but my
      // ObjStack class does not allow that.  Anyway, I happen to know
      // that we never push more than one element onto it.
      HE_group const *g = this->m_groupStack.topC();
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
  this->m_savedHistoryIndex = this->m_historyIndex;

  // This method is called rarely, there is no problem with sending the
  // notification unconditionally.
  this->m_core.notifyUnsavedChangesChange(this);
}


void TextDocument::readFile(string const &fname)
{
  // This might throw an exception, but if so, 'core' will be
  // left unmodified.
  this->m_core.readFile(fname.c_str());

  // Clear history after file has been successfully read.
  this->clearHistory();
  this->noUnsavedChanges();
}


void TextDocument::writeFile(string const &fname) const
{
  this->m_core.writeFile(fname.c_str());
}


void TextDocument::setDocumentProcessStatus(DocumentProcessStatus status)
{
  xassert(m_groupStack.isEmpty());
  m_documentProcessStatus = status;
  if (m_documentProcessStatus != DPS_NONE) {
    // DPS_FINISHED is included here because when the process finishes
    // I want to be sure to get a document with no "unsaved changes".
    this->clearHistory();
    this->noUnsavedChanges();
  }
  if (m_documentProcessStatus == DPS_RUNNING) {
    this->setReadOnly(true);
  }
}


void TextDocument::setReadOnly(bool readOnly)
{
  m_readOnly = readOnly;
}


void TextDocument::insertAt(TextCoord tc, char const *text, int textLen)
{
  // Ignore insertions of nothing.
  if (textLen > 0) {
    HE_text *e = new HE_text(tc,
                             true /*insertion*/,
                             text, textLen);
    e->apply(m_core, false /*reverse*/);
    appendElement(e);
  }
}


void TextDocument::deleteAt(TextCoord tc, int count)
{
  if (count > 0) {
    HE_text *e = new HE_text(tc,
                             false /*insertion*/,
                             NULL /*text*/, 0 /*textLen*/);
    e->computeText(m_core, count);
    e->apply(m_core, false /*reverse*/);
    appendElement(e);
  }
}


void TextDocument::appendText(char const *text, int textLen)
{
  this->insertAt(this->endCoord(), text, textLen);
}

void TextDocument::appendCStr(char const *s)
{
  this->appendText(s, strlen(s));
}

void TextDocument::appendString(string const &s)
{
  this->appendCStr(s.c_str());
}


void TextDocument::bumpHistoryIndex(int inc)
{
  bool equalBefore = (this->m_historyIndex == this->m_savedHistoryIndex);

  this->m_historyIndex += inc;

  bool equalAfter = (this->m_historyIndex == this->m_savedHistoryIndex);

  // This is called fairly frequently, so we try to only send the
  // notification when it might matter.
  if (equalBefore != equalAfter) {
    this->m_core.notifyUnsavedChangesChange(this);
  }
}


void TextDocument::appendElement(HistoryElt *e)
{
  if (m_documentProcessStatus == DPS_RUNNING) {
    // While it is running, discard undo/redo, but once it finishes,
    // resume tracking history.
    delete e;
    return;
  }

  if (m_groupStack.isEmpty()) {
    // for now, adding a new element means truncating the history
    m_history.truncate(m_historyIndex);
    m_history.append(e);
    this->bumpHistoryIndex(+1);
  }
  else {
    m_groupStack.top()->append(e);
  }
}


void TextDocument::beginUndoGroup()
{
  m_groupStack.push(new HE_group);
}

void TextDocument::endUndoGroup()
{
  if (m_groupStack.isEmpty()) {
    // Silently ignore.  One way this can happen is if the file is
    // reloaded while an undo group is open.  The worst case is some
    // actions the user thinks of as a single action will end up
    // separate.  This could happen if, for example, we reload the file
    // and then insert text, all as one UI operation.  But most
    // commonly, there are no undoable actions anyway.
    return;
  }

  HE_group *g = m_groupStack.pop();
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
  return m_history.applyOne(m_core, m_historyIndex, true /*reverse*/);
}


TextCoord TextDocument::redo()
{
  xassert(canRedo() && !inUndoGroup());

  TextCoord tc = m_history.applyOne(m_core, m_historyIndex, false /*reverse*/);
  this->bumpHistoryIndex(+1);
  return tc;
}


void TextDocument::addObserver(TextDocumentObserver *observer)
{
  this->m_core.addObserver(observer);
}

void TextDocument::removeObserver(TextDocumentObserver *observer)
{
  this->m_core.removeObserver(observer);
}

bool TextDocument::hasObserver(TextDocumentObserver const *observer) const
{
  return this->m_core.hasObserver(observer);
}


void TextDocument::printHistory(stringBuilder &sb) const
{
  m_history.printWithMark(sb, 0 /*indent*/, m_historyIndex);
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
