// test-named-td.cc
// Tests for 'named-td' module.

#include "named-td.h"                  // module to test

// smbase
#include "nonport.h"                   // fileOrDirectoryExists, removeFile
#include "sm-file-util.h"              // SMFileUtil
#include "sm-noexcept.h"               // NOEXCEPT
#include "sm-override.h"               // OVERRIDE
#include "sm-test.h"                   // USUAL_MAIN

#include <fstream>                     // ofstream

using std::ofstream;


static void testWhenUntitledExists()
{
  NamedTextDocument file;
  file.setNonFileName("untitled.txt", SMFileUtil().currentDirectory());

  // Create a file with that name.
  bool created = false;
  if (!fileOrDirectoryExists(file.name().c_str())){
    ofstream of(file.name().c_str());
    created = true;
  }

  // The document should still regard itself as not having a stale
  // modification time because it is untitled, hence not really
  // associated with any on-disk file.
  xassert(!file.hasStaleModificationTime());

  if (created) {
    (void)removeFile(file.name().c_str());
  }
}


class TestTDO : public TextDocumentObserver {
public:      // data
  // Number of calls to 'observeTotalChange;
  int m_totalChanges;

public:      // funcs
  TestTDO()
    : TextDocumentObserver(),
      m_totalChanges(0)
  {}

  virtual void observeTotalChange(TextDocumentCore const &doc) NOEXCEPT OVERRIDE
  {
    m_totalChanges++;
  }
};


// Make sure that reading a file broadcasts 'observeTotalChange'.
static void testReadFile()
{
  NamedTextDocument file;
  file.setFilename("td.h");
  file.readFile();

  TestTDO ttdo;
  file.addObserver(&ttdo);
  file.readFile();
  file.removeObserver(&ttdo);

  xassert(ttdo.m_totalChanges == 1);
}


static void testSetDocumentProcessStatus()
{
  NamedTextDocument doc;

  // Check that setting to DPS_RUNNING sets read-only.
  EXPECT_EQ(doc.isReadOnly(), false);
  doc.setDocumentProcessStatus(DPS_RUNNING);
  EXPECT_EQ(doc.isReadOnly(), true);
}


// Make sure we can handle using 'undo' to go backward past the point
// in history corresponding to file contents, then make a change.
static void testUndoPastSavePoint()
{
  NamedTextDocument doc;
  doc.setFilename("tmp.h");

  doc.appendString("x");
  doc.appendString("x");
  xassert(doc.unsavedChanges());
  doc.writeFile();
  xassert(!doc.unsavedChanges());
  doc.selfCheck();

  // Now, the saved history point is 2 (after those two edits).

  doc.undo();
  doc.undo();
  xassert(doc.unsavedChanges());
  doc.selfCheck();

  // Current history point is 0.

  doc.appendString("y");
  xassert(doc.unsavedChanges());
  doc.selfCheck();

  // Current history point is 1, and saved history should be reset to -1.

  doc.appendString("y");
  xassert(doc.unsavedChanges());
  doc.selfCheck();

  // Current history point is 2.

  (void)removeFile("tmp.h");
}


static void entry()
{
  testWhenUntitledExists();
  testReadFile();
  testSetDocumentProcessStatus();
  testUndoPastSavePoint();

  xassert(NamedTextDocument::s_objectCount == 0);
  xassert(TextDocument::s_objectCount == 0);

  cout << "test-file-fd passed\n";

}

USUAL_TEST_MAIN


// EOF
