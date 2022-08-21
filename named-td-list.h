// named-td-list.h
// Declaration of NamedTextDocumentList class.

#ifndef NAMED_TD_LIST_H
#define NAMED_TD_LIST_H

// editor
#include "named-td.h"                  // NamedTextDocument

// smbase
#include "array.h"                     // ObjArrayStack
#include "sm-macros.h"                 // NO_OBJECT_COPIES
#include "refct-serf.h"                // SerfRefCount
#include "sm-noexcept.h"               // NOEXCEPT
#include "sobjlist.h"                  // SObjList


// Forward in this file.
class NamedTextDocumentListObserver;
class NamedTextDocumentInitialView;


// A list of named documents being edited.
//
// The order is significant; this is another sort of "document" within
// the editor, and the order of elements within the list is something
// the user will see and can manipulate.
//
// In addition to storing the list, this class provides various methods
// for manipulating and querying it, per the requirements of a
// multi-document interactive text editor.
class NamedTextDocumentList : public SerfRefCount {
  // Sensible copying of lists is possible but non-trivial.
  NO_OBJECT_COPIES(NamedTextDocumentList);

private:     // data
  // Set of observers who will be notified of changes.
  RCSerfList<NamedTextDocumentListObserver> m_observers;

  // When true, we have an open iterator on 'm_observers', which means
  // it cannot be changed.
  bool m_iteratingOverObservers;

  // The list of open documents.  We maintain the invariant that there
  // is always at least one document, which might be an untitled
  // placeholder.
  //
  // Additionally, the entries have the following requirements:
  //
  // * Every 'm_documentName' must be non-empty and unique.  A client
  //   must verify that 'findDocumentByName(n)==NULL' before changing
  //   the name to 'n'.
  //
  // * Every 'title' must be non-empty and unique.  Use
  //   'findDocumentByTitle' to check, or 'computeUniqueTitle' to generate
  //   a unique title.
  //
  ObjArrayStack<NamedTextDocument> m_documents;

public:      // funcs
  // Initially there is one untitled document.
  NamedTextDocumentList();

  // The destructor does *not* notify observers.
  ~NamedTextDocumentList();

  // Check that invariants hold.  Throw assertion exception if not.
  void selfCheck() const;

  // -------------- documents: basic list operations ----------------
  // Get the number of documents in the list.  Always at least 1.
  int numDocuments() const;

  // Get the document at a particular position in [0,numDocuments()-1].
  NamedTextDocument       *getDocumentAt (int index);
  NamedTextDocument const *getDocumentAtC(int index) const;

  // Return true if the given document is in the list.
  bool hasDocument(NamedTextDocument const *doc) const;

  // Return the index in [0,numDocument()-1] of the given document, or
  // -1 if the document is not in the list.
  int getDocumentIndex(NamedTextDocument const *doc) const;

  // Add the given document to our collection.  It must already have a
  // unique name.  This routine will modify its title, if necessary, to
  // ensure it is unique too.  The document is added to the end of the
  // list, and 'this' takes ownership of it.
  void addDocument(NamedTextDocument *doc);

  // Remove the given document from the collection and notify all
  // observers.  This does *not* deallocate 'doc'!  Instead, ownership
  // of the object is transferred to the caller.
  //
  // If 'doc' is the last document, this method does 'createUntitledDocument',
  // including notifying observers, before removing 'doc' (and then
  // notifying again).
  void removeDocument(NamedTextDocument *doc);

  // Move the indicated document to the given index, shifting other documents
  // to make room.  It must be present in the list and 'newIndex' must
  // be in [0,numDocuments()-1].
  void moveDocument(NamedTextDocument *doc, int newIndex);

  // ----------------- documents: other operations ------------------
  // Create a new untitled document and add it the end of the list.  It
  // will have a name like "untitled.txt" or "untitled$N.txt" such that
  // it is unique, and its 'hasFilename()' will be false.
  NamedTextDocument *createUntitledDocument(string const &dir);

  // Find and return the document with the given name, else NULL.
  NamedTextDocument       *findDocumentByName (DocumentName const &name);
  NamedTextDocument const *findDocumentByNameC(DocumentName const &name) const;

  // Find and return the document with the given title, else NULL.
  NamedTextDocument       *findDocumentByTitle (string const &title);
  NamedTextDocument const *findDocumentByTitleC(string const &title) const;

