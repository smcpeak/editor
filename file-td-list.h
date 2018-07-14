// file-td-list.h
// Declaration of FileTextDocumentList class.

#ifndef FILE_TD_LIST_H
#define FILE_TD_LIST_H

// editor
#include "file-td.h"                   // NamedTextDocument

// smbase
#include "array.h"                     // ObjArrayStack
#include "macros.h"                    // NO_OBJECT_COPIES
#include "refct-serf.h"                // SerfRefCount
#include "sobjlist.h"                  // SObjList


// Forward in this file.
class FileTextDocumentListObserver;
class FileTextDocumentInitialView;


// A list of open files being edited.
//
// The order is significant; this is another sort of "document" within
// the editor, and the order of elements within the list is something
// the user will see and can manipulate.
//
// In addition to storing the list, this class provides various methods
// for manipulating and querying it, per the requirements of a
// multi-document interactive text editor.  My hope is to use this to
// help encapsulate all of the functionality not directly related to
// pixels and keystrokes that is currently in EditorWindow and
// GlobalState, similar to how TextDocumentEditor encapsulates all of
// the text editing procedures that previously were in EditorWidget.
class FileTextDocumentList : public SerfRefCount {
  // Sensible copying of lists is possible but non-trivial.
  NO_OBJECT_COPIES(FileTextDocumentList);

private:     // data
  // Set of observers who will be notified of changes.
  RCSerfList<FileTextDocumentListObserver> m_observers;

  // When true, we have an open iterator on 'm_observers', which means
  // it cannot be changed.
  bool m_iteratingOverObservers;

  // The list of open files.  We maintain the invariant that there is
  // always at least one file, which might be an untitled placeholder.
  //
  // Additionally, the entries have the following requirements:
  //
  // * Every 'filename' must be non-empty and unique.  A client must
  //   verify that 'findFileByName(n)==NULL' before changing the
  //   filename to 'n'.
  //
  // * Every 'title' must be non-empty and unique.  Use
  //   'findFileByTitle' to check, or 'computeUniqueTitle' to generate
  //   a unique title.
  //
  // * Every 'hotkeyDigit' must be unique among those files for which
  //   'hasHotkeyDigit' is true.  Use 'findFileByHotkey' to check, or
  //   'computeUniqueHotkey' to generate a unique hotkey.
  //
  // * Every 'windowMenuId' must be unique.  Clients are expected to
  //   ensure this on their own by using a process-wide counter.
  //
  ObjArrayStack<NamedTextDocument> m_fileDocuments;

public:      // funcs
  // Initially there is one untitled file.
  FileTextDocumentList();

  // The destructor does *not* notify observers.
  ~FileTextDocumentList();

  // Check that invariants hold.  Throw assertion exception if not.
  void selfCheck() const;

  // ------------ fileDocuments: basic list operations --------------
  // Get the number of files in the list.  Always at least 1.
  int numFiles() const;

  // Get the file at a particular position in [0,numFiles()-1].
  NamedTextDocument       *getFileAt (int index);
  NamedTextDocument const *getFileAtC(int index) const;

  // Return true if the given file is in the list.
  bool hasFile(NamedTextDocument const *file) const;

  // Return the index in [0,numFiles()-1] of the given file, or -1
  // if the file is not in the list.
  int getFileIndex(NamedTextDocument const *file) const;

  // Add the given file to our collection.  It must already have a
  // unique filename.  This routine will modify its title and hotkey,
  // if necessary, to ensure they are unique too.  The file is added
  // to the end of the list.
  void addFile(NamedTextDocument *file);

  // Remove the given file from the collection and notify all
  // observers.  This does *not* deallocate 'file'!  Instead, ownership
  // of the object is transferred to the caller.
  //
  // If 'file' is the last file, this method does 'createUntitledFile',
  // including notifying observers, before removing 'file' (and then
  // notifying again).
  void removeFile(NamedTextDocument *file);

  // Move the indicated file to the given index, shifting other files
  // to make room.  It must be present in the list and 'newIndex' must
  // be in [0,numFiles()-1].
  void moveFile(NamedTextDocument *file, int newIndex);

  // --------------- fileDocuments: other operations ----------------
  // Create a new untitled file and add it the end of the list.  It
  // will have a name like "untitled.txt" or "untitled$N.txt" such that
  // it is unique, and its 'isUntitled' field will be true.
  NamedTextDocument *createUntitledFile();

  // Find and return the document with the given name, else NULL.
  NamedTextDocument       *findFileByName (string const &name);
  NamedTextDocument const *findFileByNameC(string const &name) const;

  // Find and return the document with the given title, else NULL.
  NamedTextDocument       *findFileByTitle (string const &title);
  NamedTextDocument const *findFileByTitleC(string const &title) const;

  // Find and return the document that has a hotkey equal to
  // 'hotkeyDigit', else NULL.
  NamedTextDocument       *findFileByHotkey (int hotkeyDigit);
  NamedTextDocument const *findFileByHotkeyC(int hotkeyDigit) const;

