// lsp-get-code-lines-test.cc
// Tests for `lsp-get-code-lines` module.

#include "unit-tests.h"                          // decl for my entry point
#include "lsp-get-code-lines.h"                  // module under test

#include "host-file-line.h"                      // HostFileLine
#include "line-number.h"                         // LineNumber
#include "lsp-client.h"                          // LSPClientDocumentState
#include "vfs-test-connections.h"                // VFS_TestConnections

#include "smqtutil/sync-wait.h"                  // SynchronousWaiter

#include "smbase/chained-cond.h"                 // smbase::cc::z_le_lt
#include "smbase/either.h"                       // smbase::Either [h]
#include "smbase/gdvalue.h"                      // gdv::toGDValue [h]
#include "smbase/map-util.h"                     // smbase::{mapInsertUniqueMove, mapInsertUnique}
#include "smbase/portable-error-code.h"          // smbase::{PortableErrorCode, portableCodeDescription}
#include "smbase/sm-macros.h"                    // OPEN_ANONYMOUS_NAMESPACE, TABLESIZE
#include "smbase/sm-test.h"                      // EXPECT_EQ
#include "smbase/sm-trace.h"                     // INIT_TRACE, etc.
#include "smbase/xassert.h"                      // xassert

#include <exception>                             // std::uncaught_exceptions
#include <optional>                              // std::optional
#include <string>                                // std::string
#include <utility>                               // std::move
#include <vector>                                // std::vector

using namespace gdv;
using namespace smbase;


INIT_TRACE("lsp-get-code-lines-test");


OPEN_ANONYMOUS_NAMESPACE


// LSP client that just serves a document set.
class TestLSPClient : public LSPClientDocumentState {
public:      // methods
  // Arrange to serve `docInfo` in response to document queries.
  void addDoc(LSPDocumentInfo &&docInfo)
  {
    // If `docInfo` has pending diagnostics then it would have to be
    // added to `m_filesWithPendingDiagnostics` too.
    xassertPrecondition(!docInfo.hasPendingDiagnostics());

    // We need to make a copy of `m_fname` since we're also moving out
    // of `docInfo`.
    mapInsertUniqueMove(m_documentInfo,
      std::string(docInfo.m_fname), std::move(docInfo));
  }
};


// Class to encapsulate some common data.
class Tester {
public:      // types
  using FileReplyData = VFS_TestConnections::FileReplyData;

public:      // data
  std::string const languageId = "cpp";

  // File names.
  std::string const fname[2] = {
    "/home/user/file0.cc",
    "/home/user/file1.cc",
  };

  // File contents.
  char const * const fnameData[2] = {
    "one\n"
    "two\n"
    "three\n",

    "ONE\n"
    "TWO\n"
    "THREE\n",
  };

  // File names for files yielding errors from VFS.
  std::string const errFname[1] = {
    "/home/user/errfile0.cc",
  };

  // Corresponding error codes.
  PortableErrorCode errCode[1] = {
    PortableErrorCode::PEC_FILE_NOT_FOUND,
  };

  // Locations to look up.
  std::vector<HostFileLine> locations;

  TestSynchronousWaiter waiter;

  TestLSPClient lspClient;

  VFS_TestConnections vfsConnections;

public:      // methods
  Tester()
  {
    // Start willing to serve from the local machine.
    mapInsertUnique(vfsConnections.m_hosts,
      HostName::asLocal(), VFS_AbstractConnections::CS_READY);
  }

  ~Tester()
  {
    if (std::uncaught_exceptions() == 0) {
      lspClient.selfCheck();
      vfsConnections.selfCheck();
    }
    else {
      // The test is already failing for another reason.
    }
  }

  // ----------------------------- Helpers -----------------------------
  // Call the function under test using the populated data members.
  std::optional<std::vector<std::string>> callLspGetCodeLinesFunction()
  {
    return lspGetCodeLinesFunction(
      waiter,
      locations,
      lspClient,
      vfsConnections);
  }

  // The tests are written using line numbers, but the internal
  // interface uses line indices.
  LineIndex ln2li(int lineNumber)
  {
    return LineNumber(lineNumber).toLineIndex();
  }

  // Add to `locations` a request for `lineNumber` in `fileIndex`.
  void locAddFileLine(int fileIndex, int lineNumber)
  {
    xassert(cc::z_le_lt(fileIndex, TABLESIZE(fname)));
    locations.push_back(HostFileLine(
      HostAndResourceName::localFile(fname[fileIndex]),
      ln2li(lineNumber)
    ));
  }

