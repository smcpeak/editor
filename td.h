// td.h
// Text document with undo/redo history attached to it.

#ifndef EDITOR_TD_H
#define EDITOR_TD_H

#include "td-fwd.h"                    // fwds for this module

// editor
#include "history.h"                   // HE_group
#include "td-core.h"                   // TextDocumentCore, TextDocumentObserver

// smbase
#include "smbase/array.h"              // ArrayStack
#include "smbase/gdvalue-fwd.h"        // gdv::GDValue
#include "smbase/objstack.h"           // ObjStack
#include "smbase/refct-serf.h"         // RCSerf

// libc++
#include <vector>                      // std::vector


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

// Return "DPS_NONE", etc.
char const *toString(DocumentProcessStatus dps);

// Return a symbol named as `toString`.
gdv::GDValue toGDValue(DocumentProcessStatus dps);


// This class represents a text document (which is a sequence of lines)
// and its undo/redo history.
//
// It has basically the same interface as TextDocumentCore, plus the
// some additional functionality (like undo/redo).  But it does not
// inherit TextDocumentCore, it instead explicitly repeats that
// interface and delegates to 'm_core'.
class TextDocument : public SerfRefCount {
public:       // types
  typedef TextDocumentCore::VersionNumber VersionNumber;

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
  //
  // invariant: 0 <= m_historyIndex <= m_history.seqLength()
  int m_historyIndex;

  // what index in 'history' corresponds to the file's on-disk
  // contents?  the client of this interface has to inform me when
  // the file gets saved, but I'll track when the changes get away
  // from that point; 'savedHistoryIndex' tracks 'historyIndex' when
  // the contents are in correspondence and we're moving across
  // nondestructive actions
  //
  // This is -1 if the on-disk contents are not known to correspond
  // to any point in the history.
  //
  // invariant: -1 <= m_savedHistoryIndex <= m_history.seqLength()
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

  virtual void selfCheck() const;

  // Dump internals for test/debug purposes.
  virtual operator gdv::GDValue() const;

  // Read-only access to the underlying representation.  Use of this
  // should be infrequent, as I prefer to use the delegation queries.
  TextDocumentCore const &getCore() const { return m_core; }

  // --------------------- query document core --------------------
  // These are simple pass-through delegation queries.  They are
  // declared in the same order as in TextDocumentCore.  The
  // modification routines are *not* exposed because document changes
  // must go through the undo/redo mechanism in this class.

  int numLines() const                                       { return m_core.numLines(); }
  bool isEmptyLine(int line) const                           { return m_core.isEmptyLine(line); }
  int lineLengthBytes(int line) const                        { return m_core.lineLengthBytes(line); }
  bool validCoord(TextMCoord tc) const                       { return m_core.validCoord(tc); }
  bool validRange(TextMCoordRange const &range) const        { return m_core.validRange(range); }
  TextMCoord beginCoord() const                              { return m_core.beginCoord(); }
  TextMCoord endCoord() const                                { return m_core.endCoord(); }
  TextMCoord lineBeginCoord(int line) const                  { return m_core.lineBeginCoord(line); }
  TextMCoord lineEndCoord(int line) const                    { return m_core.lineEndCoord(line); }
  int maxLineLengthBytes() const                             { return m_core.maxLineLengthBytes(); }
  int numLinesExceptFinalEmpty() const                       { return m_core.numLinesExceptFinalEmpty(); }
  bool walkCoordBytes(TextMCoord &tc, int distance) const    { return m_core.walkCoordBytes(tc, distance); }
  int countBytesInRange(TextMCoordRange const &range) const  { return m_core.countBytesInRange(range); }
  bool adjustMCoord(TextMCoord /*INOUT*/ &tc) const          { return m_core.adjustMCoord(tc); }
  bool adjustMCoordRange(TextMCoordRange /*INOUT*/ &range) const { return m_core.adjustMCoordRange(range); }
  void getPartialLine(TextMCoord tc, ArrayStack<char> /*INOUT*/ &dest, int numBytes) const { return m_core.getPartialLine(tc, dest, numBytes); }
  bool getTextSpanningLines(TextMCoord tc, ArrayStack<char> /*INOUT*/ &dest, int numBytes) const { return m_core.getTextSpanningLines(tc, dest, numBytes); }
  void getTextForRange(TextMCoordRange const &range, ArrayStack<char> /*INOUT*/ &dest) const { m_core.getTextForRange(range, dest); }
  void getWholeLine(int line, ArrayStack<char> /*INOUT*/ &dest) const { return m_core.getWholeLine(line, dest); }
  int countLeadingSpacesTabs(int line) const                 { return m_core.countLeadingSpacesTabs(line); }
  int countTrailingSpacesTabs(int line) const                { return m_core.countTrailingSpacesTabs(line); }
  VersionNumber getVersionNumber() const                     { return m_core.getVersionNumber(); }

