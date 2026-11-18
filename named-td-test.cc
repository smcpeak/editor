// named-td-test.cc
// Tests for 'named-td' module.

#include "unit-tests.h"                          // decl for my entry point

#include "named-td.h"                            // module to test

// smbase
#include "smbase/gdvalue.h"                      // gdv::GDValue
#include "smbase/nonport.h"                      // getFileModificationTime
#include "smbase/sm-file-util.h"                 // SMFileUtil
#include "smbase/sm-macros.h"                    // OPEN_ANONYMOUS_NAMESPACE
#include "smbase/sm-noexcept.h"                  // NOEXCEPT
#include "smbase/sm-override.h"                  // OVERRIDE
#include "smbase/sm-test.h"                      // EXPECT_EQ[_GDV]

#include <fstream>                               // std::ofstream

using namespace gdv;

using std::ofstream;


OPEN_ANONYMOUS_NAMESPACE


void testWhenUntitledExists()
{
  SMFileUtil sfu;

  NamedTextDocument file;
  file.setDocumentName(DocumentName::fromNonFileResourceName(
    HostName::asLocal(), "untitled.txt",
    sfu.currentDirectory()));

  // Create a file with that name.
  bool created = false;
  if (!sfu.pathExists(file.resourceName())) {
    ofstream of(file.resourceName().c_str());
    created = true;
  }

  if (created) {
    sfu.removeFile(file.resourceName());
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


// Replace the contents of 'doc' with what is on disk.
//
// This approximates what the editor does to read a file.
void readFile(NamedTextDocument &doc)
{
  xassert(doc.hasFilename());
  string fname = doc.filename();

  SMFileUtil sfu;

  std::vector<unsigned char> bytes(sfu.readFile(fname));

  int64_t modTime;
  (void)getFileModificationTime(fname.c_str(), modTime);

  bool readOnly = sfu.isReadOnly(fname);

  // This is what `EditorWindow::openOrSwitchToFile` and
  // `EditorGlobal::reloadDocumentFile` do after reading a file.
  doc.replaceFileAndStats(bytes, modTime, readOnly);
}


// Make sure that reading a file broadcasts 'observeTotalChange'.
void testReadFile()
{
  NamedTextDocument file;
  file.setDocumentName(DocumentName::fromFilename(
    HostName::asLocal(), "td.h"));
  readFile(file);

  TestTDO ttdo;
  file.addObserver(&ttdo);
  readFile(file);
  file.removeObserver(&ttdo);

  xassert(ttdo.m_totalChanges == 1);
}


void testSetDocumentProcessStatus()
{
  NamedTextDocument doc;

  // Check that setting to DPS_RUNNING sets read-only.
  EXPECT_EQ(doc.isReadOnly(), false);
  doc.setDocumentProcessStatus(DPS_RUNNING);
  EXPECT_EQ(doc.isReadOnly(), true);
}


// Write 'doc' to its file name.  This approximates what the editor app
// does when writing a file.
//
// TODO: Factor the actual file reading and writing code so it can be
// tested here.
void writeFile(NamedTextDocument &doc)
{
  xassert(doc.hasFilename());
  string fname = doc.filename();

  SMFileUtil sfu;
  std::vector<unsigned char> bytes(doc.getWholeFile());

  sfu.writeFile(fname, bytes);

  // This is what `EditorWindow::writeTheFile` does after a successful
  // file write.
  (void)getFileModificationTime(fname.c_str(), doc.m_lastFileTimestamp);
  doc.m_modifiedOnDisk = false;
  doc.noUnsavedChanges();
}


// Make sure we can handle using 'undo' to go backward past the point
// in history corresponding to file contents, then make a change.
void testUndoPastSavePoint()
{
  char const * const fname = "out/tmp.h";

  SMFileUtil sfu;
  sfu.createParentDirectories(fname);

  NamedTextDocument doc;
  doc.setDocumentName(DocumentName::fromFilename(
    HostName::asLocal(), fname));

  doc.appendString("x");
  doc.appendString("x");
  xassert(doc.unsavedChanges());
  writeFile(doc);
  xassert(!doc.unsavedChanges());
  xassert(!doc.m_modifiedOnDisk);
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

  sfu.removeFile(fname);
}


// Check `doc.checkEditSafety()` against `expect`.
#define EXPECT_ESP(doc, expect) \
  EXPECT_EQ_GDVSER((doc).checkEditSafety(), expect)


void testCheckEditSafety()
{
  NamedTextDocument doc;

  // Initially, there are no problems.
  EXPECT_ESP(doc, ESP_NONE);

  // Read-only is a problem.
  doc.setReadOnly(true);
  EXPECT_ESP(doc, ESP_READ_ONLY);

  // Read-only takes precedence over modified-on-disk.
  doc.m_modifiedOnDisk = true;
  EXPECT_ESP(doc, ESP_READ_ONLY);

  // Clearing read-only exposes the modified-on-disk problem.
  doc.setReadOnly(false);
  EXPECT_ESP(doc, ESP_MODIFIED_ON_DISK);

  // Once there are unsaved changes, modified-on-disk is not reported,
  // on the assumption that the warning was already dismissed.
  doc.appendString("x");
  xassert(doc.unsavedChanges());
  EXPECT_ESP(doc, ESP_NONE);

  // But read-only is still reported even with unsaved changes.
  doc.setReadOnly(true);
  EXPECT_ESP(doc, ESP_READ_ONLY);
  doc.setReadOnly(false);
  EXPECT_ESP(doc, ESP_NONE);

  // Undoing back to the save point brings back the modified-on-disk
  // problem.
  doc.undo();
  xassert(!doc.unsavedChanges());
  EXPECT_ESP(doc, ESP_MODIFIED_ON_DISK);

  // Reloading the file contents clears modified-on-disk.
  doc.replaceFileAndStats(std::vector<unsigned char>{'a', '\n'},
                          0 /*modTime*/, false /*readOnly*/);
  EXPECT_ESP(doc, ESP_NONE);

  // Reloading as read-only yields a read-only problem.
  doc.replaceFileAndStats(std::vector<unsigned char>{'b', '\n'},
                          0 /*modTime*/, true /*readOnly*/);
  EXPECT_ESP(doc, ESP_READ_ONLY);
}


#undef EXPECT_ESP


void test_toGDValue()
{
  NamedTextDocument doc;

  // This mostly just confirms that all the elements are present.  It is
  // a very denegerate case.
  EXPECT_EQ_GDV(doc, fromGDVN(R"(
    NamedTextDocument[
      core: TextDocumentCore[version:1 lines:[""]]
      historyIndex: 0
      savedHistoryIndex: 0
      documentProcessStatus: DPS_NONE
      readOnly: false
      documentName: DocumentName[
        hostName: HostName[sshHostName:""]
        resourceName: ""
        hasFilename: false
        directory: ""
      ]
      diagnostics: null
      hasTddUpdater: false
      observationRecorder: {:}
      lastFileTimestamp: 0
      modifiedOnDisk: false
      title: ""
      highlighter: null
      highlightTrailingWhitespace: true
      lspUpdateContinuously: true
    ]
  )"));

  // Quick check of EditSafetyProblem stringification.
  EXPECT_EQ_GDV(ESP_MODIFIED_ON_DISK, "ESP_MODIFIED_ON_DISK"_sym);
}


CLOSE_ANONYMOUS_NAMESPACE


// Called from unit-tests.cc.
void test_named_td(CmdlineArgsSpan args)
{
  testWhenUntitledExists();
  testReadFile();
  testSetDocumentProcessStatus();
  testUndoPastSavePoint();
  testCheckEditSafety();
  test_toGDValue();

  xassert(NamedTextDocument::s_objectCount == 0);
  xassert(TextDocument::s_objectCount == 0);
}


// EOF