  // Add to `locations` a request for `lineNumber` in `errFileIndex`.
  void locAddErrFileLine(int errFileIndex, int lineNumber)
  {
    xassert(cc::z_le_lt(errFileIndex, TABLESIZE(errFname)));
    locations.push_back(HostFileLine(
      HostAndResourceName::localFile(errFname[errFileIndex]),
      ln2li(lineNumber)
    ));
  }

  // Add `fileIndex` to LSP.
  void addFileToLSP(int fileIndex)
  {
    xassert(cc::z_le_lt(fileIndex, TABLESIZE(fname)));
    lspClient.addDoc(LSPDocumentInfo(
      fname[fileIndex],
      LSP_VersionNumber(1),
      fnameData[fileIndex]));
  }

  // Add `fileIndex` to VFS.
  void addFileToVFS(int fileIndex)
  {
    xassert(cc::z_le_lt(fileIndex, TABLESIZE(fname)));
    mapInsertUnique(vfsConnections.m_files,
      fname[fileIndex],
      FileReplyData(std::string(fnameData[fileIndex])));
  }

  // Add `errFileIndex` to VFS.
  void addErrFileToVFS(int errFileIndex)
  {
    xassert(cc::z_le_lt(errFileIndex, TABLESIZE(errFname)));
    mapInsertUnique(vfsConnections.m_files,
      errFname[errFileIndex],
      FileReplyData(errCode[errFileIndex]));
  }


  // ------------------------------ Tests ------------------------------
  // Simple example of "happy path" lookup of one location for which the
  // file is in the LSP client already (so no waiting occurs).
  void test_oneLSPLookup()
  {
    TEST_CASE("test_oneLSPLookup");

    locAddFileLine(0, 2);

    // Serve the data from the LSP client's copy.
    addFileToLSP(0);

    std::optional<std::vector<std::string>> linesOpt =
      callLspGetCodeLinesFunction();

    EXPECT_TRUE(linesOpt.has_value());
    EXPECT_EQ(linesOpt->size(), 1);
    EXPECT_EQ(linesOpt->at(0), "two");
    EXPECT_EQ(waiter.m_waitUntilCount, 0);
  }


  // As in the previous test, but get one file from VFS.
  void test_oneVFSLookup()
  {
    TEST_CASE("test_oneVFSLookup");

    locAddFileLine(0, 2);

    // Serve the data from VFS.
    addFileToVFS(0);

    std::optional<std::vector<std::string>> linesOpt =
      callLspGetCodeLinesFunction();

    EXPECT_TRUE(linesOpt.has_value());
    EXPECT_EQ(linesOpt->size(), 1);
    EXPECT_EQ(linesOpt->at(0), "two");
    EXPECT_EQ(waiter.m_waitUntilCount, 1);
  }


  // Single VFS lookup but the user cancels it.
  void test_cancelVFSLookup()
  {
    TEST_CASE("test_cancelVFSLookup");

    locAddFileLine(0, 2);

    // Cancel the first wait attempt.
    waiter.m_cancelCountdown = 0;

    // Serve the data from VFS.
    addFileToVFS(0);

    std::optional<std::vector<std::string>> linesOpt =
      callLspGetCodeLinesFunction();

    EXPECT_FALSE(linesOpt.has_value());
    EXPECT_EQ(waiter.m_waitUntilCount, 1);
  }


  // Two VFS lookups, and the second gets canceled.
  void test_cancelSecondVFSLookup()
  {
    TEST_CASE("test_cancelSecondVFSLookup");

    locAddFileLine(0, 2);
    locAddFileLine(1, 3);

    // Cancel the second wait attempt.
    waiter.m_cancelCountdown = 1;

    // Serve both files from VFS.
    addFileToVFS(0);
    addFileToVFS(1);

    std::optional<std::vector<std::string>> linesOpt =
      callLspGetCodeLinesFunction();

    EXPECT_FALSE(linesOpt.has_value());
    EXPECT_EQ(waiter.m_waitUntilCount, 2);
  }


