// named-td-test.cc
// Tests for 'named-td' module.

#include "smbase/gdvalue-optional-fwd.h"         // gdv::GDValue(std::optional)

#include "named-td.h"                            // module to test

#include "td-diagnostics.h"                      // Diagnostic, TextDocumentDiagnostics

// smbase
#include "smbase/gdvalue.h"                      // gdv::GDValue
#include "smbase/gdvalue-optional.h"             // gdv::GDValue(std::optional)
#include "smbase/nonport.h"                      // fileOrDirectoryExists, removeFile
#include "smbase/ordered-map-ops.h"              // smbase::OrderedMap (for TEST_CASE_EXPRS)
#include "smbase/sm-file-util.h"                 // SMFileUtil
#include "smbase/sm-noexcept.h"                  // NOEXCEPT
#include "smbase/sm-override.h"                  // OVERRIDE
#include "smbase/sm-test.h"                      // USUAL_MAIN, TEST_CASE_EXPRS

#include <fstream>                               // ofstream

using namespace gdv;

using std::ofstream;


// TODO: Use anonymous namespace.


static void testWhenUntitledExists()
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
static void readFile(NamedTextDocument &doc)
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
static void testReadFile()
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


static void testSetDocumentProcessStatus()
{
  NamedTextDocument doc;

  // Check that setting to DPS_RUNNING sets read-only.
  EXPECT_EQ(doc.isReadOnly(), false);
  doc.setDocumentProcessStatus(DPS_RUNNING);
  EXPECT_EQ(doc.isReadOnly(), true);
}


// Write 'doc' to its file name.  This approximates what the editor app
// does when writing a file.
static void writeFile(NamedTextDocument &doc)
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
static void testUndoPastSavePoint()
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


static void testApplyCommandSubstitutions()
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


// ---------------------- TextDocumentDiagnostics ----------------------
// These tests are for the `td-diagnostics` module, but my test scheme
// requires it to be tested here due to the dependency structure.
//
// TODO: Rework how tests are done in this repo.


void testOneContainsByteIndex(
  std::optional<int> startByteIndex,
  std::optional<int> endByteIndex,
  int testByteIndex,
  bool expect)
{
  EXN_CONTEXT_EXPR(toGDValue(startByteIndex));
  EXN_CONTEXT_EXPR(toGDValue(endByteIndex));
  EXN_CONTEXT_EXPR(testByteIndex);

  Diagnostic dummy("");
  TextDocumentDiagnostics::LineEntry entry(
    startByteIndex, endByteIndex, &dummy);

  EXPECT_EQ(entry.containsByteIndex(testByteIndex), expect);
}


static void test_TDD_LineEntry_containsByteIndex()
{
  testOneContainsByteIndex({}, {}, 0, true);
  testOneContainsByteIndex({}, {}, 1, true);
  testOneContainsByteIndex({}, {}, 999, true);

  testOneContainsByteIndex(5, {}, 1, false);
  testOneContainsByteIndex(5, {}, 4, false);
  testOneContainsByteIndex(5, {}, 5, true);
  testOneContainsByteIndex(5, {}, 6, true);

  testOneContainsByteIndex({}, 10, 5, true);
  testOneContainsByteIndex({}, 10, 9, true);
  testOneContainsByteIndex({}, 10, 10, false);
  testOneContainsByteIndex({}, 10, 11, false);

  testOneContainsByteIndex(5, 10, 4, false);
  testOneContainsByteIndex(5, 10, 5, true);
  testOneContainsByteIndex(5, 10, 9, true);
  testOneContainsByteIndex(5, 10, 10, false);

  // Special case for collapsed ranges.
  testOneContainsByteIndex(7, 7, 6, false);
  testOneContainsByteIndex(7, 7, 7, true);
  testOneContainsByteIndex(7, 7, 8, false);
}


