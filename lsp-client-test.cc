// lsp-client-test.cc
// Tests for `lsp-client`.

#include "lsp-client-test.h"                    // this module
#include "unit-tests.h"                          // decl for my entry point

#include "lsp-client.h"                          // module under test

#include "json-rpc-reply.h"                      // JSON_RPC_Reply
#include "lsp-conv.h"                            // convertLSPDiagsToTDD, toLSP_VersionNumber, lspSendUpdatedContents
#include "lsp-client-scope.h"                    // LSPClientScope
#include "lsp-data.h"                            // LSP_PublishDiagnosticsParams
#include "lsp-symbol-request-kind.h"             // LSPSymbolRequestKind
#include "td-diagnostics.h"                      // TextDocumentDiagnostics
#include "td-obs-recorder.h"                     // TextDocumentObservationRecorder
#include "uri-util.h"                            // makeFileURI

#include "smqtutil/qtutil.h"                     // waitForQtEvent

#include "smbase/gdvalue.h"                      // gdv::toGDValue
#include "smbase/sm-env.h"                       // smbase::envAsBool
#include "smbase/sm-file-util.h"                 // SMFileUtil
#include "smbase/sm-macros.h"                    // OPEN_ANONYMOUS_NAMESPACE, smbase_loopi
#include "smbase/sm-test.h"                      // DIAG, envRandomizedTestIters
#include "smbase/sm-trace.h"                     // INIT_TRACE, etc.
#include "smbase/string-util.h"                  // stringVectorFromPointerArray
#include "smbase/xassert.h"                      // xassert
#include "smbase/xassert-eq-container.h"         // XASSERT_EQUAL_SETS

#include <functional>                            // std::function
#include <memory>                                // std::unique_ptr
#include <optional>                              // std::optional
#include <string>                                // std::string
#include <vector>                                // std::vector


using namespace gdv;
using namespace smbase;


INIT_TRACE("lsp-client-test");


char const *LSPClientTester::s_lspStderrInitialName =
  "out/lsp-client-test-server-stderr.txt";


LSPClientTester::~LSPClientTester()
{
  disconnectSignals();
}


LSPClientTester::LSPClientTester(
  LSPTestRequestParams const &params,
  std::ostream * NULLABLE protocolDiagnosticLog)
  : m_lspClient(
      params.m_useRealClangd,
      s_lspStderrInitialName,
      protocolDiagnosticLog),
    m_params(params),
    m_state(S_INIT),
    m_failed(false),
    m_declarationRequestID(0),
    m_contentRequestID(0),
    m_numEditsMade(0),
    m_numEditsToMake(envRandomizedTestIters(20, "LMT_EDIT_ITERS")),
    m_doc()
{
  m_doc.setDocumentName(
    DocumentName::fromLocalFilename(m_params.m_fname));
  m_doc.replaceWholeFileString(m_params.m_fileContents);

  xassert(m_lspClient.getOpenFileNames().empty());

  // I do not connect the signals here because the synchronous tests are
  // meant to run without using signals.
}


/*static*/ char const *LSPClientTester::toString(State state)
{
  RETURN_ENUMERATION_STRING_OR(
    State,
    NUM_STATES,
    (
      "S_INIT",
      "S_STARTING",
      "S_AWAITING_INITIAL_DIAGNOSTICS",
      "S_AWAITING_DECLARATION_REPLY",
      "S_AWAITING_INITIAL_CONTENTS",
      "S_AWAITING_UPDATED_DIAGNOSTICS",
      "S_AWAITING_UPDATED_CONTENTS",
      "S_STOPPING",
      "S_DONE",
    ),
    state,
    "<invalid state>"
  )
}


void LSPClientTester::setState(State newState)
{
  DIAG("State transition: " << toString(m_state) <<
       " -> " << toString(newState));
  m_state = newState;
}


void LSPClientTester::startServer()
{
  m_lspClient.selfCheck();
  xassert(m_lspClient.getProtocolState() == LSP_PS_CLIENT_INACTIVE);

  if (std::optional<std::string> failureReason =
        m_lspClient.startServer(LSPClientScope::localCPP())) {
    xfailure_stringbc("startServer: " << *failureReason);
  }

  DIAG("Status: " << m_lspClient.checkStatus());
  m_lspClient.selfCheck();

  DIAG("Initializing...");
}


