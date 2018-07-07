// test-file-td-list.cc
// Tests for 'file-td-list' module.

#include "file-td-list.h"              // module to test

#include "test.h"                      // USUAL_MAIN

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
class TestObserver : public FileTextDocumentListObserver {
public:      // types
  // Record of a received notification.
  struct Notification {
    NotifyFunction m_nfunc;
    FileTextDocument *m_file;

    Notification(NotifyFunction n, FileTextDocument *f = NULL)
      : m_nfunc(n),
        m_file(f)
    {}
  };

public:      // data
  // Received but not yet checked notifications.
  ObjList<Notification> m_pendingNotifications;

  // We will only use this class with a single list at a time.
  FileTextDocumentList *m_documentList;

public:      // funcs
  TestObserver(FileTextDocumentList *d)
    : m_documentList(d)
  {}

  void fileTextDocumentAdded(
    FileTextDocumentList *documentList, FileTextDocument *file) override
  {
    xassert(documentList == m_documentList);
    m_pendingNotifications.append(new Notification(NF_ADDED, file));
  }

  void fileTextDocumentRemoved(
    FileTextDocumentList *documentList, FileTextDocument *file) override
  {
    xassert(documentList == m_documentList);
    m_pendingNotifications.append(new Notification(NF_REMOVED, file));
  }

  void fileTextDocumentAttributeChanged(
    FileTextDocumentList *documentList, FileTextDocument *file) override
  {
    xassert(documentList == m_documentList);
    m_pendingNotifications.append(new Notification(NF_ATTRIBUTE, file));
  }

  void fileTextDocumentListOrderChanged(
    FileTextDocumentList *documentList) override
  {
    xassert(documentList == m_documentList);
    m_pendingNotifications.append(new Notification(NF_ORDER));
  }

  // Remove the next notification and check its attributes.
  void expect(NotifyFunction nfunc, FileTextDocument *file = NULL);

  void expectEmpty() { xassert(m_pendingNotifications.isEmpty()); }

  void expectOnly(NotifyFunction nfunc, FileTextDocument *file = NULL)
    { expect(nfunc, file); expectEmpty(); }
};


void TestObserver::expect(NotifyFunction nfunc, FileTextDocument *file)
{
  xassert(m_pendingNotifications.isNotEmpty());
  Notification *n = m_pendingNotifications.removeFirst();
  xassert(n->m_nfunc == nfunc);
  xassert(n->m_file == file);
  delete n;
}


// Add a file with a specific name.
static FileTextDocument *add(FileTextDocumentList &dlist, string name)
{
  FileTextDocument *file = new FileTextDocument;
  file->filename = name;
  file->isUntitled = false;
  dlist.addFile(file);
  return file;
}


// Just some simple things to get started.
static void testSimple()
{
  FileTextDocumentList dlist;
  xassert(dlist.numFiles() == 1);

  TestObserver observer(&dlist);
  dlist.addObserver(&observer);

  FileTextDocument *file0 = dlist.getFileAt(0);
  xassert(file0->isUntitled);
  xassert(dlist.getFileIndex(file0) == 0);
  xassert(dlist.hasFile(file0));
  xassert(dlist.getFileIndex(NULL) == -1);
  xassert(!dlist.hasFile(NULL));

  observer.expectEmpty();

  FileTextDocument *file1 = dlist.createUntitledFile();
  xassert(file1->isUntitled);
  xassert(dlist.numFiles() == 2);
  xassert(dlist.getFileIndex(file1) == 1);

  observer.expectOnly(NF_ADDED, file1);

  dlist.removeFile(file0);
  delete file0;
  xassert(dlist.numFiles() == 1);
  xassert(dlist.getFileIndex(file1) == 0);

  observer.expectOnly(NF_REMOVED, file0);

  dlist.selfCheck();
  dlist.removeObserver(&observer);
}


// Expect the files to be in a particular order.  NULL ends the list.
static void expectOrder(FileTextDocumentList &dlist, FileTextDocument *file0, ...)
{
  xassert(dlist.getFileAt(0) == file0);
  int i = 1;

  va_list args;
  va_start(args, file0);
  while (FileTextDocument *file = va_arg(args, FileTextDocument*)) {
    xassert(dlist.getFileAt(i) == file);
    i++;
  }
  va_end(args);
}


