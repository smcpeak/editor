// td.h
// Text document with undo/redo history attached to it.

#ifndef TD_H
#define TD_H

// editor
#include "history.h"                   // HE_group
#include "td-core.h"                   // TextDocumentCore

// smbase
#include "array.h"                     // ArrayStack
#include "objstack.h"                  // ObjStack
#include "refct-serf.h"                // RCSerf


// The state of the process feeding output to a document.
enum DocumentProcessStatus {
  // There was never a process associated with this document.
  DPS_NONE,

  // The process is still running.
  DPS_RUNNING,

  // The process has finished.
  DPS_FINISHED,

  NUM_DOCUMENT_PROCESS_STATUSES
};


// This class represents a text document (which is a sequence of lines)
// and its undo/redo history.
class TextDocument : public SerfRefCount {
public:       // static data
  static int s_objectCount;

private:      // data
  // The sequence of text lines without any history information.
  TextDocumentCore m_core;

  // modification history
  HE_group m_history;

  // where are we in that history?  usually,
  // historyIndex==history.seqLength(), meaning we're at the end of the
  // recorded history; undo/redo modifies 'historyIndex' and 'buf' but
  // not 'history'
  int m_historyIndex;

  // what index in 'history' corresponds to the file's on-disk
  // contents?  the client of this interface has to inform me when
  // the file gets saved, but I'll track when the changes get away
  // from that point; 'savedHistoryIndex' tracks 'historyIndex' when
  // the contents are in correspondence and we're moving across
  // nondestructive actions
  //
  // invariant: -1 <= savedHistoryIndex <= seq.Length()
  int m_savedHistoryIndex;

  // stack of open history groups, which will soon be collapsed
  // and added to their parent group, or 'history' for the last
  // (outermost) group; typically this stack is empty, or has
  // just one element between beginGroup() and endGroup(), but
  // I allow for the generality of a stack anyway
  ObjStack<HE_group> m_groupStack;

  // State of an associated process, if any.
  //
  // If this is not DPS_NONE, which is the default, then we do not
  // retain any undo/redo history, and objects looking at this document
  // may behave differently (for example, automatically moving their
  // cursor to the end of the document).
  DocumentProcessStatus m_documentProcessStatus;

  // If true, the user interface should prevent attempts to modify the
  // document contents (the lines of text).  Initially false.
  //
  // The purpose of this flag is to prevent unintended changes that will
  // probably not get saved, such as editing the output of a process or
  // a file that is read-only on disk.  This is *not* a form of access
  // control.  The user is assumed to have the ability to turn off the
  // read-only flag on any document if they want to.
  //
  // The methods of this class do *not* enforce the read-only property.
  // It is entirely up to the UI to do that.  Part of the reason for not
  // enforcing read-only here is I want ProcessWatcher to be able to
  // freely insert text, as the changes it is making are not valuable,
  // original content, but TextDocument does not understand that.
  bool m_readOnly;

private:     // funcs
  // Change 'historyIndex' by 'inc' and possibly send a notification
  // event to observers.
  void bumpHistoryIndex(int inc);

  // Add an element either to the open group or to the undo list,
  // itself if there is no open group.
  void appendElement(HistoryElt *e);

public:      // funcs
  TextDocument();      // empty buffer, empty history, cursor at 0,0
  virtual ~TextDocument();

  // Read-only access to the underlying representation.  Use of this
  // should be unusual.
  TextDocumentCore const &getCore() const { return m_core; }

  // ------------------------ query document -----------------------
  int numLines() const                    { return m_core.numLines(); }
  int lineLength(int line) const          { return m_core.lineLength(line); }
  bool validCoord(TextCoord tc) const     { return m_core.validCoord(tc); }
  TextCoord endCoord() const              { return m_core.endCoord(); }
  int maxLineLength() const               { return m_core.maxLineLength(); }
  int numLinesExceptFinalEmpty() const    { return m_core.numLinesExceptFinalEmpty(); }

