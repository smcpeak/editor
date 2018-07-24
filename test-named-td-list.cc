// test-named-td-list.cc
// Tests for 'named-td-list' module.

#include "named-td-list.h"             // module to test

// smbase
#include "sm-file-util.h"              // SMFileUtil
#include "sm-noexcept.h"               // NOEXCEPT
#include "sm-override.h"               // OVERRIDE
#include "strutil.h"                   // dirname
#include "test.h"                      // USUAL_TEST_MAIN

// libc
#include <stdarg.h>                    // va_start, etc.


// Kinds of notifications.
enum NotifyFunction {
  NF_ADDED,
  NF_REMOVED,
  NF_ATTRIBUTE,
  NF_ORDER
};


// An observer that simply accumulates a record of its notifications,
// then removes them as they are checked for correctness.
class TestObserver : public NamedTextDocumentListObserver {
public:      // types
  // Record of a received notification.
  struct Notification {
    NotifyFunction m_nfunc;
    NamedTextDocument *m_file;

    Notification(NotifyFunction n, NamedTextDocument *f = NULL)
      : m_nfunc(n),
        m_file(f)
    {}
  };

public:      // data
  // Received but not yet checked notifications.
  ObjList<Notification> m_pendingNotifications;

  // We will only use this class with a single list at a time.
  NamedTextDocumentList *m_documentList;

public:      // funcs
  TestObserver(NamedTextDocumentList *d)
    : m_documentList(d)
  {}

  void namedTextDocumentAdded(
    NamedTextDocumentList *documentList, NamedTextDocument *file) NOEXCEPT OVERRIDE
  {
    xassert(documentList == m_documentList);
    m_pendingNotifications.append(new Notification(NF_ADDED, file));
  }

  void namedTextDocumentRemoved(
    NamedTextDocumentList *documentList, NamedTextDocument *file) NOEXCEPT OVERRIDE
  {
    xassert(documentList == m_documentList);
    m_pendingNotifications.append(new Notification(NF_REMOVED, file));
  }

  void namedTextDocumentAttributeChanged(
    NamedTextDocumentList *documentList, NamedTextDocument *file) NOEXCEPT OVERRIDE
  {
    xassert(documentList == m_documentList);
    m_pendingNotifications.append(new Notification(NF_ATTRIBUTE, file));
  }

  void namedTextDocumentListOrderChanged(
    NamedTextDocumentList *documentList) NOEXCEPT OVERRIDE
  {
    xassert(documentList == m_documentList);
    m_pendingNotifications.append(new Notification(NF_ORDER));
  }

  // Remove the next notification and check its attributes.
  void expect(NotifyFunction nfunc, NamedTextDocument *file = NULL);

  void expectEmpty() { xassert(m_pendingNotifications.isEmpty()); }

  void expectOnly(NotifyFunction nfunc, NamedTextDocument *file = NULL)
    { expect(nfunc, file); expectEmpty(); }
};


void TestObserver::expect(NotifyFunction nfunc, NamedTextDocument *file)
{
  xassert(m_pendingNotifications.isNotEmpty());
  Notification *n = m_pendingNotifications.removeFirst();
  xassert(n->m_nfunc == nfunc);
  xassert(n->m_file == file);
  delete n;
}


// Add a file with a specific name.
static NamedTextDocument *add(NamedTextDocumentList &dlist, string name)
{
  NamedTextDocument *file = new NamedTextDocument;
  file->setFilename(name);
  dlist.addDocument(file);
  return file;
}


static NamedTextDocument *createUntitled(NamedTextDocumentList &dlist)
{
  return dlist.createUntitledDocument(SMFileUtil().currentDirectory());
}