  // One lookup goes to LSP and one to VFS.
  void test_oneLSP_oneVFS(bool lspFirst)
  {
    TEST_CASE_EXPRS("test_oneLSP_oneVFS", lspFirst);

    locAddFileLine(0, 2);
    locAddFileLine(1, 3);

    // Serve file 0/1 data from the LSP client's copy.
    addFileToLSP(lspFirst? 0:1);

    // Serve file 1/0 data from VFS.
    addFileToVFS(lspFirst? 1:0);

    std::optional<std::vector<std::string>> linesOpt =
      callLspGetCodeLinesFunction();

    EXPECT_TRUE(linesOpt.has_value());
    EXPECT_EQ(linesOpt->size(), 2);
    EXPECT_EQ(linesOpt->at(0), "two");
    EXPECT_EQ(linesOpt->at(1), "THREE");

    // Reading LSP does not require a wait, but reading VFS does.
    EXPECT_EQ(waiter.m_waitUntilCount, 1);
  }


  // Line number too large for LSP.
  void test_largeLineNumberLSP()
  {
    TEST_CASE("test_largeLuneNumberLSP");

    locAddFileLine(0, 5);

    // Serve the data from the LSP client's copy.
    addFileToLSP(0);

    std::optional<std::vector<std::string>> linesOpt =
      callLspGetCodeLinesFunction();

    EXPECT_TRUE(linesOpt.has_value());
    EXPECT_EQ(linesOpt->size(), 1);
    EXPECT_EQ(linesOpt->at(0),
      "<Line number 5 is out of range for \"/home/user/file0.cc\", "
      "which has 4 lines.>");
    EXPECT_EQ(waiter.m_waitUntilCount, 0);
  }


  // Line number too large for VFS.
  void test_largeLineNumberVFS()
  {
    TEST_CASE("test_largeLuneNumberVFS");

    locAddFileLine(0, 5);

    // Serve the data from VFS.
    addFileToVFS(0);

    std::optional<std::vector<std::string>> linesOpt =
      callLspGetCodeLinesFunction();

    EXPECT_TRUE(linesOpt.has_value());
    EXPECT_EQ(linesOpt->size(), 1);
    EXPECT_EQ(linesOpt->at(0),
      "<Line number 5 is out of range for \"/home/user/file0.cc\", "
      "which has 4 lines.>");
    EXPECT_EQ(waiter.m_waitUntilCount, 1);
  }


  // One lookup to VFS, and it yields an error.
  void test_errorVFSLookup()
  {
    TEST_CASE("test_errorVFSLookup");

    locAddErrFileLine(0, 5);

    // Serve the data from VFS.
    addErrFileToVFS(0);

    std::optional<std::vector<std::string>> linesOpt =
      callLspGetCodeLinesFunction();

    EXPECT_TRUE(linesOpt.has_value());
    EXPECT_EQ(linesOpt->size(), 1);
    EXPECT_EQ(linesOpt->at(0),
      "<Error: File not found (code PEC_FILE_NOT_FOUND)>");
    EXPECT_EQ(waiter.m_waitUntilCount, 1);
  }


  // Try to get a line from a non-local file.
  //
  // Eventually my LSP implementation should allow this, but for now it
  // does not and I want to exercise the refusal.
  //
  void test_nonLocal()
  {
    TEST_CASE("test_nonLocal");

    locations.push_back(HostFileLine(
      HostAndResourceName(HostName::asSSH("somehost"), "/some/file"),
      ln2li(3)
    ));

    std::optional<std::vector<std::string>> linesOpt =
      callLspGetCodeLinesFunction();

    EXPECT_TRUE(linesOpt.has_value());
    EXPECT_EQ(linesOpt->size(), 1);
    EXPECT_EQ(linesOpt->at(0),
      "<Not local: \"/some/file\" on ssh:somehost>");
    EXPECT_EQ(waiter.m_waitUntilCount, 0);
  }
};


CLOSE_ANONYMOUS_NAMESPACE


// Called from unit-tests.cc.
void test_lsp_get_code_lines(CmdlineArgsSpan args)
{
  // Each test runs with a fresh copy of `Tester` to avoid
  // cross-contamination.
  Tester().test_oneLSPLookup();
  Tester().test_oneVFSLookup();
  Tester().test_cancelVFSLookup();
  Tester().test_cancelSecondVFSLookup();
  Tester().test_oneLSP_oneVFS(true /*lspFirst*/);
  Tester().test_oneLSP_oneVFS(false /*lspFirst*/);
  Tester().test_largeLineNumberLSP();
  Tester().test_largeLineNumberVFS();
  Tester().test_errorVFSLookup();
  Tester().test_nonLocal();
}


// EOF