void LSPClientTester::sendDidOpen()
{
  DIAG("Sending didOpen...");
  m_lspClient.notify_textDocument_didOpen(
    m_params.m_fname,
    "cpp",
    LSP_VersionNumber::fromTDVN(m_doc.getVersionNumber()),
    m_doc.getWholeFileString());
  DIAG("Status: " << m_lspClient.checkStatus());
  m_lspClient.selfCheck();

  XASSERT_EQUAL_SETS(m_lspClient.getOpenFileNames(),
                     std::set<string>{m_params.m_fname});
  EXPECT_EQ(
    m_lspClient.getDocInfo(m_params.m_fname)->m_waitingForDiagnostics,
    true);

  m_doc.beginTrackingChanges();

  DIAG("Waiting for diagnostics notification...");
}


void LSPClientTester::takeDiagnostics()
{
  std::unique_ptr<LSP_PublishDiagnosticsParams> diags =
    m_lspClient.takePendingDiagnosticsFor(
      m_lspClient.getFileWithPendingDiagnostics());
  DIAG("Diagnostics: " << toGDValue(*diags).asIndentedString());

  EXPECT_EQ(
    m_lspClient.getDocInfo(m_params.m_fname)->m_waitingForDiagnostics,
    false);

  m_doc.updateDiagnostics(
    convertLSPDiagsToTDD(diags.get(), URIPathSemantics::NORMAL));
}


void LSPClientTester::checkClientContents() const
{
  RCSerf<LSPDocumentInfo const> docInfo =
    m_lspClient.getDocInfo(m_doc.filename());
  xassert(docInfo->lastContentsEquals(m_doc.getCore()));
}


void LSPClientTester::sendDeclarationRequest()
{
  xassert(m_declarationRequestID == 0);

  DIAG("Sending declaration request...");
  m_declarationRequestID =
    m_lspClient.requestRelatedLocation(
      LSPSymbolRequestKind::K_DECLARATION,
      m_params.m_fname,

      // Coordinate mismatch for the column, but it doesn't really
      // matter here.
      TextMCoord(m_params.m_line, ByteIndex(m_params.m_col)));
  m_lspClient.selfCheck();

  DIAG("Status: " << m_lspClient.checkStatus());

  // The ID should not be pending yet.
  xassert(!m_lspClient.hasReplyForID(m_declarationRequestID));

  DIAG("Declaration request ID is " << m_declarationRequestID <<
       "; awaiting reply.");
}


void LSPClientTester::takeDeclarationReply()
{
  xassert(m_lspClient.hasReplyForID(m_declarationRequestID));
  JSON_RPC_Reply reply = m_lspClient.takeReplyForID(m_declarationRequestID);
  xassert(!m_lspClient.hasReplyForID(m_declarationRequestID));
  m_declarationRequestID = 0;

  m_lspClient.selfCheck();

  DIAG("Declaration reply: " << reply);
  DIAG("Status: " << m_lspClient.checkStatus());

  xassert(reply.isSuccess());
}


void LSPClientTester::waitUntil(std::function<bool()> condition)
{
  while (m_lspClient.isRunningNormally() && !condition()) {
    waitForQtEvent();
    TRACE1("Status: " << m_lspClient.checkStatus());
    m_lspClient.selfCheck();
  }

  if (!m_lspClient.isRunningNormally()) {
    xfailure_stringbc(
      "LSPClient not running normally: " << m_lspClient.checkStatus());
  }
}


void LSPClientTester::makeRandomEdit()
{
  TextDocumentChangeSequence edit = makeRandomChange(m_doc.getCore());
  VPVAL(toGDValue(edit));
  edit.applyToDocument(m_doc);

  ++m_numEditsMade;
}


void LSPClientTester::sendUpdatedContents()
{
  lspSendUpdatedContents(m_lspClient, m_doc);

  // Check the client's copy.
  checkClientContents();
}


void LSPClientTester::requestDocumentContents()
{
  DIAG("Sending getTextDocumentContents request");
  GDValue params(GDVMap{
    { "textDocument", GDVMap {
      { "uri", makeFileURI(m_params.m_fname, URIPathSemantics::NORMAL) },
      { "version", m_doc.getVersionNumber() },
    }},
  });
  m_contentRequestID = m_lspClient.sendRequest(
    "$/getTextDocumentContents", params);
}


