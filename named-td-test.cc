// named-td-test.cc
// Tests for 'named-td' module.

#include "unit-tests.h"                          // decl for my entry point

#include "named-td.h"                            // module to test

#include "td-diagnostics.h"                      // Diagnostic, TextDocumentDiagnostics

// smbase
#include "smbase/nonport.h"                      // fileOrDirectoryExists, removeFile
#include "smbase/sm-file-util.h"                 // SMFileUtil
#include "smbase/sm-macros.h"                    // OPEN_ANONYMOUS_NAMESPACE
#include "smbase/sm-noexcept.h"                  // NOEXCEPT
#include "smbase/sm-override.h"                  // OVERRIDE
#include "smbase/sm-test.h"                      // EXPECT_EQ

#include <fstream>                               // std::ofstream

using namespace gdv;

using std::ofstream;


OPEN_ANONYMOUS_NAMESPACE


void testWhenUntitledExists()
{
  NamedTextDocument file;
  file.setDocumentName(DocumentName::fromNonFileResourceName(
    HostName::asLocal(), "untitled.txt",
    SMFileUtil().currentDirectory()));

  // Create a file with that name.
  bool created = false;
  if (!fileOrDirectoryExists(file.resourceName().c_str())) {
    ofstream of(file.resourceName().c_str());
    created = true;
  }

  if (created) {
    (void)removeFile(file.resourceName().c_str());
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
void writeFile(NamedTextDocument &doc)
{
  xassert(doc.hasFilename());
  string fname = doc.filename();

  SMFileUtil sfu;
  std::vector<unsigned char> bytes(doc.getWholeFile());

  sfu.writeFile(fname, bytes);

  doc.noUnsavedChanges();

  (void)getFileModificationTime(fname.c_str(), doc.m_lastFileTimestamp);
}


// Make sure we can handle using 'undo' to go backward past the point
// in history corresponding to file contents, then make a change.
void testUndoPastSavePoint()
{
  NamedTextDocument doc;
  doc.setDocumentName(DocumentName::fromFilename(
    HostName::asLocal(), "tmp.h"));

  doc.appendString("x");
  doc.appendString("x");
  xassert(doc.unsavedChanges());
  writeFile(doc);
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


void testApplyCommandSubstitutions()
{
  NamedTextDocument doc;

  // Initially it has no file name.
  EXPECT_EQ(doc.applyCommandSubstitutions("$f"),
    "''");

  doc.setDocumentName(DocumentName::fromFilename(
    HostName::asLocal(), "tmp.h"));
  EXPECT_EQ(doc.applyCommandSubstitutions("$f"),
    "tmp.h");
  EXPECT_EQ(doc.applyCommandSubstitutions("abc $f def $f hij"),
    "abc tmp.h def tmp.h hij");

  // This isn't necessariliy ideal, but it is the current behavior.
  EXPECT_EQ(doc.applyCommandSubstitutions("$$f"),
    "$tmp.h");

  doc.setDocumentName(DocumentName::fromFilename(
    HostName::asLocal(), "d1/d2/foo.txt"));
  EXPECT_EQ(doc.applyCommandSubstitutions("$f"),
    "foo.txt");
}


CLOSE_ANONYMOUS_NAMESPACE


// Called from unit-tests.cc.
void test_named_td(CmdlineArgsSpan args)
{
  testWhenUntitledExists();
  testReadFile();
  testSetDocumentProcessStatus();
  testUndoPastSavePoint();
  testApplyCommandSubstitutions();

  xassert(NamedTextDocument::s_objectCount == 0);
  xassert(TextDocument::s_objectCount == 0);
}


// EOF
