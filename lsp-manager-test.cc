// lsp-manager-test.cc
// Tests for `lsp-manager`.

#include "lsp-manager-test.h"          // this module
#include "unit-tests.h"                // decl for my entry point

#include "lsp-manager.h"               // module under test

#include "lsp-data.h"                  // LSP_PublishDiagnosticsParams
#include "lsp-symbol-request-kind.h"   // LSPSymbolRequestKind

#include "smqtutil/qtutil.h"           // waitForQtEvent

#include "smbase/gdvalue.h"            // gdv::toGDValue
#include "smbase/sm-env.h"             // smbase::envAsBool
#include "smbase/sm-file-util.h"       // SMFileUtil
#include "smbase/sm-macros.h"          // OPEN_ANONYMOUS_NAMESPACE
#include "smbase/sm-test.h"            // DIAG
#include "smbase/string-util.h"        // stringVectorFromPointerArray
#include "smbase/trace.h"              // TRACE_ARGS
#include "smbase/xassert.h"            // xassert

#include <string>                      // std::string
#include <vector>                      // std::vector


using namespace gdv;
using namespace smbase;


LSPManagerTester::~LSPManagerTester()
{
  disconnectSignals();
}


LSPManagerTester::LSPManagerTester(
  LSPTestRequestParams const &params,
  std::ostream * NULLABLE protocolDiagnosticLog)
  : m_lspManager(
      params.m_useRealClangd,
      "out/lsp-manager-test-server-stderr.txt",
      protocolDiagnosticLog),
    m_params(params),
    m_done(false),
    m_failed(false),
    m_declarationRequestID(0)
{
  // I do not connect the signals here because the synchronous tests are
  // meant to run without using signals.
}


void LSPManagerTester::startServer()
{
  m_lspManager.selfCheck();
  xassert(m_lspManager.getProtocolState() == LSP_PS_MANAGER_INACTIVE);

  bool success;
  std::string startResult = m_lspManager.startServer(success);
  DIAG("Start: " << startResult)
  xassert(success);

  DIAG("Status: " << m_lspManager.checkStatus());
  m_lspManager.selfCheck();

  DIAG("Initializing...");
}


void LSPManagerTester::sendDidOpen()
{
  DIAG("Sending didOpen...");
  m_lspManager.notify_textDocument_didOpen(
    m_params.m_fname,
    "cpp",
    1,
    std::string(m_params.m_fileContents));
  DIAG("Status: " << m_lspManager.checkStatus());
  m_lspManager.selfCheck();

  EXPECT_EQ(
    m_lspManager.getDocInfo(m_params.m_fname)->m_waitingForDiagnostics,
    true);

  DIAG("Waiting for diagnostics notification...");
}


void LSPManagerTester::takeDiagnostics()
{
  std::unique_ptr<LSP_PublishDiagnosticsParams> diags =
    m_lspManager.takePendingDiagnosticsFor(
      m_lspManager.getFileWithPendingDiagnostics());
  DIAG("Diagnostics: " << toGDValue(*diags).asIndentedString());

  EXPECT_EQ(
    m_lspManager.getDocInfo(m_params.m_fname)->m_waitingForDiagnostics,
    false);
}


void LSPManagerTester::sendDeclarationRequest()
{
  xassert(m_declarationRequestID == 0);

  DIAG("Sending declaration request...");
  m_declarationRequestID =
    m_lspManager.requestRelatedLocation(
      LSPSymbolRequestKind::K_DECLARATION,
      m_params.m_fname,
      TextMCoord(m_params.m_line, m_params.m_col));
  m_lspManager.selfCheck();

  DIAG("Status: " << m_lspManager.checkStatus());

  // The ID should not be pending yet.
  xassert(!m_lspManager.hasReplyForID(m_declarationRequestID));

  DIAG("Declaration request ID is " << m_declarationRequestID <<
       "; awaiting reply.");
}


void LSPManagerTester::takeDeclarationReply()
{
  xassert(m_lspManager.hasReplyForID(m_declarationRequestID));
  GDValue reply = m_lspManager.takeReplyForID(m_declarationRequestID);
  xassert(!m_lspManager.hasReplyForID(m_declarationRequestID));
  m_lspManager.selfCheck();

  DIAG("Declaration reply: " << reply.asIndentedString());
  DIAG("Status: " << m_lspManager.checkStatus());
}


void LSPManagerTester::stopServer()
{
  std::string stopResult = m_lspManager.stopServer();
  DIAG("Stop: " << stopResult);

  DIAG("Status: " << m_lspManager.checkStatus());
  m_lspManager.selfCheck();

  DIAG("Waiting for shutdown...");
}


