// named-td-list-test.cc
// Tests for 'named-td-list' module.

#include "unit-tests.h"                // decl for my entry point

#include "named-td-list.h"             // module to test

#include "smbase/sm-file-util.h"       // SMFileUtil
#include "smbase/sm-noexcept.h"        // NOEXCEPT
#include "smbase/sm-override.h"        // OVERRIDE
#include "smbase/sm-test.h"            // EXPECT_EQ
#include "smbase/strutil.h"            // dirname

#include <cstddef>                     // std::size_t
#include <vector>                      // std::vector

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
    NamedTextDocumentList const *documentList, NamedTextDocument *file) NOEXCEPT OVERRIDE
  {
    xassert(documentList == m_documentList);
    m_pendingNotifications.append(new Notification(NF_ADDED, file));
  }

  void namedTextDocumentRemoved(
    NamedTextDocumentList const *documentList, NamedTextDocument *file) NOEXCEPT OVERRIDE
  {
    xassert(documentList == m_documentList);
    m_pendingNotifications.append(new Notification(NF_REMOVED, file));
  }

  void namedTextDocumentAttributeChanged(
    NamedTextDocumentList const *documentList, NamedTextDocument *file) NOEXCEPT OVERRIDE
  {
    xassert(documentList == m_documentList);
    m_pendingNotifications.append(new Notification(NF_ATTRIBUTE, file));
  }

  void namedTextDocumentListOrderChanged(
    NamedTextDocumentList const *documentList) NOEXCEPT OVERRIDE
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
static NamedTextDocument *add(NamedTextDocumentList &dlist,
                              DocumentName const &docName)
{
  NamedTextDocument *file = new NamedTextDocument;
  file->setDocumentName(docName);
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

  HostName hostName(HostName::asLocal());
  DocumentName docName1;
  docName1.setFilename(hostName, "file1");

  NamedTextDocument *file1 = add(dlist, docName1);
  xassert(file1->m_title == "file1");
  xassert(dlist.findDocumentByName(docName1) == file1);
  xassert(dlist.findDocumentByTitle("file1") == file1);

  observer.expectOnly(NF_ADDED, file1);

  DocumentName docName2;
  docName2.setFilename(hostName, "a/file2");

  NamedTextDocument *file2 = add(dlist, docName2);
  xassert(file2->m_title == "file2");
  xassert(dlist.findDocumentByName(docName2) == file2);
  xassert(dlist.findDocumentByTitle("file2") == file2);

  observer.expectOnly(NF_ADDED, file2);

  // Title uniqueness has to include a directory component.
  DocumentName docName3;
  docName3.setFilename(hostName, "b/file2");
  NamedTextDocument *file3 = add(dlist, docName3);
  xassert(file3->m_title == "b/file2");

  observer.expectOnly(NF_ADDED, file3);

  // Title uniqueness has to append a digit.
  DocumentName docName2b;
  docName2b.setFilename(hostName, "file2");
  NamedTextDocument *file4 = add(dlist, docName2b);
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
  xassert(!dlist.hasUnsavedFiles());

  NamedTextDocument *file0 = dlist.getDocumentAt(0);

  NamedTextDocument *file1 = createUntitled(dlist);
  observer.expectOnly(NF_ADDED, file1);
  xassert(file1->resourceName() == "untitled2.txt");

  NamedTextDocument *file2 = createUntitled(dlist);
  observer.expectOnly(NF_ADDED, file2);
  xassert(file2->resourceName() == "untitled3.txt");

  // Test 'findUntitledUnmodifiedFile'.
  NamedTextDocument *f = dlist.findUntitledUnmodifiedDocument();
  xassert(f != NULL);
  xassert(!dlist.hasUnsavedFiles());

  file1->insertAt(TextMCoord(LineIndex(0),ByteIndex(0)), "hi", ByteCount(2));
  f = dlist.findUntitledUnmodifiedDocument();
  xassert(f == file0 || f == file2);
  xassert(dlist.hasUnsavedFiles());

  // Make it no longer untitled.
  file2->setDocumentName(DocumentName::fromFilename(
    HostName::asLocal(), file2->resourceName()));
  f = dlist.findUntitledUnmodifiedDocument();
  xassert(f == file0);

  file0->insertAt(TextMCoord(LineIndex(0),ByteIndex(0)), "\n", ByteCount(1));
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
  file0->setDocumentName(DocumentName::fromFilename(
    HostName::asLocal(), "a/some-name.txt"));
  dlist.assignUniqueTitle(file0);
  observer.expectOnly(NF_ATTRIBUTE, file0);
  xassert(file0->m_title == "some-name.txt");

  dlist.removeObserver(&observer);
}


