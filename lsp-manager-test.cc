// lsp-manager-test.cc
// Tests for `lsp-manager`.

#include "lsp-manager-test.h"          // this module
#include "unit-tests.h"                // decl for my entry point

#include "lsp-manager.h"               // module under test

#include "lsp-data.h"                  // LSP_PublishDiagnosticsParams

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


LSPManagerTester::LSPManagerTester(LSPTestRequestParams const &params)
  : m_lspManager(
      params.m_useRealClangd,
      "out/lsp-manager-test-server-stderr.txt"),
    m_params(params),
    m_done(false),
    m_failed(false)
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
  SMFileUtil sfu;
  m_lspManager.notify_textDocument_didOpen(
    sfu.getAbsolutePath("eclf.h"),
    "cpp",
    1,
    sfu.readFileAsString("eclf.h"));
  DIAG("Status: " << m_lspManager.checkStatus());
  m_lspManager.selfCheck();

  DIAG("Waiting for diagnostics notification...");
}


void LSPManagerTester::takeDiagnostics()
{
  std::unique_ptr<LSP_PublishDiagnosticsParams> diags =
    m_lspManager.takePendingDiagnostics();
  DIAG("Diagnostics: " << toGDValue(*diags).asIndentedString());
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
  stopServer();

  // Await `LSP_PS_MANAGER_INACTIVE`.

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
    LSPManagerTester tester(params);
    tester.testSynchronously();
  }

  DIAG("-------- asynchronous --------");
  {
    LSPManagerTester tester(params);
    tester.testAsynchronously();
  }
}


// EOF
