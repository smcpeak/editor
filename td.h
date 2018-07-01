// td.h
// Text document with undo/redo history attached to it.

#ifndef TD_H
#define TD_H

#include "history.h"                   // HE_group
#include "td-core.h"                   // TextDocumentCore

#include "array.h"                     // ArrayStack
#include "objstack.h"                  // ObjStack


// This class represents a text document (which is a sequence of lines)
// and its undo/redo history.
class TextDocument {
private:      // data
  // The sequence of text lines without any history information.
  TextDocumentCore core;

  // modification history
  HE_group history;

  // where are we in that history?  usually,
  // historyIndex==history.seqLength(), meaning we're at the end of the
  // recorded history; undo/redo modifies 'historyIndex' and 'buf' but
  // not 'history'
  int historyIndex;

  // what index in 'history' corresponds to the file's on-disk
  // contents?  the client of this interface has to inform me when
  // the file gets saved, but I'll track when the changes get away
  // from that point; 'savedHistoryIndex' tracks 'historyIndex' when
  // the contents are in correspondence and we're moving across
  // nondestructive actions
  //
  // invariant: -1 <= savedHistoryIndex <= seq.Length()
  int savedHistoryIndex;

  // stack of open history groups, which will soon be collapsed
  // and added to their parent group, or 'history' for the last
  // (outermost) group; typically this stack is empty, or has
  // just one element between beginGroup() and endGroup(), but
  // I allow for the generality of a stack anyway
  ObjStack<HE_group> groupStack;

private:     // funcs
  void appendElement(HistoryElt *e);

public:      // funcs
  TextDocument();      // empty buffer, empty history, cursor at 0,0
  ~TextDocument();

  // Read-only access to the underlying representation.  Use of this
  // should be unusual.
  TextDocumentCore const &getCore() const { return core; }

  // -------------------- query document shape --------------------
  int numLines() const                    { return core.numLines(); }
  int lineLength(int line) const          { return core.lineLength(line); }
  bool validCoord(TextCoord tc) const     { return core.validCoord(tc); }
  TextCoord endCoord() const              { return core.endCoord(); }
  void getLine(TextCoord tc, char *dest, int destLen) const
    { return core.getLine(tc, dest, destLen); }
  int maxLineLength() const               { return core.maxLineLength(); }

  // ------------------------ global changes ----------------------
  // clear history, leaving only the current buffer contents
  void clearHistory();

  // clear buffer contents *and* history
  void clearContentsAndHistory();

  // Replace current contents with a new file, and reset cursor
  // to 0,0; clears the history.
  //
  // Like ::readFile, if the file cannot be opened, then this throws
  // XOpen and does not modify anything.  But a later read error leaves
  // this object in an incomplete state.
  void readFile(char const *fname);

  // ------------- modify document, appending to history -----------
  // Insert 'text' at 'tc'.  'text' may contain newline characters.
  // 'tc' must be valid for the document.
  void insertAt(TextCoord tc, char const *text, int textLen);

  // Delete 'count' characters at (to the right of) 'tc'.  This
  // may span lines.  Each end-of-line counts as one character.
  // 'tc' must be valid for the document.
  void deleteAt(TextCoord tc, int count);

  // -------------------------- undo/redo --------------------------
  // Group actions with HE_group.
  void beginUndoGroup();
  void endUndoGroup();

  // True if we have an open group; note that undo/redo is not allowed
  // in that case, even though canUndo/Redo() may return true.
  bool inUndoGroup() const    { return groupStack.isNotEmpty(); }

  // True if there is additional history available in the corresponding
  // direction, and hence the operation can be invoked.
  bool canUndo() const        { return historyIndex > 0; }
  bool canRedo() const        { return historyIndex < history.seqLength(); }

  // These return the location at the left edge of the modified text.
  TextCoord undo();
  TextCoord redo();

  // Do the current contents differ from those we remember saving?
  bool unsavedChanges() const;

  // Remember the current historyIndex as one where the file's contents
  // agree with those on the disk.
  void noUnsavedChanges() { savedHistoryIndex = historyIndex; }

  // -------------------------- observers ---------------------------
  // Add a new observer of this document's contents.  This observer
  // must not already be observing this document.
  void addObserver(TextDocumentObserver *observer);

  // Remove an observer, which must be observing this document.
  void removeObserver(TextDocumentObserver *observer);

  // ------------------------- diagnostics --------------------------
  // print the history in a textual format, with the current history
  // index marked (or no mark if history index is at the end)
  void printHistory(stringBuilder &sb) const;
  void printHistory(/*to stdout*/) const;

  // get statistics about history memory usage
  void historyStats(HistoryStats &stats) const
    { history.stats(stats); }
  void printHistoryStats() const;
};


#endif // TD_H
