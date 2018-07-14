// named-td.h
// NamedTextDocument class.

#ifndef NAMED_TD_H
#define NAMED_TD_H

#include "hilite.h"                    // Highlighter
#include "td.h"                        // TextDocument

#include "str.h"                       // string

#include <stdint.h>                    // int64_t


// This class binds a TextDocument, which is an abstract mathematical
// object, to a name, which refers somehow to the origin of the
// contents of the document.  Often, this is a file name, but it can
// also be, for example, a command line.
//
// If the name refers to a durable external resource, such as a file,
// then the document is saved to, loaded from, and checked against the
// resource at appropriate points.
//
// This class further associates that binding with several ways of
// naming it from within the editor application: the hotkey, the window
// menu id, and the document title.
//
// Finally, it contains an interpretation of the document's meaning in
// the form of a syntax highlighter.
//
// All of the data in this class is shared by all editor windows that
// operate on a given document.
class NamedTextDocument : public TextDocument {
private:     // static data
  // Next value to use when assigning menu ids.
  static int s_nextWindowMenuId;

public:      // static data
  static int s_objectCount;

private:     // data
  // True if there is a hotkey the user can use to jump to this buffer.
  bool m_hasHotkeyDigit;

  // Digit the user can press Alt with to jump to this buffer, if
  // 'hasHotkeyDigit'.  It is a number in [0,9].
  int m_hotkeyDigit;

  // Name of document.  This is a filename if 'm_hasFilename' is true.
  // Otherwise, it is a human-readable string describing the origin of
  // the content.  It must be unique within the list of
  // NamedTextDocuments in its containing NamedTextDocumentList.  It
  // must not be empty.
  string m_name;

  // When true, 'm_name' is the name of a file on disk.
  bool m_hasFilename;

  // Directory associated with this document.  For a file, this is the
  // directory containing the file.  For process output, it is the
  // working directory of the process.  For others, it's somewhat
  // arbitrary, with the working directory of the editor itself acting
  // as the final fallback.  It must always end with a path separator
  // character.
  string m_directory;

public:      // data
  // Modification timestamp (unix time) the last time we interacted
  // with it on the file system.
  //
  // This is 0 for an untitled document or a file that does not yet exist,
  // although there is never a reason to explicitly check for that
  // since we have 'hasFilename' for the former, and for the latter, we
  // always try to stat() the file before comparing its timestamp.
  int64_t m_lastFileTimestamp;

  // Title of the document.  Must be unique within the containing
  // NamedTextDocumentList.  This will usually be similar to the name,
  // but perhaps shortened so long as it remains unique.
  string m_title;

  // Numeric identifier for this buffer.  This is used to identify
  // it in the Window menu.
  int const m_windowMenuId;

  // current highlighter; clients can come in and replace the
  // highlighter, but it must always be the case that the
  // highlighter is attached to 'this' buffer (because it's allowed
  // to maintain internal incremental state about the buffer
  // contents)
  Highlighter *m_highlighter;      // (nullable owner)

  // When true, the widget will highlight instances of whitespace at
  // the end of a line.  Initially true, but is set to false by
  // 'setProcessOutputStatus' for other than DPS_NONE.
  //
  // In a sense, this is a sort of "overlay" highlighter, as it acts
  // after the main highlighter.  I could perhaps generalize the idea
  // of highlighting compositions at some point.
  bool m_highlightTrailingWhitespace;

private:     // funcs
  void setDirectory(string const &dir);

public:      // funcs
  // Create an anonymous document.  The caller must call either
  // 'setFilename' or 'setNonFileName' before adding it to a document
  // list.
  //
  // TODO: Should I force those to be passed to the constructor?
  NamedTextDocument();

  ~NamedTextDocument();

  // Perform additional actions when setting process status.
  virtual void setDocumentProcessStatus(DocumentProcessStatus status) OVERRIDE;

  // ----------------------------- names ----------------------------
  // Get the document's unique (within its NamedTextDocumentList) name.
  string name() const                  { return m_name; }

  // True if the document's name is a file name.
  bool hasFilename() const             { return m_hasFilename; }

  // Get the filename for this document.  Requires 'hasFilename()'.
  string filename() const;

  // Set 'm_name' to be 'filename', and 'm_hasFilename' to true.  It is
  // the caller's responsibility to ensure uniqueness within the
  // containing NamedTextDocumentList.  This also sets 'm_directory' to
  // the directory of the file.
  void setFilename(string const &filename);

  // Set 'm_name' to 'name' and 'm_hasFilename' to false.  The name
  // still has to be unique.  Sets 'm_directory' to 'dir'.
  void setNonFileName(string const &name, string const &dir);

  // Get the directory associated with the document.
  string directory() const             { return m_directory; }

  // ---------------------------- hotkeys ---------------------------
  // Return true if this buffer has an assigned hotkey.
  bool hasHotkey() const { return this->m_hasHotkeyDigit; }

  // Get the hotkey digit in [0,9].  Asserts 'hasHotkey()'.
  int getHotkeyDigit() const;

  // Human-readable description of 'hotkey'; might return "".
  string hotkeyDesc() const;

  // Remove the hotkey, if any.
  void clearHotkey();

  // Set the hotkey to the indicated digit in [0,9].
  void setHotkeyDigit(int digit);

  // -------------------- file system interaction -------------------
  // Read the from 'filename()'.  Requires 'hasFilename()'.  Updates
  // the disk modification time.
  //
  // Throws an exception on error, but in that case this object will be
  // left unmodified.
  void readFile();

  // Write to 'filename()'.  Requires 'hasFilename()'.  Updates the disk
  // modification time.  May throw.
  void writeFile();

  // Get the modification time of filename() without consulting
  // or modifying 'lastFileTimestamp'.  Return false if it cannot
  // be obtained.
  bool getDiskModificationTime(int64_t &modTime) const;

  // Compare 'lastFileTimestamp' to what is on disk.  Return true
  // if they are different, meaning some on-disk change has happened
  // since we last interacted with it.
  //
  // If '!hasFilename()', then this always returns false, since in that
  // case we are not really associated with any on-disk file.
  bool hasStaleModificationTime() const;

  // Set 'lastFileTimestamp' to equal the on-disk timestamp.
  void refreshModificationTime();
};

#endif // NAMED_TD_H