void LSPClientTester::processContentsReply()
{
  JSON_RPC_Reply reply = m_lspClient.takeReplyForID(m_contentRequestID);
  m_contentRequestID = 0;

  xassert(reply.isSuccess());

  std::string text = reply.result().mapGetValueAt("text").stringGet();
  xassert(text == m_doc.getWholeFileString());

  LSP_VersionNumber version =
    LSP_VersionNumber(reply.result().mapGetValueAt("version").smallIntegerGet());
  xassert(version == LSP_VersionNumber::fromTDVN(m_doc.getVersionNumber()));

  DIAG("confirmed server agrees about contents");
}


void LSPClientTester::stopServer()
{
  std::string stopResult = m_lspClient.stopServer();
  DIAG("Stop: " << stopResult);

  DIAG("Status: " << m_lspClient.checkStatus());
  m_lspClient.selfCheck();

  DIAG("Waiting for shutdown...");
}


void LSPClientTester::acknowledgeShutdown()
{
  DIAG("Stopped.");
  m_lspClient.selfCheck();
}


void LSPClientTester::checkFinalState()
{
  // The main purpose of these checks is to ensure that both sync and
  // async properly maintain the state variables.
  xassert(m_state == S_DONE);
  xassert(m_failed == false);
  xassert(m_declarationRequestID == 0);
  xassert(m_contentRequestID == 0);
  xassert(m_numEditsMade == m_numEditsToMake);
  m_doc.selfCheck();

  // Also test this method.
  EXPECT_EQ(m_lspClient.lspStderrLogFname().value(),
    s_lspStderrInitialName);
}


void LSPClientTester::testSynchronously()
{
  // The synchronous code doesn't really use the state, but I update it
  // as a guide to what is supposed to happen in async mode.
  xassert(m_state == S_INIT);

  startServer();
  setState(S_STARTING);

  // This cannot use `waitUntil` because we are not running normally
  // until the condition is satisfied.
  while (m_lspClient.getProtocolState() != LSP_PS_NORMAL) {
    waitForQtEvent();
    TRACE1("Status: " << m_lspClient.checkStatus());
    m_lspClient.selfCheck();
  }

  sendDidOpen();
  setState(S_AWAITING_INITIAL_DIAGNOSTICS);

  waitUntil([this]() -> bool
    { return m_lspClient.hasPendingDiagnostics(); });

  takeDiagnostics();

  sendDeclarationRequest();
  setState(S_AWAITING_DECLARATION_REPLY);

  waitUntil([this]() -> bool
    { return m_lspClient.hasReplyForID(m_declarationRequestID); });

  takeDeclarationReply();

  setState(S_AWAITING_INITIAL_CONTENTS);
  syncCheckDocumentContents();

  // Prepare for incremental edits.
  checkClientContents();
  m_doc.beginTrackingChanges();

  // Experiment with incremental edits.
  while (m_numEditsMade < m_numEditsToMake) {
    makeRandomEdit();

    sendUpdatedContents();
    setState(S_AWAITING_UPDATED_DIAGNOSTICS);

    // Wait for the server to send diagnostics for the new version.
    waitUntil([this]() -> bool
      { return m_lspClient.hasPendingDiagnostics(); });

    // Incorporate the reply.
    takeDiagnostics();

    // Now ask the server what it thinks the document looks like.
    setState(S_AWAITING_UPDATED_CONTENTS);
    syncCheckDocumentContents();
  }

  stopServer();
  setState(S_STOPPING);

  // Cannot use `waitUntil` because the goal is to wait until the server
  // is not running normally.
  while (m_lspClient.getProtocolState() != LSP_PS_CLIENT_INACTIVE) {
    waitForQtEvent();
    TRACE1("Status: " << m_lspClient.checkStatus());
    m_lspClient.selfCheck();
  }

  setState(S_DONE);
  acknowledgeShutdown();
}


void LSPClientTester::syncCheckDocumentContents()
{
  requestDocumentContents();

  // Wait for the reply.
  DIAG("Waiting for getTextDocumentContents reply, id=" << m_contentRequestID);
  waitUntil([this]() -> bool
    { return m_lspClient.hasReplyForID(m_contentRequestID); });

  processContentsReply();
}