  // Find and return the document that has the given 'windowMenuId',
  // else NULL.
  NamedTextDocument       *findFileByWindowMenuId (int id);
  NamedTextDocument const *findFileByWindowMenuIdC(int id) const;

  // Find a file that is untitled and has no modifications, else NULL.
  NamedTextDocument       *findUntitledUnmodifiedFile ();
  NamedTextDocument const *findUntitledUnmodifiedFileC() const;

  // Compute a title based on 'filename' that is not the title of any
  // file in the list.
  //
  // As a special exception to the usual invariant, this method is
  // allowed to be called while one file's title is temporarily empty
  // so that it does not play a role in the uniqueness check.
  string computeUniqueTitle(string filename) const;

  // Given a file that is already in the collection (with a unique
  // filename, per usual) compute a new unique title based on the
  // filename and update 'file' to have that title.
  void assignUniqueTitle(NamedTextDocument *file);

  // Compute a hotkey digit that no file is currently using, or return
  // false if all are in use.
  bool computeUniqueHotkey(int /*OUT*/ &digit) const;

  // Compute and assign a unique hotkey.  There may not be any
  // unused hotkeys, in which case remove any hotkey from the file.
  void assignUniqueHotkey(NamedTextDocument *file);

  // Put into 'dirs' the unique set of directories containing files
  // currently open, in order from most to least recently used.  Any
  // existing entries in 'dirs' are *retained* ahead of added entries.
  void getUniqueDirectories(ArrayStack<string> /*INOUT*/ &dirs) const;

  // ------------------------- observers ----------------------------
  // Add an observer.  It must not already be one.
  void addObserver(FileTextDocumentListObserver *observer);

  // Remove an observer, which must be one now.
  void removeObserver(FileTextDocumentListObserver *observer);

  // ----------------- observer notification --------------------
  // Call 'fileTextDocumentAdded(file)' for all observers.
  void notifyAdded(NamedTextDocument *file);

  // Call 'fileTextDocumentRemoved(file)' for all observers.
  void notifyRemoved(NamedTextDocument *file);

  // Call 'fileTextDocumentAttributeChanged(file)' for all observers.
  //
  // If a client changes an attribute without using one of the methods
  // in this class, the client should call this function.
  void notifyAttributeChanged(NamedTextDocument *file);

  // Call 'fileTextDocumentListOrderChanged()' for all observers.
  void notifyListOrderChanged();

  // Call 'getFileTextDocumentInitialView' for all observers until one
  // returns true; else false if none do so.
  bool notifyGetInitialView(NamedTextDocument *file,
    FileTextDocumentInitialView /*OUT*/ &view);
};


// Interface for an observer of a FileTextDocumentList.
//
// All methods have default no-op implementations.  There is no need
// for subclass implementations to call them.
//
// Currently, it is not allowed for an observer method to invoke a
// method on the observee that modifies the set of observers.
//
// These method names are relatively long because it is expected that
// a class implementing the interface will itself have many members,
// and these methods need to be uniquely named among that larger set.
//
// As these are notification methods, they should not throw exceptions.
//
// This inherts SerfRefCount for the same reason, and in the same way,
// that TextDocumentObserver does.
class FileTextDocumentListObserver : virtual public SerfRefCount {
public:      // funcs
  // A file was added to the list.
  virtual void fileTextDocumentAdded(
    FileTextDocumentList *documentList, NamedTextDocument *file) noexcept;

  // A file was removed.  When this is called, the file has already
  // been removed from the list, but the object is still valid.
  virtual void fileTextDocumentRemoved(
    FileTextDocumentList *documentList, NamedTextDocument *file) noexcept;

  // An attribute of a file may have changed.  The client has to
  // inspect the file to determine what has changed.
  //
  // The attributes that trigger notification are:
  //   - hotkeyDigit
  //   - filename, isUntitled
  //   - title
  //   - highlighter
  //
  // The existence of this method is a slight abuse of the FTDLO
  // concept, since we are notifying about a single object, rather
  // that something intrinsically tied to the "list" aspect.
  virtual void fileTextDocumentAttributeChanged(
    FileTextDocumentList *documentList, NamedTextDocument *file) noexcept;

  // The order of files in the list may have changed.  Observers must
  // query the list in order to obtain the new order.
  virtual void fileTextDocumentListOrderChanged(
    FileTextDocumentList *documentList) noexcept;

  // This is a question, not a notification.  Some widget is about to
  // show 'file' for the first time and wants to know a good view area
  // within the file to start at.  If the observer has one, it should
  // fill in 'view' and return true; else false.
  virtual bool getFileTextDocumentInitialView(
    FileTextDocumentList *documentList, NamedTextDocument *file,
    FileTextDocumentInitialView /*OUT*/ &view) noexcept;

  // Silence dumb warnings.
  virtual ~FileTextDocumentListObserver();
};


// Details about a view of a document suitable for another view
// to be constructed based on it.
struct FileTextDocumentInitialView {
  // Upper-left grid spot.
  TextCoord firstVisible;

  // Location of cursor.
  TextCoord cursor;
};


#endif // FILE_TD_LIST_H
