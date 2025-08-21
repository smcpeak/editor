// td.cc
// code for td.h

#include "td.h"                        // this module

#include "history.h"                   // HE_text
#include "range-text-repl.h"           // RangeTextReplacement

// smbase
#include "smbase/chained-cond.h"       // smbase::cc::{le_le, z_le_le}
#include "smbase/exc.h"                // GENERIC_CATCH_BEGIN
#include "smbase/gdvalue.h"            // gdv::GDValue
#include "smbase/mysig.h"              // printSegfaultAddrs
#include "smbase/objcount.h"           // CHECK_OBJECT_COUNT
#include "smbase/overflow.h"           // safeToInt
#include "smbase/sm-macros.h"          // DEFINE_ENUMERATION_TO_STRING_OR
#include "smbase/string-util.h"        // stringToVectorOfUChar
#include "smbase/trace.h"              // TRACE

using namespace gdv;
using namespace smbase;


DEFINE_ENUMERATION_TO_STRING_OR(
  DocumentProcessStatus,
  NUM_DOCUMENT_PROCESS_STATUSES,
  (
    "DPS_NONE",
    "DPS_RUNNING",
    "DPS_FINISHED",
  ),
  "DPS_<invalid>"
)


gdv::GDValue toGDValue(DocumentProcessStatus dps)
{
  return GDValue(GDVSymbol(toString(dps)));
}


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


void TextDocument::selfCheck() const
{
  m_core.selfCheck();

  xassert(cc::z_le_le(m_historyIndex, m_history.seqLength()));

  xassert(cc::le_le(-1, m_savedHistoryIndex, m_history.seqLength()));
}


TextDocument::operator gdv::GDValue() const
{
  GDValue m(GDVK_TAGGED_ORDERED_MAP, "TextDocument"_sym);
  GDV_WRITE_MEMBER_SYM(m_core);
  // TODO: GDV_WRITE_MEMBER_SYM(m_history);
  GDV_WRITE_MEMBER_SYM(m_historyIndex);
  GDV_WRITE_MEMBER_SYM(m_savedHistoryIndex);
  // TODO: GDV_WRITE_MEMBER_SYM(m_groupStack);
  GDV_WRITE_MEMBER_SYM(m_documentProcessStatus);
  GDV_WRITE_MEMBER_SYM(m_readOnly);
  return m;
}


void TextDocument::clearHistory()
{
  m_historyIndex = 0;
  m_savedHistoryIndex = -1;     // no historyIndex is known to correspond to on-disk
  m_history.truncate(m_historyIndex);
  m_groupStack.clear();

  this->m_core.notifyMetadataChange();
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
  this->m_core.notifyMetadataChange();
}


std::vector<unsigned char> TextDocument::getWholeFile() const
{
  return this->m_core.getWholeFile();
}


void TextDocument::replaceWholeFile(std::vector<unsigned char> const &bytes)
{
  this->m_core.replaceWholeFile(bytes);

  // Clear history after contents have been replaced.
  this->clearHistory();
  this->noUnsavedChanges();
}


std::string TextDocument::getWholeFileString() const
{
  return m_core.getWholeFileString();
}


void TextDocument::replaceWholeFileString(std::string const &str)
{
  // This does not just call the `m_core` method because we want to
  // clear the history, etc.
  replaceWholeFile(stringToVectorOfUChar(str));
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


void TextDocument::insertAt(TextMCoord tc, char const *text, int textLen)
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


void TextDocument::deleteAt(TextMCoord tc, int byteCount)
{
  if (byteCount > 0) {
    HE_text *e = new HE_text(tc,
                             false /*insertion*/,
                             NULL /*text*/, 0 /*textLen*/);
    e->computeText(m_core, byteCount);
    e->apply(m_core, false /*reverse*/);
    appendElement(e);
  }
}


void TextDocument::deleteTextRange(TextMCoordRange const &range)
{
  xassert(range.isRectified());

  int byteCount = m_core.countBytesInRange(range);
  this->deleteAt(range.m_start, byteCount);
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


void TextDocument::replaceMultilineRange(
  TextMCoordRange const &range, std::string const &text)
{
  xassert(validRange(range));

  TextDocumentHistoryGrouper grouper(*this);

  if (!range.empty()) {
    deleteTextRange(range);
  }

  if (!text.empty()) {
    insertAt(range.m_start, text.data(), safeToInt(text.size()));
  }
}


void TextDocument::applyRangeTextReplacement(
  RangeTextReplacement const &repl)
{
  if (repl.m_range) {
    replaceMultilineRange(*repl.m_range, repl.m_text);
  }
  else {
    replaceWholeFileString(repl.m_text);
  }
}


void TextDocument::bumpHistoryIndex(int inc)
{
  bool equalBefore = (this->m_historyIndex == this->m_savedHistoryIndex);

  this->m_historyIndex += inc;

  bool equalAfter = (this->m_historyIndex == this->m_savedHistoryIndex);

  // This is called fairly frequently, so we try to only send the
  // notification when it might matter.
  if (equalBefore != equalAfter) {
    this->m_core.notifyMetadataChange();
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

    if (m_savedHistoryIndex > m_historyIndex) {
      // The on-disk contents correspond to a point in the history
      // that we just discarded.
      m_savedHistoryIndex = -1;
    }

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


TextMCoord TextDocument::undo()
{
  xassert(canUndo() && !inUndoGroup());

  this->bumpHistoryIndex(-1);
  return m_history.applyOne(m_core, m_historyIndex, true /*reverse*/);
}


TextMCoord TextDocument::redo()
{
  xassert(canRedo() && !inUndoGroup());

  TextMCoord tc = m_history.applyOne(m_core, m_historyIndex, false /*reverse*/);
  this->bumpHistoryIndex(+1);
  return tc;
}


void TextDocument::addObserver(TextDocumentObserver *observer) const
{
  this->m_core.addObserver(observer);
}

void TextDocument::removeObserver(TextDocumentObserver *observer) const
{
  this->m_core.removeObserver(observer);
}

bool TextDocument::hasObserver(TextDocumentObserver const *observer) const
{
  return this->m_core.hasObserver(observer);
}

void TextDocument::notifyMetadataChange() const
{
  this->m_core.notifyMetadataChange();
}


void TextDocument::printHistory(std::ostream &sb) const
{
  m_history.printWithMark(sb, 0 /*indent*/, m_historyIndex);
}

void TextDocument::printHistory() const
{
  std::ostringstream sb;
  printHistory(sb);
  cout << sb.str();
  cout.flush();
}


void TextDocument::printHistoryStats() const
{
  HistoryStats stats;
  historyStats(stats);
  stats.printInfo();
}


// -------------------- TextDocumentHistoryGrouper ---------------------
TextDocumentHistoryGrouper::TextDocumentHistoryGrouper(TextDocument &doc)
  : m_doc(&doc)
{
  m_doc->beginUndoGroup();
}


TextDocumentHistoryGrouper::~TextDocumentHistoryGrouper() noexcept
{
  GENERIC_CATCH_BEGIN

  m_doc->endUndoGroup();

  GENERIC_CATCH_END
}


// EOF
