// text-document-file.h
// a Buffer, plus some state suitable for an editor

// in an editor, the TextDocumentFile would contain all the info that is
// remembered for *undisplayed* buffers

#ifndef TEXT_DOCUMENT_FILE_H
#define TEXT_DOCUMENT_FILE_H

#include "buffer.h"     // Buffer
#include "str.h"        // string
#include "hilite.h"     // Highlighter

#include <stdint.h>     // int64_t


// EditorWidget editing state for a Buffer that is *used* when the
// buffer is shown to the user, and *saved* when it is not.  This data
// is copied between the EditorWidget and the TextDocumentFile object as
// the user cycles among open files.
class SavedEditingState {
public:      // data
  // cursor position (0-based)
  //int cursorLine;
  //int cursorCol;
  // UPDATE: The cursor has been moved into Buffer itself (via
  // TextDocument and CursorBuffer), and so is no longer present
  // in this class.

  // selection state: a location, and a flag to enable it
  int selectLine;
  int selectCol;
  bool selectEnabled;

  #if 0     // moved back into EditorWidget
  // the following fields are valid only after normalizeSelect(),
  // and before any subsequent modification to cursor or select
  int selLowLine, selLowCol;     // whichever of cursor/select comes first
  int selHighLine, selHighCol;   // whichever comes second
  #endif // 0

  // Scrolling offset.  Changes are done via EditorWidget::setView(), which
  // calls EditingState::setFirstVisibleLC().
  int const firstVisibleLine, firstVisibleCol;

  // Information about viewable area; these are set by the
  // EditorWidget::updateView() routine and should be treated as read-only by
  // other code.
  //
  // By "visible", I mean the entire line or column is visible.  It may
  // be that a portion of the next line/col is also visible.
  int lastVisibleLine, lastVisibleCol;

  // when nonempty, any buffer text matching this string will
  // be highlighted in the 'hit' style; match is carried out
  // under influence of 'hitTextFlags'
  string hitText;
  Buffer::FindStringFlags hitTextFlags;

protected:   // funcs
  // Set firstVisibleLine/Col; for use by EditingState::copy()
  // and EditorWidget::setView() *only*.
  void setFirstVisibleLC(int newFirstLine, int newFirstCol);

public:      // funcs
  SavedEditingState();
  ~SavedEditingState();

  // copy editing state from 'src'
  void copySavedEditingState(SavedEditingState const &src);
};


// A Buffer, plus additional data about that buffer that the editor
// UI needs whether or not this buffer is currently shown.
class TextDocumentFile : public Buffer {
private:     // static data
  // Next value to use when assigning menu ids.
  static int nextWindowMenuId;

public:      // static data
  static int objectCount;

private:     // data
  // True if there is a hotkey the user can use to jump to this buffer.
  bool hasHotkeyDigit;

  // Digit the user can press Alt with to jump to this buffer, if
  // 'hasHotkeyDigit'.  It is a number in [0,9].
  int hotkeyDigit;

public:      // data
  // name of file being edited
  string filename;

  // Modification timestamp (unix time) the last time we interacted
  // with it on the file system.
  int64_t lastFileTimestamp;

  // title of the buffer; this will usually be similar
  // to the filename, but perhaps only the last part of
  // the fully-qualified path name, etc.
  string title;

  // Numeric identifier for this buffer.  This is used to identify
  // it in the Window menu.
  int const windowMenuId;

  // true when there are unsaved changes
  //bool changed;
  // replaced with unsavedChanges() method

  // current highlighter; clients can come in and replace the
  // highlighter, but it must always be the case that the
  // highlighter is attached to 'this' buffer (because it's allowed
  // to maintain internal incremental state about the buffer
  // contents)
  Highlighter *highlighter;      // (nullable owner)

  // Saved editing state to be restored to an EditorWidget when
  // the buffer becomes visible again.
  SavedEditingState savedState;

public:      // funcs
  TextDocumentFile();
  ~TextDocumentFile();

  // Return true if this buffer has an assigned hotkey.
  bool hasHotkey() const { return this->hasHotkeyDigit; }

  // Get the hotkey digit in [0,9].  Asserts 'hasHotkey()'.
  int getHotkeyDigit() const;

  // Human-readable description of 'hotkey'; might return "".
  string hotkeyDesc() const;

  // Remove the hotkey, if any.
  void clearHotkey();

  // Set the hotkey to the indicated digit in [0,9].
  void setHotkeyDigit(int digit);

  // Get the modification time of b->filename without consulting
  // or modifying 'lastFileTimestamp'.  Return false if it cannot
  // be obtained.
  bool getDiskModificationTime(int64_t &modTime) const;

  // Compare 'lastFileTimestamp' to what is on disk.  Return true
  // if they are different, meaning some on-disk change has happened
  // since we last interacted with it.
  bool hasStaleModificationTime() const;

  // Set 'lastFileTimestamp' to equal the on-disk timestamp.
  void refreshModificationTime();
};

#endif // TEXT_DOCUMENT_FILE_H