void LSPManagerTester::acknowledgeShutdown()
{
  DIAG("Stopped.");
  m_lspManager.selfCheck();
}


void LSPManagerTester::testSynchronously()
{
  startServer();

  while (m_lspManager.getProtocolState() != LSP_PS_NORMAL) {
    waitForQtEvent();
    DIAG("Status: " << m_lspManager.checkStatus());
    m_lspManager.selfCheck();
  }

  sendDidOpen();

  while (!m_lspManager.hasPendingDiagnostics()) {
    waitForQtEvent();
    DIAG("Status: " << m_lspManager.checkStatus());
    m_lspManager.selfCheck();
  }

  takeDiagnostics();

  sendDeclarationRequest();

  while (!m_lspManager.hasReplyForID(m_declarationRequestID)) {
    waitForQtEvent();
    DIAG("Status: " << m_lspManager.checkStatus());
    m_lspManager.selfCheck();
  }

  takeDeclarationReply();

  stopServer();

  while (m_lspManager.getProtocolState() != LSP_PS_MANAGER_INACTIVE) {
    waitForQtEvent();
    DIAG("Status: " << m_lspManager.checkStatus());
    m_lspManager.selfCheck();
  }

  acknowledgeShutdown();
}


void LSPManagerTester::connectSignals()
{
  QObject::connect(
    &m_lspManager, &LSPManager::signal_changedProtocolState,
    this,        &LSPManagerTester::on_changedProtocolState);
  QObject::connect(
    &m_lspManager, &LSPManager::signal_hasPendingDiagnostics,
    this,        &LSPManagerTester::on_hasPendingDiagnostics);
  QObject::connect(
    &m_lspManager, &LSPManager::signal_hasReplyForID,
    this,        &LSPManagerTester::on_hasReplyForID);
  QObject::connect(
    &m_lspManager, &LSPManager::signal_hasPendingErrorMessages,
    this,        &LSPManagerTester::on_hasPendingErrorMessages);
}


void LSPManagerTester::disconnectSignals()
{
  QObject::disconnect(&m_lspManager, nullptr, this, nullptr);
}


void LSPManagerTester::testAsynchronously()
{
  connectSignals();

  startServer();

  xassert(m_lspManager.getProtocolState() == LSP_PS_INITIALIZING);

  // The immediate next state is `LSP_PS_NORMAL`.

  // Meanwhile, pump the event queue until we are completely done.
  while (!m_done && !m_failed) {
    waitForQtEvent();
    //DIAG("Status: " << m_lspManager.checkStatus());
    m_lspManager.selfCheck();
  }

  acknowledgeShutdown();

  // This is also (harmlessly redundantly) done in the destructor.
  disconnectSignals();

  xassert(!m_failed);
}


void LSPManagerTester::on_changedProtocolState() NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  LSPProtocolState state = m_lspManager.getProtocolState();

  DIAG("changedProtocolState to: " << toString(state));

  switch (state) {
    default:
      // Ignore.
      break;

    case LSP_PS_NORMAL:
      sendDidOpen();
      // Await the diagnostics notification.
      break;

    case LSP_PS_MANAGER_INACTIVE:
      m_done = true;
      break;
  }

  GENERIC_CATCH_END
}


void LSPManagerTester::on_hasPendingDiagnostics() NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  takeDiagnostics();

  sendDeclarationRequest();

  GENERIC_CATCH_END
}


void LSPManagerTester::on_hasReplyForID(int id) NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  if (id == m_declarationRequestID) {
    DIAG("Received diagnostic reply ID " << id);

    takeDeclarationReply();

    stopServer();

    // Await `LSP_PS_MANAGER_INACTIVE`.
  }

  else {
    DIAG("Received unexpected reply ID " << id);
    m_failed = true;
  }

  GENERIC_CATCH_END
}


void LSPManagerTester::on_hasPendingErrorMessages() NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  DIAG("LSPManager reports errors.  Status:");
  DIAG(m_lspManager.checkStatus());

  m_failed = true;

  GENERIC_CATCH_END
}


// Called from unit-tests.cc.
void test_lsp_manager(CmdlineArgsSpan args)
{
  SMFileUtil().createDirectoryAndParents("out");

  LSPTestRequestParams params =
    LSPTestRequestParams::getFromCmdLine(args);

  DIAG("-------- synchronous --------");
  {
    LSPManagerTester tester(params, &std::cout);
    tester.testSynchronously();
  }

  DIAG("-------- asynchronous --------");
  {
    LSPManagerTester tester(params, &std::cout);
    tester.testAsynchronously();
  }
}


// EOF