  void getLine(TextCoord tc, char *dest, int destLen) const
    { return m_core.getLine(tc, dest, destLen); }
  int countLeadingSpacesTabs(int line) const
    { return m_core.countLeadingSpacesTabs(line); }
  int countTrailingSpacesTabs(int line) const
    { return m_core.countTrailingSpacesTabs(line); }

  DocumentProcessStatus documentProcessStatus() const
    { return m_documentProcessStatus; }
  bool isProcessOutput() const
    { return m_documentProcessStatus != DPS_NONE; }

  bool isReadOnly() const
    { return m_readOnly; }

  // ------------------------ global changes ----------------------
  // clear history, leaving only the current buffer contents
  void clearHistory();

  // clear buffer contents *and* history
  void clearContentsAndHistory();

  // Replace current contents with a new file, and reset cursor
  // to 0,0.  Clears the undo history and undo group stack.
  //
  // If there is a read error, throws an exception, but the document
  // is left unmodified.
  void readFile(string const &fname);

  // Write the contents to 'fname'.  May throw.
  void writeFile(string const &fname) const;

  // Change the 'm_documentProcessStatus' setting.  Setting it to a
  // value DPS_RUNNING will set the document as read-only and
  // immediately discard all undo/redo history.  There must not be any
  // open history groups.
  //
  // This also has an effect on the highlighting state in
  // NamedTextDocument, which is why it is virtual.
  virtual void setDocumentProcessStatus(DocumentProcessStatus status);

  // Change the read-only flag.
  void setReadOnly(bool readOnly);

  // ------------- modify document, appending to history -----------
  // Insert 'text' at 'tc'.  'text' may contain newline characters.
  // 'tc' must be valid for the document.
  void insertAt(TextCoord tc, char const *text, int textLen);

  // Delete 'count' characters at (to the right of) 'tc'.  This
  // may span lines.  Each end-of-line counts as one character.
  // 'tc' must be valid for the document.
  void deleteAt(TextCoord tc, int count);

  // Convenience functions to append to the end of the document.
  void appendText(char const *text, int textLen);
  void appendCStr(char const *s);
  void appendString(string const &s);

  // -------------------------- undo/redo --------------------------
  // Group actions with HE_group.
  //
  // NOTE: 'readFile' clears the undo group stack, even if there are
  // open groups.  When that happens 'endUndoGroup' silently does
  // nothing.
  void beginUndoGroup();
  void endUndoGroup();

  // True if we have an open group; note that undo/redo is not allowed
  // in that case, even though canUndo/Redo() may return true.
  bool inUndoGroup() const    { return m_groupStack.isNotEmpty(); }

  // True if there is additional history available in the corresponding
  // direction, and hence the operation can be invoked.
  bool canUndo() const        { return m_historyIndex > 0; }
  bool canRedo() const        { return m_historyIndex < m_history.seqLength(); }

  // These return the location at the left edge of the modified text.
  TextCoord undo();
  TextCoord redo();

  // Do the current contents differ from those we remember saving?
  bool unsavedChanges() const;

  // Remember the current historyIndex as one where the file's contents
  // agree with those on the disk.
  void noUnsavedChanges();

  // -------------------------- observers ---------------------------
  // Add a new observer of this document's contents.  This observer
  // must not already be observing this document.
  void addObserver(TextDocumentObserver *observer);

  // Remove an observer, which must be observing this document.
  void removeObserver(TextDocumentObserver *observer);

  // Return true if 'observer' is among our current observers.
  bool hasObserver(TextDocumentObserver const *observer) const;

  // ------------------------- diagnostics --------------------------
  // print the history in a textual format, with the current history
  // index marked (or no mark if history index is at the end)
  void printHistory(stringBuilder &sb) const;
  void printHistory(/*to stdout*/) const;

  // get statistics about history memory usage
  void historyStats(HistoryStats &stats) const
    { m_history.stats(stats); }
  void printHistoryStats() const;
};


#endif // TD_H