  // This is a modification of sorts, but does not need undo/redo.
  void bumpVersionNumber()                                   { m_core.bumpVersionNumber(); }

  // ---------------------- extra attributes ----------------------
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

  // Return the entire contents of the file as a byte sequence.
  std::vector<unsigned char> getWholeFile() const;

  // Replace the file contents with those from 'bytes'.  Resets cursor
  // to 0,0 and clears the undo history and undo group stack.
  void replaceWholeFile(std::vector<unsigned char> const &bytes);

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
  void insertAt(TextMCoord tc, char const *text, int textLen);

  // Delete 'byteCount' bytes at (to the right of) 'tc'.  This
  // may span lines.  Each end-of-line counts as one byte.
  // 'tc' must be valid for the document.
  void deleteAt(TextMCoord tc, int byteCount);

  // Delete text specified by a range.
  void deleteTextRange(TextMCoordRange const &range);

  // Convenience functions to append to the end of the document.
  void appendText(char const *text, int textLen);
  void appendCStr(char const *s);
  void appendString(string const &s);

  // -------------------------- undo/redo --------------------------
  // Group actions with HE_group.
  //
  // NOTE: 'replaceWholeFile' clears the undo group stack, even if there
  // are open groups.  When that happens, 'endUndoGroup' silently does
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
  TextMCoord undo();
  TextMCoord redo();

  // Do the current contents differ from those we remember saving?
  bool unsavedChanges() const;

  // Remember the current historyIndex as one where the file's contents
  // agree with those on the disk.
  void noUnsavedChanges();

  // -------------------------- observers ---------------------------
  // Add a new observer of this document's contents.  This observer
  // must not already be observing this document.
  //
  // This is `const` for consistency with
  // `TextDocumentCore::addObserver`.
  void addObserver(TextDocumentObserver *observer) const;

  // Remove an observer, which must be observing this document.
  void removeObserver(TextDocumentObserver *observer) const;

  // Return true if 'observer' is among our current observers.
  bool hasObserver(TextDocumentObserver const *observer) const;

  // Send `observeMetadataChange` to observers.
  void notifyMetadataChange() const;

  // ------------------------- diagnostics --------------------------
  // print the history in a textual format, with the current history
  // index marked (or no mark if history index is at the end)
  void printHistory(std::ostream &sb) const;
  void printHistory(/*to stdout*/) const;

  // get statistics about history memory usage
  void historyStats(HistoryStats &stats) const
    { m_history.stats(stats); }
  void printHistoryStats() const;

  // ---------------------- iterator ----------------------------
public:      // types
  // Iterate over the bytes in a line.
  //
  // TODO UTF-8: Allow iteration over code points.
  class LineIterator {
    NO_OBJECT_COPIES(LineIterator);

  private:     // instance data
    // Underlying iterator.
    TextDocumentCore::LineIterator m_iter;

  public:      // funcs
    // Same interface as TextDocumentCore::LineIterator.
    LineIterator(TextDocument const &td, int line)
      : m_iter(td.getCore(), line)     {}
    ~LineIterator()                    {}
    bool has() const                   { return m_iter.has(); }
    int byteOffset() const             { return m_iter.byteOffset(); }
    int byteAt() const                 { return m_iter.byteAt(); }
    void advByte()                     { return m_iter.advByte(); }
  };
};


#endif // EDITOR_TD_H
