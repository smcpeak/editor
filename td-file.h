// td-file.h
// TextDocumentFile class.

#ifndef TD_FILE_H
#define TD_FILE_H

#include "hilite.h"                    // Highlighter
#include "td.h"                        // TextDocument

#include "str.h"                       // string

#include <stdint.h>                    // int64_t


// This class binds a TextDocument, which is an abstract mathematical
// object, to a File, which is a resource that exists outside the
// editor process.  The document is saved to, loaded from, and checked
// against the resource at appropriate points: hence we have a file
// name and timestamp.
//
// This class further associates that binding with several ways of
// naming it from within the editor application: the hotkey, the window
// menu id, and the file title.
//
// Finally, it contains an interpretation of the file's meaning in
// the form of a syntax highlighter.
//
// All of the data in this class is shared by all editor windows that
// operate on a given file.
class TextDocumentFile : public TextDocument {
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

#endif // TD_FILE_H