// Make several files, rearrange them, then remove them.
static void testAddMoveRemove()
{
  FileTextDocumentList dlist;
  xassert(dlist.numFiles() == 1);

  TestObserver observer(&dlist);
  dlist.addObserver(&observer);

  FileTextDocument *file0 = dlist.getFileAt(0);
  xassert(file0->isUntitled);
  xassert(dlist.getFileIndex(file0) == 0);
  xassert(dlist.getFileIndex(NULL) == -1);

  observer.expectEmpty();

  FileTextDocument *file1 = add(dlist, "file1");
  xassert(file1->title == "file1");
  xassert(file1->hasHotkey());
  xassert(dlist.findFileByName("file1") == file1);
  xassert(dlist.findFileByTitle("file1") == file1);
  xassert(dlist.findFileByHotkey(file1->getHotkeyDigit()) == file1);
  xassert(dlist.findFileByWindowMenuId(file1->windowMenuId) == file1);
  xassert(dlist.findFileByWindowMenuId(-1) == NULL);

  observer.expectOnly(NF_ADDED, file1);

  FileTextDocument *file2 = add(dlist, "a/file2");
  xassert(file2->title == "file2");
  xassert(dlist.findFileByName("a/file2") == file2);
  xassert(dlist.findFileByTitle("file2") == file2);
  xassert(dlist.findFileByHotkey(file2->getHotkeyDigit()) == file2);
  xassert(dlist.findFileByWindowMenuId(file2->windowMenuId) == file2);

  observer.expectOnly(NF_ADDED, file2);

  // Title uniqueness has to include a directory component.
  FileTextDocument *file3 = add(dlist, "b/file2");
  xassert(file3->title == "b/file2");

  observer.expectOnly(NF_ADDED, file3);

  // Title uniqueness has to append a digit.
  FileTextDocument *file4 = add(dlist, "file2");
  xassert(file4->title == "file2:2");

  observer.expectOnly(NF_ADDED, file4);

  // Check the order
  expectOrder(dlist, file0, file1, file2, file3, file4, NULL);

  // Do some rearranging.
  dlist.moveFile(file4, 1);
  observer.expectOnly(NF_ORDER);
  expectOrder(dlist, file0, file4, file1, file2, file3, NULL);
  dlist.moveFile(file0, 4);
  observer.expectOnly(NF_ORDER);
  expectOrder(dlist, file4, file1, file2, file3, file0, NULL);
  dlist.moveFile(file2, 3);
  observer.expectOnly(NF_ORDER);
  expectOrder(dlist, file4, file1, file3, file2, file0, NULL);

  // Remove files.
  dlist.removeFile(file1);
  observer.expectOnly(NF_REMOVED, file1);
  expectOrder(dlist, file4, file3, file2, file0, NULL);
  delete file1;

  dlist.removeFile(file0);
  observer.expectOnly(NF_REMOVED, file0);
  expectOrder(dlist, file4, file3, file2, NULL);
  delete file0;

  dlist.removeFile(file4);
  observer.expectOnly(NF_REMOVED, file4);
  expectOrder(dlist, file3, file2, NULL);
  delete file4;

  dlist.removeFile(file3);
  observer.expectOnly(NF_REMOVED, file3);
  expectOrder(dlist, file2, NULL);
  delete file3;

  dlist.removeFile(file2);
  file0 = dlist.getFileAt(0);     // New untitled file.
  observer.expect(NF_ADDED, file0);
  observer.expectOnly(NF_REMOVED, file2);
  expectOrder(dlist, file0, NULL);
  delete file2;

  observer.expectEmpty();
}