// Provoke a name like "a:3".
static void testColon3()
{
  NamedTextDocumentList dlist;

  // Also exercise the no-op observer functions.
  NamedTextDocumentListObserver observer;
  dlist.addObserver(&observer);

  NamedTextDocument *file1 = add(dlist,
    DocumentName::fromFilename(HostName::asLocal(), "a/b"));
  xassert(file1->m_title == "b");

  NamedTextDocument *file2 = add(dlist,
    DocumentName::fromFilename(HostName::asLocal(), "b:2"));
  xassert(file2->m_title == "b:2");

  NamedTextDocument *file3 = add(dlist,
    DocumentName::fromFilename(HostName::asLocal(), "b"));
  xassert(file3->m_title == "b:3");

  dlist.removeDocument(file3);
  delete file3;

  dlist.moveDocument(file2, 0);

  file2->setDocumentName(DocumentName::fromFilename(
    HostName::asLocal(), "zoo"));
  dlist.assignUniqueTitle(file2);

  dlist.removeObserver(&observer);
}


// Expect output of 'getUniqueDirectories' to match the NULL-terminated
// sequence of arguments.
static void expectDirs(NamedTextDocumentList &dlist, char const *dir0, ...)
{
  std::vector<HostAndResourceName> actual;
  dlist.getUniqueDirectories(actual);

  if (!dir0) {
    xassert(actual.empty());
    return;
  }
  xassert(!actual.empty());

  EXPECT_EQ(actual[0].resourceName(), dir0);
  std::size_t i = 1;

  va_list args;
  va_start(args, dir0);
  while (char const *dir = va_arg(args, char const *)) {
    EXPECT_EQ(actual[i].resourceName(), dir);
    i++;
  }
  va_end(args);

  xassert(actual.size() == i);
}


static void testGetUniqueDirectories()
{
  NamedTextDocumentList dlist;
  expectDirs(dlist, NULL);

  add(dlist, DocumentName::fromFilename(HostName::asLocal(), "/a/b"));
  expectDirs(dlist, "/a/", NULL);

  // Check that existing entries are preserved.
  {
    std::vector<HostAndResourceName> actual;
    actual.push_back(HostAndResourceName::localFile("existing/"));
    dlist.getUniqueDirectories(actual);
    xassert(actual.size() == 2);
    xassert(actual[0].resourceName() == "existing/");
    xassert(actual[1].resourceName() == "/a/");
  }

  // Check that existing entries are not duplicated.
  {
    std::vector<HostAndResourceName> actual;
    actual.push_back(HostAndResourceName::localFile("/a/"));
    dlist.getUniqueDirectories(actual);
    xassert(actual.size() == 1);
    xassert(actual[0].resourceName() == "/a/");
  }

  add(dlist, DocumentName::fromFilename(HostName::asLocal(), "/a/c"));
  expectDirs(dlist, "/a/", NULL);

  add(dlist, DocumentName::fromFilename(HostName::asLocal(), "/b/c"));
  expectDirs(dlist, "/a/", "/b/", NULL);

  add(dlist, DocumentName::fromFilename(HostName::asLocal(), "/b/d/e/f/g"));
  expectDirs(dlist, "/a/", "/b/", "/b/d/e/f/", NULL);
}


// Called from unit-tests.cc.
void test_named_td_list(CmdlineArgsSpan args)
{
  testSimple();
  testAddMoveRemove();
  testCreateUntitled();
  testSaveAs();
  testColon3();
  testGetUniqueDirectories();

  xassert(NamedTextDocument::s_objectCount == 0);
  xassert(TextDocument::s_objectCount == 0);
}


// EOF
