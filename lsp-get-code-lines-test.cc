// lsp-get-code-lines-test.cc
// Tests for `lsp-get-code-lines` module.

#include "unit-tests.h"                // decl for my entry point
#include "lsp-get-code-lines-test.h"   // this module

#include "lsp-get-code-lines.h"        // module under test

#include "host-file-and-line-opt.h"    // HostFileAndLineOpt
#include "lsp-manager.h"               // LSPManagerDocumentState

#include "smqtutil/sync-wait.h"        // SynchronousWaiter

#include "smbase/map-util.h"           // smbase::mapInsertUniqueMove
#include "smbase/overflow.h"           // safeToInt
#include "smbase/set-util.h"           // smbase::setIsDisjointWith
#include "smbase/sm-macros.h"          // OPEN_ANONYMOUS_NAMESPACE
#include "smbase/sm-test.h"            // EXPECT_EQ
#include "smbase/string-util.h"        // stringToVectorOfUChar
#include "smbase/xassert.h"            // xassert

#include <memory>                      // std::unique_ptr
#include <optional>                    // std::optional
#include <string>                      // std::string
#include <utility>                     // std::move
#include <vector>                      // std::vector

using namespace smbase;


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
  while (!m_issuedRequests.empty()) {
    // Dequeue the next request.
    auto it = m_issuedRequests.begin();
    RequestID id = (*it).first;
    std::unique_ptr<VFS_Message> msg = std::move((*it).second);
    m_issuedRequests.erase(it);

    if (auto rfr = dynamic_cast<VFS_ReadFileRequest const *>(msg.get())) {
      mapInsertUniqueMove(m_availableReplies, id,
        std::unique_ptr<VFS_Message>(processRFR(rfr).release()));
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


// "Waiter" that insists the client not wait at all.
class NoWaitingWaiter : public SynchronousWaiter {
public:      // methods
  virtual bool waitUntil(
    std::function<bool()> condition,
    int activityDialogDelayMS,
    std::string const &activityDialogTitle,
    std::string const &activityDialogMessage) override
  {
    xfailure("should not wait");
    return false;
  }
};


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


// Simple example of "happy path" lookup of one location for which the
// file is in the LSP manager already (so no waiting occurs).
void test_oneLSPLookup()
{
  std::string const fname1("/home/user/file1.cc");
  std::string const languageId("cpp");

  // Locations to look up.
  std::vector<HostFileAndLineOpt> locations;
  locations.push_back(HostFileAndLineOpt(
    HostAndResourceName::localFile(fname1),
    LineNumber(2),
    0 /*byteIndex*/
  ));

  NoWaitingWaiter waiter;

  TestLSPManager lspManager;
  lspManager.addDoc(LSPDocumentInfo(
    fname1,
    LSP_VersionNumber(1),
    std::string(
      "one\n"
      "two\n"
      "three\n")));

  // This object has no files to serve.
  VFS_TestConnections vfsConnections;

  std::optional<std::vector<std::string>> linesOpt =
    lspGetCodeLinesFunction(
      waiter,
      locations,
      lspManager,
      vfsConnections);

  EXPECT_TRUE(linesOpt.has_value());
  EXPECT_EQ(linesOpt->size(), 1);
  EXPECT_EQ(linesOpt->at(0), "two");
}


CLOSE_ANONYMOUS_NAMESPACE


// ---------------------------- Entry point ----------------------------
// Called from unit-tests.cc.
void test_lsp_get_code_lines(CmdlineArgsSpan args)
{
  test_oneLSPLookup();
}


// EOF