// Create several untitled files.
static void testCreateUntitled()
{
  FileTextDocumentList dlist;
  TestObserver observer(&dlist);
  dlist.addObserver(&observer);

  FileTextDocument *file0 = dlist.getFileAt(0);

  FileTextDocument *file1 = dlist.createUntitledFile();
  observer.expectOnly(NF_ADDED, file1);
  xassert(file1->filename == "untitled2.txt");

  FileTextDocument *file2 = dlist.createUntitledFile();
  observer.expectOnly(NF_ADDED, file2);
  xassert(file2->filename == "untitled3.txt");

  // Test 'findUntitledUnmodifiedFile'.
  FileTextDocument *f = dlist.findUntitledUnmodifiedFile();
  xassert(f != NULL);

  file1->insertAt(TextCoord(0,0), "hi", 2);
  f = dlist.findUntitledUnmodifiedFile();
  xassert(f == file0 || f == file2);

  file2->isUntitled = false;
  f = dlist.findUntitledUnmodifiedFile();
  xassert(f == file0);

  file0->insertAt(TextCoord(0,0), "\n", 1);
  f = dlist.findUntitledUnmodifiedFile();
  xassert(f == NULL);
}


// Exercise a "Save as..." scenario.
static void testSaveAs()
{
  FileTextDocumentList dlist;
  TestObserver observer(&dlist);
  dlist.addObserver(&observer);

  FileTextDocument *file0 = dlist.getFileAt(0);
  file0->filename = "a/some-name.txt";
  file0->isUntitled = false;
  dlist.assignUniqueTitle(file0);
  observer.expectOnly(NF_ATTRIBUTE, file0);
  xassert(file0->title == "some-name.txt");
}


// Exhaust hotkeys.
static void testExhaustHotkeys()
{
  FileTextDocumentList dlist;
  TestObserver observer(&dlist);
  dlist.addObserver(&observer);

  for (int i=0; i<10; i++) {
    FileTextDocument *file = dlist.createUntitledFile();
    observer.expectOnly(NF_ADDED, file);
  }

  xassert(dlist.numFiles() == 11);
  FileTextDocument *file10 = dlist.getFileAt(10);
  xassert(!file10->hasHotkey());

  // Remove some files.
  for (int i=2; i <= 7; i++) {
    FileTextDocument *file = dlist.getFileAt(2);
    dlist.removeFile(file);
    observer.expectOnly(NF_REMOVED, file);
    delete file;
  }

  // Now should be able to assign hotkey for file10.
  dlist.assignUniqueHotkey(file10);
  observer.expectOnly(NF_ATTRIBUTE, file10);
  xassert(file10->hasHotkey());

  // Check 'removeObserver' incidentally.
  dlist.removeObserver(&observer);
  dlist.createUntitledFile();
  observer.expectEmpty();
}


// Add a file that already has an assigned hotkey that clashes with
// an existing file.
static void testDuplicateHotkeys()
{
  FileTextDocumentList dlist;

  FileTextDocument *file0 = dlist.getFileAt(0);
  FileTextDocument *file1 = dlist.createUntitledFile();

  xassert(file0->hasHotkey());
  xassert(file1->hasHotkey());

  dlist.removeFile(file1);
  file1->setHotkeyDigit(file0->getHotkeyDigit());
  dlist.addFile(file1);

  // Should have had its hotkey reassigned.
  xassert(file1->hasHotkey());
  xassert(file1->getHotkeyDigit() != file0->getHotkeyDigit());

  // Now remove and add, expecting it to retain its hotkey.
  int hotkey = file1->getHotkeyDigit();
  dlist.removeFile(file1);
  dlist.addFile(file1);
  xassert(file1->getHotkeyDigit() == hotkey);
}


// Provoke a name like "a:3".
static void testColon3()
{
  FileTextDocumentList dlist;

  // Also exercise the no-op observer functions.
  FileTextDocumentListObserver observer;
  dlist.addObserver(&observer);

  FileTextDocument *file1 = add(dlist, "a/b");
  xassert(file1->title == "b");

  FileTextDocument *file2 = add(dlist, "b:2");
  xassert(file2->title == "b:2");

  FileTextDocument *file3 = add(dlist, "b");
  xassert(file3->title == "b:3");

  dlist.removeFile(file3);
  delete file3;

  dlist.moveFile(file2, 0);

  file2->filename = "zoo";
  dlist.assignUniqueTitle(file2);
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

  xassert(FileTextDocument::s_objectCount == 0);

  cout << "test-file-td-list passed\n";
}

USUAL_MAIN


// EOF