// Just some simple things to get started.
static void testSimple()
{
  NamedTextDocumentList dlist;
  xassert(dlist.numDocuments() == 1);

  TestObserver observer(&dlist);
  dlist.addObserver(&observer);

  NamedTextDocument *file0 = dlist.getDocumentAt(0);
  xassert(!file0->hasFilename());
  xassert(dlist.getDocumentIndex(file0) == 0);
  xassert(dlist.hasDocument(file0));
  xassert(dlist.getDocumentIndex(NULL) == -1);
  xassert(!dlist.hasDocument(NULL));

  observer.expectEmpty();

  NamedTextDocument *file1 = createUntitled(dlist);
  xassert(!file1->hasFilename());
  xassert(dlist.numDocuments() == 2);
  xassert(dlist.getDocumentIndex(file1) == 1);

  observer.expectOnly(NF_ADDED, file1);

  dlist.removeDocument(file0);
  delete file0;
  xassert(dlist.numDocuments() == 1);
  xassert(dlist.getDocumentIndex(file1) == 0);

  observer.expectOnly(NF_REMOVED, file0);

  dlist.selfCheck();
  dlist.removeObserver(&observer);
}


// Expect the files to be in a particular order.  NULL ends the list.
static void expectOrder(NamedTextDocumentList &dlist, NamedTextDocument *file0, ...)
{
  xassert(dlist.getDocumentAt(0) == file0);
  int i = 1;

  va_list args;
  va_start(args, file0);
  while (NamedTextDocument *file = va_arg(args, NamedTextDocument*)) {
    xassert(dlist.getDocumentAt(i) == file);
    i++;
  }
  va_end(args);

  xassert(dlist.numDocuments() == i);
}


// Make several files, rearrange them, then remove them.
static void testAddMoveRemove()
{
  NamedTextDocumentList dlist;
  xassert(dlist.numDocuments() == 1);

  TestObserver observer(&dlist);
  dlist.addObserver(&observer);

  NamedTextDocument *file0 = dlist.getDocumentAt(0);
  xassert(!file0->hasFilename());
  xassert(dlist.getDocumentIndex(file0) == 0);
  xassert(dlist.getDocumentIndex(NULL) == -1);

  observer.expectEmpty();

  NamedTextDocument *file1 = add(dlist, "file1");
  xassert(file1->m_title == "file1");
  xassert(file1->hasHotkey());
  xassert(dlist.findDocumentByName("file1") == file1);
  xassert(dlist.findDocumentByTitle("file1") == file1);
  xassert(dlist.findDocumentByHotkey(file1->getHotkeyDigit()) == file1);
  xassert(dlist.findDocumentByWindowMenuId(file1->m_windowMenuId) == file1);
  xassert(dlist.findDocumentByWindowMenuId(-1) == NULL);

  observer.expectOnly(NF_ADDED, file1);

  NamedTextDocument *file2 = add(dlist, "a/file2");
  xassert(file2->m_title == "file2");
  xassert(dlist.findDocumentByName("a/file2") == file2);
  xassert(dlist.findDocumentByTitle("file2") == file2);
  xassert(dlist.findDocumentByHotkey(file2->getHotkeyDigit()) == file2);
  xassert(dlist.findDocumentByWindowMenuId(file2->m_windowMenuId) == file2);

  observer.expectOnly(NF_ADDED, file2);

  // Title uniqueness has to include a directory component.
  NamedTextDocument *file3 = add(dlist, "b/file2");
  xassert(file3->m_title == "b/file2");

  observer.expectOnly(NF_ADDED, file3);

  // Title uniqueness has to append a digit.
  NamedTextDocument *file4 = add(dlist, "file2");
  xassert(file4->m_title == "file2:2");

  observer.expectOnly(NF_ADDED, file4);

  // Check the order
  expectOrder(dlist, file0, file1, file2, file3, file4, NULL);

  // Do some rearranging.
  dlist.moveDocument(file4, 1);
  observer.expectOnly(NF_ORDER);
  expectOrder(dlist, file0, file4, file1, file2, file3, NULL);
  dlist.moveDocument(file0, 4);
  observer.expectOnly(NF_ORDER);
  expectOrder(dlist, file4, file1, file2, file3, file0, NULL);
  dlist.moveDocument(file2, 3);
  observer.expectOnly(NF_ORDER);
  expectOrder(dlist, file4, file1, file3, file2, file0, NULL);

  // Remove files.
  dlist.removeDocument(file1);
  observer.expectOnly(NF_REMOVED, file1);
  expectOrder(dlist, file4, file3, file2, file0, NULL);
  delete file1;

  dlist.removeDocument(file0);
  observer.expectOnly(NF_REMOVED, file0);
  expectOrder(dlist, file4, file3, file2, NULL);
  delete file0;

  dlist.removeDocument(file4);
  observer.expectOnly(NF_REMOVED, file4);
  expectOrder(dlist, file3, file2, NULL);
  delete file4;

  dlist.removeDocument(file3);
  observer.expectOnly(NF_REMOVED, file3);
  expectOrder(dlist, file2, NULL);
  delete file3;

  dlist.removeDocument(file2);
  file0 = dlist.getDocumentAt(0);     // New untitled file.
  observer.expect(NF_ADDED, file0);
  observer.expectOnly(NF_REMOVED, file2);
  expectOrder(dlist, file0, NULL);
  delete file2;

  observer.expectEmpty();

  dlist.removeObserver(&observer);
}


