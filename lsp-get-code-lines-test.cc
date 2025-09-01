// lsp-get-code-lines-test.cc
// Tests for `lsp-get-code-lines` module.

#include "unit-tests.h"                // decl for my entry point
#include "lsp-get-code-lines-test.h"   // this module

#include "lsp-get-code-lines.h"        // module under test

#include "host-file-and-line-opt.h"    // HostFileAndLineOpt
#include "lsp-manager.h"               // LSPManagerDocumentState

#include "smqtutil/sync-wait.h"        // SynchronousWaiter

#include "smbase/chained-cond.h"       // smbase::cc::z_le_lt
#include "smbase/gdvalue.h"            // gdv::toGDValue
#include "smbase/map-util.h"           // smbase::{mapInsertUniqueMove, mapInsertUnique}
#include "smbase/overflow.h"           // safeToInt
#include "smbase/set-util.h"           // smbase::setIsDisjointWith
#include "smbase/sm-macros.h"          // OPEN_ANONYMOUS_NAMESPACE
#include "smbase/sm-test.h"            // EXPECT_EQ
#include "smbase/sm-trace.h"           // INIT_TRACE, etc.
#include "smbase/string-util.h"        // stringToVectorOfUChar
#include "smbase/xassert.h"            // xassert

#include <exception>                   // std::uncaught_exceptions
#include <memory>                      // std::unique_ptr
#include <optional>                    // std::optional
#include <string>                      // std::string
#include <utility>                     // std::move
#include <vector>                      // std::vector

using namespace gdv;
using namespace smbase;


INIT_TRACE("lsp-get-code-lines-test");


// ------------------------ VFS_TestConnections ------------------------
VFS_TestConnections::VFS_TestConnections()
  : m_hosts(),
    m_issuedRequests()
{
  // The purpose of this signal and slot pair is to defer processing
  // until we return to the main event loop.
  QObject::connect(
    this, &VFS_TestConnections::signal_processRequests,
    this, &VFS_TestConnections::  slot_processRequests,
    Qt::QueuedConnection);
}


void VFS_TestConnections::selfCheck() const
{
  VFS_AbstractConnections::selfCheck();

  // There should not be any overlap in the domains of
  // `m_issuedRequests` and `m_availableReplies`.
  xassert(setIsDisjointWith(
    mapKeySet(m_issuedRequests),
    mapKeySet(m_availableReplies)));
}


VFS_TestConnections::ConnectionState
VFS_TestConnections::connectionState(
  HostName const &hostName) const
{
  if (auto value = mapFindOpt(m_hosts, hostName)) {
    return (**value).second;
  }
  else {
    return CS_INVALID;
  }
}


void VFS_TestConnections::issueRequest(
  RequestID &requestID /*OUT*/,
  HostName const &hostName,
  std::unique_ptr<VFS_Message> req)
{
  requestID = m_nextRequestID++;
  mapInsertUniqueMove(m_issuedRequests, requestID, std::move(req));

  TRACE1("emitting signal_processRequests");
  Q_EMIT signal_processRequests();
}


bool VFS_TestConnections::requestIsOutstanding(RequestID requestID) const
{
  return mapContains(m_issuedRequests, requestID);
}


void VFS_TestConnections::cancelRequest(RequestID requestID)
{
  // I don't call this in this test.
  xfailure("unimplemented");
}


int VFS_TestConnections::numOutstandingRequests() const
{
  return safeToInt(m_issuedRequests.size());
}


void VFS_TestConnections::slot_processRequests()
{
  TRACE1("in slot_processRequests; num issued requests is " <<
         m_issuedRequests.size());

  while (!m_issuedRequests.empty()) {
    // Dequeue the next request.
    auto it = m_issuedRequests.begin();
    RequestID id = (*it).first;
    std::unique_ptr<VFS_Message> msg = std::move((*it).second);
    m_issuedRequests.erase(it);

    if (auto rfr = dynamic_cast<VFS_ReadFileRequest const *>(msg.get())) {
      mapInsertUniqueMove(m_availableReplies, id,
        std::unique_ptr<VFS_Message>(processRFR(rfr).release()));

      TRACE1("emitting signal_vfsReplyAvailable(" << id << ")");
      Q_EMIT signal_vfsReplyAvailable(id);
    }

    else {
      xfailure("unrecognized message");
    }
  }
}