  // Find a document that is untitled and has no modifications, else NULL.
  NamedTextDocument       *findUntitledUnmodifiedDocument ();
  NamedTextDocument const *findUntitledUnmodifiedDocumentC() const;

  // Compute a title based on 'name' that is not the title of any
  // document in the list.
  //
  // As a special exception to the usual invariant, this method is
  // allowed to be called while one document's title is temporarily empty
  // so that it does not play a role in the uniqueness check.
  string computeUniqueTitle(DocumentName const &docName) const;

  // Given a document that is already in the collection (with a unique
  // name, per usual) compute a new unique title based on the
  // name and update 'doc' to have that title.
  void assignUniqueTitle(NamedTextDocument *doc);

  // Put into 'dirs' the unique set of directories containing files
  // currently open, in order from most to least recently used.  Any
  // existing entries in 'dirs' are *retained* ahead of added entries.
  void getUniqueDirectories(ArrayStack<HostAndResourceName> /*INOUT*/ &dirs) const;

  // ------------------------- observers ----------------------------
  // Add an observer.  It must not already be one.
  void addObserver(NamedTextDocumentListObserver *observer);

  // Remove an observer, which must be one now.
  void removeObserver(NamedTextDocumentListObserver *observer);

  // ----------------- observer notification --------------------
  // Call 'namedTextDocumentAdded(doc)' for all observers.
  void notifyAdded(NamedTextDocument *doc);

  // Call 'namedTextDocumentRemoved(doc)' for all observers.
  void notifyRemoved(NamedTextDocument *doc);

  // Call 'namedTextDocumentAttributeChanged(doc)' for all observers.
  //
  // If a client changes an attribute without using one of the methods
  // in this class, the client should call this function.
  void notifyAttributeChanged(NamedTextDocument *doc);

  // Call 'namedTextDocumentListOrderChanged()' for all observers.
  void notifyListOrderChanged();

  // Call 'getNamedTextDocumentInitialView' for all observers until one
  // returns true; else false if none do so.
  bool notifyGetInitialView(NamedTextDocument *doc,
    NamedTextDocumentInitialView /*OUT*/ &view);
};


// Interface for an observer of a NamedTextDocumentList.
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
class NamedTextDocumentListObserver : virtual public SerfRefCount {
public:      // funcs
  // A document was added to the list.
  virtual void namedTextDocumentAdded(
    NamedTextDocumentList *documentList, NamedTextDocument *doc) NOEXCEPT;

  // A document was removed.  When this is called, the document has already
  // been removed from the list, but the object is still valid.
  virtual void namedTextDocumentRemoved(
    NamedTextDocumentList *documentList, NamedTextDocument *doc) NOEXCEPT;

  // An attribute of a document may have changed.  The client has to
  // inspect the document to determine what has changed.
  //
  // The attributes that trigger notification are:
  //   - name, hasFilename
  //   - title
  //   - highlighter
  //
  // The existence of this method is a slight abuse of the FTDLO
  // concept, since we are notifying about a single object, rather
  // that something intrinsically tied to the "list" aspect.
  virtual void namedTextDocumentAttributeChanged(
    NamedTextDocumentList *documentList, NamedTextDocument *doc) NOEXCEPT;

  // The order of documents in the list may have changed.  Observers must
  // query the list in order to obtain the new order.
  virtual void namedTextDocumentListOrderChanged(
    NamedTextDocumentList *documentList) NOEXCEPT;

  // This is a question, not a notification.  Some widget is about to
  // show 'doc' for the first time and wants to know a good view area
  // within the document to start at.  If the observer has one, it should
  // fill in 'view' and return true; else false.
  virtual bool getNamedTextDocumentInitialView(
    NamedTextDocumentList *documentList, NamedTextDocument *doc,
    NamedTextDocumentInitialView /*OUT*/ &view) NOEXCEPT;

  // Silence dumb warnings.
  virtual ~NamedTextDocumentListObserver();
};


// Details about a view of a document suitable for another view
// to be constructed based on it.
struct NamedTextDocumentInitialView {
  // Upper-left grid spot.
  TextLCoord firstVisible;

  // Location of cursor.
  TextLCoord cursor;
};


#endif // NAMED_TD_LIST_H