// Create several untitled files.
static void testCreateUntitled()
{
  NamedTextDocumentList dlist;
  TestObserver observer(&dlist);
  dlist.addObserver(&observer);

  NamedTextDocument *file0 = dlist.getDocumentAt(0);

  NamedTextDocument *file1 = createUntitled(dlist);
  observer.expectOnly(NF_ADDED, file1);
  xassert(file1->name() == "untitled2.txt");

  NamedTextDocument *file2 = createUntitled(dlist);
  observer.expectOnly(NF_ADDED, file2);
  xassert(file2->name() == "untitled3.txt");

  // Test 'findUntitledUnmodifiedFile'.
  NamedTextDocument *f = dlist.findUntitledUnmodifiedDocument();
  xassert(f != NULL);

  file1->insertAt(TextCoord(0,0), "hi", 2);
  f = dlist.findUntitledUnmodifiedDocument();
  xassert(f == file0 || f == file2);

  file2->setFilename(file2->name());      // Make it no longer untitled.
  f = dlist.findUntitledUnmodifiedDocument();
  xassert(f == file0);

  file0->insertAt(TextCoord(0,0), "\n", 1);
  f = dlist.findUntitledUnmodifiedDocument();
  xassert(f == NULL);

  dlist.removeObserver(&observer);
}


// Exercise a "Save as..." scenario.
static void testSaveAs()
{
  NamedTextDocumentList dlist;
  TestObserver observer(&dlist);
  dlist.addObserver(&observer);

  NamedTextDocument *file0 = dlist.getDocumentAt(0);
  file0->setFilename("a/some-name.txt");
  dlist.assignUniqueTitle(file0);
  observer.expectOnly(NF_ATTRIBUTE, file0);
  xassert(file0->m_title == "some-name.txt");

  dlist.removeObserver(&observer);
}


// Exhaust hotkeys.
static void testExhaustHotkeys()
{
  NamedTextDocumentList dlist;
  TestObserver observer(&dlist);
  dlist.addObserver(&observer);

  for (int i=0; i<10; i++) {
    NamedTextDocument *file = createUntitled(dlist);
    observer.expectOnly(NF_ADDED, file);
  }

  xassert(dlist.numDocuments() == 11);
  NamedTextDocument *file10 = dlist.getDocumentAt(10);
  xassert(!file10->hasHotkey());

  // Remove some files.
  for (int i=2; i <= 7; i++) {
    NamedTextDocument *file = dlist.getDocumentAt(2);
    dlist.removeDocument(file);
    observer.expectOnly(NF_REMOVED, file);
    delete file;
  }

  // Now should be able to assign hotkey for file10.
  dlist.assignUniqueHotkey(file10);
  observer.expectOnly(NF_ATTRIBUTE, file10);
  xassert(file10->hasHotkey());

  // Check 'removeObserver' incidentally.
  dlist.removeObserver(&observer);
  createUntitled(dlist);
  observer.expectEmpty();
}