std::unique_ptr<VFS_ReadFileReply> VFS_TestConnections::processRFR(
  VFS_ReadFileRequest const *rfr)
{
  std::unique_ptr<VFS_ReadFileReply> reply(new VFS_ReadFileReply);

  if (auto itOpt = mapFindOpt(m_files, rfr->m_path)) {
    reply->m_success = true;
    reply->m_contents = stringToVectorOfUChar((**itOpt).second);
  }
  else {
    reply->m_success = false;
    reply->m_failureReasonCode = PortableErrorCode::PEC_FILE_NOT_FOUND;
    reply->m_failureReasonString = "File not found.";
  }

  return reply;
}


// ---------------------------- Local decls ----------------------------
OPEN_ANONYMOUS_NAMESPACE


// LSP manager that just serves a document set.
class TestLSPManager : public LSPManagerDocumentState {
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

  // Locations to look up.
  std::vector<HostFileAndLineOpt> locations;

  TestSynchronousWaiter waiter;

  TestLSPManager lspManager;

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
      lspManager.selfCheck();
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
      lspManager,
      vfsConnections);
  }

  // Add to `locations` a request for `lineNumber` in `fileIndex`.
  void locAddFileLine(int fileIndex, int lineNumber)
  {
    xassert(cc::z_le_lt(fileIndex, 2));
    locations.push_back(HostFileAndLineOpt(
      HostAndResourceName::localFile(fname[fileIndex]),
      LineNumber(lineNumber),
      0 /*byteIndex*/
    ));
  }

  // Add `fileIndex` to LSP.
  void addFileToLSP(int fileIndex)
  {
    xassert(cc::z_le_lt(fileIndex, 2));
    lspManager.addDoc(LSPDocumentInfo(
      fname[fileIndex],
      LSP_VersionNumber(1),
      fnameData[fileIndex]));
  }

  // Add `fileIndex` to VFS.
  void addFileToVFS(int fileIndex)
  {
    xassert(cc::z_le_lt(fileIndex, 2));
    mapInsertUnique(vfsConnections.m_files,
      fname[fileIndex], std::string(fnameData[fileIndex]));
  }


  // ------------------------------ Tests ------------------------------
  // Simple example of "happy path" lookup of one location for which the
  // file is in the LSP manager already (so no waiting occurs).
  void test_oneLSPLookup()
  {
    TEST_CASE("test_oneLSPLookup");

    locAddFileLine(0, 2);

    // Serve the data from the LSP manager's copy.
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


  // One lookup goes to LSP and one to VFS.
  void test_oneLSP_oneVFS(bool lspFirst)
  {
    TEST_CASE_EXPRS("test_oneLSP_oneVFS", lspFirst);

    locAddFileLine(0, 2);
    locAddFileLine(1, 3);

    // Serve file 0/1 data from the LSP manager's copy.
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

    // Serve the data from the LSP manager's copy.
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
};


CLOSE_ANONYMOUS_NAMESPACE


// ---------------------------- Entry point ----------------------------
// Called from unit-tests.cc.
void test_lsp_get_code_lines(CmdlineArgsSpan args)
{
  // Each test runs with a fresh copy of `Tester` to avoid
  // cross-contamination.
  Tester().test_oneLSPLookup();
  Tester().test_oneVFSLookup();
  Tester().test_cancelVFSLookup();
  Tester().test_oneLSP_oneVFS(true /*lspFirst*/);
  Tester().test_oneLSP_oneVFS(false /*lspFirst*/);
  Tester().test_largeLineNumberLSP();
  Tester().test_largeLineNumberVFS();
}


// EOF