static void testOneGetDiagnosticsAt(
  TextDocumentDiagnostics const &tdd,
  int line,
  int byteIndex,
  char const *expect)
{
  EXN_CONTEXT_EXPR(line);
  EXN_CONTEXT_EXPR(byteIndex);

  RCSerf<Diagnostic const> actual =
    tdd.getDiagnosticAt(TextMCoord(line, byteIndex));

  EXPECT_EQ(actual==nullptr, expect==nullptr);

  if (expect) {
    EXPECT_EQ(actual->m_message, expect);
  }
}


static void testOneAdjacentDiagnostic(
  TextDocumentDiagnostics const &tdd,
  bool next,
  int startLine,
  int startByteIndex,
  int expectLine,
  int expectByteIndex)
{
  TEST_CASE_EXPRS("testOneAdjacentDiagnostic",
    next, startLine, startByteIndex);

  std::optional<TextMCoord> actual = tdd.getAdjacentDiagnosticLocation(
    next, TextMCoord(startLine, startByteIndex));

  if (expectLine == -1) {
    xassert(!actual.has_value());
  }
  else {
    xassert(actual.has_value());

    EXPECT_EQ(actual->m_line, expectLine);
    EXPECT_EQ(actual->m_byteIndex, expectByteIndex);
  }
}


static void testOneNextDiagnostic(
  TextDocumentDiagnostics const &tdd,
  int startLine,
  int startByteIndex,
  int expectLine,
  int expectByteIndex)
{
  testOneAdjacentDiagnostic(tdd, true /*next*/,
    startLine, startByteIndex,
    expectLine, expectByteIndex);
}


static void testOnePreviousDiagnostic(
  TextDocumentDiagnostics const &tdd,
  int startLine,
  int startByteIndex,
  int expectLine,
  int expectByteIndex)
{
  testOneAdjacentDiagnostic(tdd, false /*next*/,
    startLine, startByteIndex,
    expectLine, expectByteIndex);
}