// Add a file that already has an assigned hotkey that clashes with
// an existing file.
static void testDuplicateHotkeys()
{
  NamedTextDocumentList dlist;

  NamedTextDocument *file0 = dlist.getDocumentAt(0);
  NamedTextDocument *file1 = createUntitled(dlist);

  xassert(file0->hasHotkey());
  xassert(file1->hasHotkey());

  dlist.removeDocument(file1);
  file1->setHotkeyDigit(file0->getHotkeyDigit());
  dlist.addDocument(file1);

  // Should have had its hotkey reassigned.
  xassert(file1->hasHotkey());
  xassert(file1->getHotkeyDigit() != file0->getHotkeyDigit());

  // Now remove and add, expecting it to retain its hotkey.
  int hotkey = file1->getHotkeyDigit();
  dlist.removeDocument(file1);
  dlist.addDocument(file1);
  xassert(file1->getHotkeyDigit() == hotkey);
}


// Provoke a name like "a:3".
static void testColon3()
{
  NamedTextDocumentList dlist;

  // Also exercise the no-op observer functions.
  NamedTextDocumentListObserver observer;
  dlist.addObserver(&observer);

  NamedTextDocument *file1 = add(dlist, "a/b");
  xassert(file1->m_title == "b");

  NamedTextDocument *file2 = add(dlist, "b:2");
  xassert(file2->m_title == "b:2");

  NamedTextDocument *file3 = add(dlist, "b");
  xassert(file3->m_title == "b:3");

  dlist.removeDocument(file3);
  delete file3;

  dlist.moveDocument(file2, 0);

  file2->setFilename("zoo");
  dlist.assignUniqueTitle(file2);

  dlist.removeObserver(&observer);
}


// Expect output of 'getUniqueDirectories' to match the NULL-terminated
// sequence of arguments.
static void expectDirs(NamedTextDocumentList &dlist, char const *dir0, ...)
{
  ArrayStack<string> actual;
  dlist.getUniqueDirectories(actual);

  if (!dir0) {
    xassert(actual.isEmpty());
    return;
  }
  xassert(!actual.isEmpty());

  xassert(actual[0] == dir0);
  int i = 1;

  va_list args;
  va_start(args, dir0);
  while (char const *dir = va_arg(args, char const *)) {
    xassert(actual[i] == dir);
    i++;
  }
  va_end(args);

  xassert(actual.length() == i);
}


static void testGetUniqueDirectories()
{
  NamedTextDocumentList dlist;
  expectDirs(dlist, NULL);

  add(dlist, "/a/b");
  expectDirs(dlist, "/a", NULL);

  // Check that existing entries are preserved.
  {
    ArrayStack<string> actual;
    actual.push("existing");
    dlist.getUniqueDirectories(actual);
    xassert(actual.length() == 2);
    xassert(actual[0] == "existing");
    xassert(actual[1] == "/a");
  }

  add(dlist, "/a/c");
  expectDirs(dlist, "/a", NULL);

  add(dlist, "/b/c");
  expectDirs(dlist, "/a", "/b", NULL);

  add(dlist, "/b/d/e/f/g");
  expectDirs(dlist, "/a", "/b", "/b/d/e/f", NULL);
}


void entry()
{
  testSimple();
  testAddMoveRemove();
  testCreateUntitled();
  testSaveAs();
  testExhaustHotkeys();
  testDuplicateHotkeys();
  testColon3();
  testGetUniqueDirectories();

  xassert(NamedTextDocument::s_objectCount == 0);
  xassert(TextDocument::s_objectCount == 0);

  cout << "test-named-td-list passed\n";
}

USUAL_TEST_MAIN


// EOF