void LSPClientTester::connectSignals()
{
  QObject::connect(
    &m_lspClient, &LSPClient::signal_changedProtocolState,
    this,       &LSPClientTester::on_changedProtocolState);
  QObject::connect(
    &m_lspClient, &LSPClient::signal_hasPendingDiagnostics,
    this,       &LSPClientTester::on_hasPendingDiagnostics);
  QObject::connect(
    &m_lspClient, &LSPClient::signal_hasReplyForID,
    this,       &LSPClientTester::on_hasReplyForID);
  QObject::connect(
    &m_lspClient, &LSPClient::signal_hasPendingErrorMessages,
    this,       &LSPClientTester::on_hasPendingErrorMessages);
}


void LSPClientTester::disconnectSignals()
{
  QObject::disconnect(&m_lspClient, nullptr, this, nullptr);
}


void LSPClientTester::testAsynchronously()
{
  connectSignals();

  startServer();
  setState(S_STARTING);

  xassert(m_lspClient.getProtocolState() == LSP_PS_INITIALIZING);

  // The immediate next state is `LSP_PS_NORMAL`.

  // Meanwhile, pump the event queue until we are completely done.
  while (m_state != S_DONE && !m_failed) {
    waitForQtEvent();
    //DIAG("Status: " << m_lspClient.checkStatus());
    m_lspClient.selfCheck();
  }

  acknowledgeShutdown();

  // This is also (harmlessly redundantly) done in the destructor.
  disconnectSignals();

  xassert(!m_failed);
}


void LSPClientTester::on_changedProtocolState() NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  LSPProtocolState lspState = m_lspClient.getProtocolState();

  DIAG("changedProtocolState to: " << ::toString(lspState));

  if (m_state == S_STARTING && lspState == LSP_PS_NORMAL) {
    sendDidOpen();
    setState(S_AWAITING_INITIAL_DIAGNOSTICS);
  }

  else if (m_state == S_STOPPING && lspState == LSP_PS_CLIENT_INACTIVE) {
    setState(S_DONE);
  }

  GENERIC_CATCH_END
}


void LSPClientTester::on_hasPendingDiagnostics() NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  takeDiagnostics();

  if (m_state == S_AWAITING_INITIAL_DIAGNOSTICS) {
    sendDeclarationRequest();
    setState(S_AWAITING_DECLARATION_REPLY);
  }

  else if (m_state == S_AWAITING_UPDATED_DIAGNOSTICS) {
    requestDocumentContents();
    setState(S_AWAITING_UPDATED_CONTENTS);
  }

  else {
    xfailure("bad state");
  }


  GENERIC_CATCH_END
}


void LSPClientTester::on_hasReplyForID(int id) NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  if (m_state == S_AWAITING_DECLARATION_REPLY &&
      id == m_declarationRequestID) {
    DIAG("Received diagnostic reply ID " << id);

    takeDeclarationReply();

    requestDocumentContents();
    setState(S_AWAITING_INITIAL_CONTENTS);
  }

  else if ((m_state == S_AWAITING_INITIAL_CONTENTS ||
            m_state == S_AWAITING_UPDATED_CONTENTS) &&
           id == m_contentRequestID) {
    processContentsReply();

    if (m_numEditsMade < m_numEditsToMake) {
      makeRandomEdit();

      sendUpdatedContents();
      setState(S_AWAITING_UPDATED_DIAGNOSTICS);
    }
    else {
      stopServer();
      setState(S_STOPPING);
    }
  }

  else {
    DIAG("Received unexpected reply ID " << id);
    m_failed = true;
  }

  GENERIC_CATCH_END
}


void LSPClientTester::on_hasPendingErrorMessages() NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  DIAG("LSPClient reports errors.  Status:");
  DIAG(m_lspClient.checkStatus());

  m_failed = true;

  GENERIC_CATCH_END
}


// Called from unit-tests.cc.
void test_lsp_client(CmdlineArgsSpan args)
{
  SMFileUtil().createDirectoryAndParents("out");

  LSPTestRequestParams params =
    LSPTestRequestParams::getFromCmdLine(args);

  VPVAL(params.m_fname);
  VPVAL(params.m_line);
  VPVAL(params.m_col);
  VPVAL(params.m_useRealClangd);

  {
    DIAG("-------- synchronous --------");
    LSPClientTester tester(params, &std::cout);
    tester.testSynchronously();
    tester.checkFinalState();
  }

  if (!envAsBool("SYNC_ONLY")) {
    DIAG("-------- asynchronous --------");
    LSPClientTester tester(params, &std::cout);
    tester.testAsynchronously();
    tester.checkFinalState();
  }
}


// EOF