// This also tests next/previous diagnostic navigation.
static void test_TDD_getDiagnosticAt()
{
  TEST_CASE("test_TDD_getDiagnosticAt");

  NamedTextDocument doc;

  //                          1
  //                01234567890123456789
  doc.appendString("   [1]     [2 2]    \n");  // 0
  doc.appendString("                    \n");  // 1
  doc.appendString("  [3 [4 [5] 4] 3]   \n");  // 2
  doc.appendString("  [6   [7   6]  7]  \n");  // 3
  doc.appendString("                    \n");  // 4
                           // ^ 9
  doc.appendString("      [10  [11]     \n");  // 5
  doc.appendString("   [12]        10]  \n");  // 6
  doc.appendString("   [13  13]  14]    \n");  // 7
  doc.appendString("   [15  [16  16]    \n");  // 8
  EXPECT_EQ(doc.numLines(), 10);  // The final line-without-NL counts.
  doc.selfCheck();

  TextDocumentDiagnostics tdd(1 /*version*/, &doc);
  tdd.selfCheck();
  EXPECT_EQ(tdd.maxDiagnosticLine(), -1);
  tdd.insert({{0,3}, {0,6}}, Diagnostic("1"));
  tdd.selfCheck();
  tdd.insert({{0,11}, {0,16}}, Diagnostic("2"));
  tdd.selfCheck();
  tdd.insert({{2,2}, {2,17}}, Diagnostic("3"));
  tdd.selfCheck();
  tdd.insert({{2,5}, {2,14}}, Diagnostic("4"));
  tdd.selfCheck();
  tdd.insert({{2,8}, {2,11}}, Diagnostic("5"));
  tdd.insert({{3,2}, {3,14}}, Diagnostic("6"));
  tdd.insert({{3,7}, {3,18}}, Diagnostic("7"));
  EXPECT_EQ(tdd.maxDiagnosticLine(), 3);
  // I skipped "8" I guess.
  tdd.insert({{4,10}, {4,10}}, Diagnostic("9"));
  tdd.insert({{5,6}, {6,18}}, Diagnostic("10"));
  EXPECT_EQ(tdd.maxDiagnosticLine(), 6);
  tdd.insert({{5,11}, {5,15}}, Diagnostic("11"));
  tdd.insert({{6,3}, {6,7}}, Diagnostic("12"));
  tdd.insert({{7,3}, {7,11}}, Diagnostic("13"));
  tdd.insert({{7,3}, {7,16}}, Diagnostic("14"));
  tdd.insert({{8,3}, {8,16}}, Diagnostic("15"));
  tdd.insert({{8,8}, {8,16}}, Diagnostic("16"));
  EXPECT_EQ(tdd.maxDiagnosticLine(), 8);
  tdd.selfCheck();

  testOneGetDiagnosticsAt(tdd, 0,0, nullptr);
  testOneGetDiagnosticsAt(tdd, 0,2, nullptr);
  testOneGetDiagnosticsAt(tdd, 0,3, "1");
  testOneGetDiagnosticsAt(tdd, 0,4, "1");
  testOneGetDiagnosticsAt(tdd, 0,5, "1");
  testOneGetDiagnosticsAt(tdd, 0,6, nullptr);
  testOneGetDiagnosticsAt(tdd, 0,10, nullptr);
  testOneGetDiagnosticsAt(tdd, 0,11, "2");
  testOneGetDiagnosticsAt(tdd, 0,15, "2");
  testOneGetDiagnosticsAt(tdd, 0,16, nullptr);

  testOneGetDiagnosticsAt(tdd, 1,3, nullptr);

  testOneGetDiagnosticsAt(tdd, 2,1, nullptr);
  testOneGetDiagnosticsAt(tdd, 2,2, "3");
  testOneGetDiagnosticsAt(tdd, 2,4, "3");
  testOneGetDiagnosticsAt(tdd, 2,5, "4");
  testOneGetDiagnosticsAt(tdd, 2,7, "4");
  testOneGetDiagnosticsAt(tdd, 2,8, "5");
  testOneGetDiagnosticsAt(tdd, 2,10, "5");
  testOneGetDiagnosticsAt(tdd, 2,11, "4");
  testOneGetDiagnosticsAt(tdd, 2,13, "4");
  testOneGetDiagnosticsAt(tdd, 2,14, "3");
  testOneGetDiagnosticsAt(tdd, 2,16, "3");
  testOneGetDiagnosticsAt(tdd, 2,17, nullptr);

  testOneGetDiagnosticsAt(tdd, 3,1, nullptr);
  testOneGetDiagnosticsAt(tdd, 3,2, "6");
  testOneGetDiagnosticsAt(tdd, 3,6, "6");
  testOneGetDiagnosticsAt(tdd, 3,7, "7");
  testOneGetDiagnosticsAt(tdd, 3,13, "7");
  testOneGetDiagnosticsAt(tdd, 3,14, "7");
  testOneGetDiagnosticsAt(tdd, 3,17, "7");
  testOneGetDiagnosticsAt(tdd, 3,18, nullptr);

  testOneGetDiagnosticsAt(tdd, 4,9, nullptr);
  testOneGetDiagnosticsAt(tdd, 4,10, "9");
  testOneGetDiagnosticsAt(tdd, 4,11, nullptr);

  testOneGetDiagnosticsAt(tdd, 5,5, nullptr);
  testOneGetDiagnosticsAt(tdd, 5,6, "10");
  testOneGetDiagnosticsAt(tdd, 5,10, "10");
  testOneGetDiagnosticsAt(tdd, 5,11, "11");
  testOneGetDiagnosticsAt(tdd, 5,14, "11");
  testOneGetDiagnosticsAt(tdd, 5,15, "10");
  testOneGetDiagnosticsAt(tdd, 6,2, "10");
  testOneGetDiagnosticsAt(tdd, 6,3, "12");
  testOneGetDiagnosticsAt(tdd, 6,6, "12");
  testOneGetDiagnosticsAt(tdd, 6,7, "10");
  testOneGetDiagnosticsAt(tdd, 6,17, "10");
  testOneGetDiagnosticsAt(tdd, 6,18, nullptr);

  testOneGetDiagnosticsAt(tdd, 7,2, nullptr);
  testOneGetDiagnosticsAt(tdd, 7,3, "13");
  testOneGetDiagnosticsAt(tdd, 7,10, "13");
  testOneGetDiagnosticsAt(tdd, 7,11, "14");
  testOneGetDiagnosticsAt(tdd, 7,15, "14");
  testOneGetDiagnosticsAt(tdd, 7,16, nullptr);

  testOneGetDiagnosticsAt(tdd, 8,2, nullptr);
  testOneGetDiagnosticsAt(tdd, 8,3, "15");
  testOneGetDiagnosticsAt(tdd, 8,7, "15");
  testOneGetDiagnosticsAt(tdd, 8,8, "16");
  testOneGetDiagnosticsAt(tdd, 8,15, "16");
  testOneGetDiagnosticsAt(tdd, 8,16, nullptr);


  testOneNextDiagnostic(tdd, 0,0, 0,3);
  testOneNextDiagnostic(tdd, 0,1, 0,3);
  testOneNextDiagnostic(tdd, 0,2, 0,3);

  testOneNextDiagnostic(tdd, 0,3, 0,11);
  testOneNextDiagnostic(tdd, 0,4, 0,11);
  testOneNextDiagnostic(tdd, 0,10, 0,11);

  testOneNextDiagnostic(tdd, 0,11, 2,2);
  testOneNextDiagnostic(tdd, 1,0, 2,2);
  testOneNextDiagnostic(tdd, 2,0, 2,2);
  testOneNextDiagnostic(tdd, 2,1, 2,2);

  testOneNextDiagnostic(tdd, 2,2, 2,5);

  testOneNextDiagnostic(tdd, 2,5, 2,8);

  testOneNextDiagnostic(tdd, 2,8, 3,2);

  testOneNextDiagnostic(tdd, 3,2, 3,7);

  testOneNextDiagnostic(tdd, 3,7, 4,10);

  testOneNextDiagnostic(tdd, 4,10, 5,6);

  testOneNextDiagnostic(tdd, 5,6, 5,11);

  testOneNextDiagnostic(tdd, 5,11, 6,3);

  testOneNextDiagnostic(tdd, 6,3, 7,3);

  testOneNextDiagnostic(tdd, 7,3, 8,3);

  testOneNextDiagnostic(tdd, 8,3, 8,8);

  testOneNextDiagnostic(tdd, 8,8, -1,0);


  testOnePreviousDiagnostic(tdd, 0,0, -1,0);

  testOnePreviousDiagnostic(tdd, 8,20, 8,8);

  testOnePreviousDiagnostic(tdd, 5,2, 4,10);

  // This notification should clear the diagnostics.
  tdd.observeTotalChange(doc.getCore());
  EXPECT_EQ(tdd.empty(), true);
  EXPECT_EQ(tdd.size(), 0);
  EXPECT_EQ(tdd.maxDiagnosticLine(), -1);
  tdd.selfCheck();
  doc.selfCheck();
}


// ------------------------------- entry -------------------------------
// Defined in doc-type-detect.cc.
void test_doc_type_detect();

static void entry()
{
  testWhenUntitledExists();
  testReadFile();
  testSetDocumentProcessStatus();
  testUndoPastSavePoint();
  testApplyCommandSubstitutions();
  test_TDD_LineEntry_containsByteIndex();
  test_TDD_getDiagnosticAt();

  // Put this here for the moment.
  //
  // TODO: Fix the test infrastructure so each test is not in its own
  // executable.
  test_doc_type_detect();

  xassert(NamedTextDocument::s_objectCount == 0);
  xassert(TextDocument::s_objectCount == 0);

  cout << "named-td-test passed\n";
}


USUAL_TEST_MAIN


// EOF
