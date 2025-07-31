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
#include "smbase/sm-test.h"            // ARGS_MAIN
#include "smbase/string-util.h"        // stringVectorFromPointerArray
#include "smbase/trace.h"              // TRACE_ARGS
#include "smbase/xassert.h"            // xassert

#include <string>                      // std::string
#include <vector>                      // std::vector


using namespace gdv;
using namespace smbase;

using std::cout;


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
  cout << "Start: " << startResult << "\n";
  xassert(success);

  cout << "Status: " << m_lspManager.checkStatus() << "\n";
  m_lspManager.selfCheck();

  cout << "Initializing...\n";
}


void LSPManagerTester::sendDidOpen()
{
  cout << "Sending didOpen...\n";
  SMFileUtil sfu;
  m_lspManager.notify_textDocument_didOpen(
    sfu.getAbsolutePath("eclf.h"),
    "cpp",
    1,
    sfu.readFileAsString("eclf.h"));
  cout << "Status: " << m_lspManager.checkStatus() << "\n";
  m_lspManager.selfCheck();

  cout << "Waiting for diagnostics notification...\n";
}


void LSPManagerTester::takeDiagnostics()
{
  std::unique_ptr<LSP_PublishDiagnosticsParams> diags =
    m_lspManager.takePendingDiagnostics();
  cout << "Diagnostics: " << toGDValue(*diags).asLinesString();
}


void LSPManagerTester::stopServer()
{
  cout << "Stop: " << m_lspManager.stopServer() << "\n";
  cout << "Status: " << m_lspManager.checkStatus() << "\n";
  m_lspManager.selfCheck();

  cout << "Waiting for shutdown...\n";
}


void LSPManagerTester::acknowledgeShutdown()
{
  cout << "Stopped.\n";
  m_lspManager.selfCheck();
}


void LSPManagerTester::testSynchronously()
{
  startServer();

  while (m_lspManager.getProtocolState() != LSP_PS_NORMAL) {
    waitForQtEvent();
    cout << "Status: " << m_lspManager.checkStatus() << "\n";
    m_lspManager.selfCheck();
  }

  sendDidOpen();

  while (!m_lspManager.hasPendingDiagnostics()) {
    waitForQtEvent();
    cout << "Status: " << m_lspManager.checkStatus() << "\n";
    m_lspManager.selfCheck();
  }

  takeDiagnostics();
  stopServer();

  while (m_lspManager.getProtocolState() != LSP_PS_MANAGER_INACTIVE) {
    waitForQtEvent();
    cout << "Status: " << m_lspManager.checkStatus() << "\n";
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
    //cout << "Status: " << m_lspManager.checkStatus() << "\n";
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

  cout << "changedProtocolState to: " << toString(state) << "\n";

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

  cout << "LSPManager reports errors.  Status:\n";
  cout << m_lspManager.checkStatus() << "\n";

  m_failed = true;

  GENERIC_CATCH_END
}


// Called from unit-tests.cc.
void test_lsp_manager(CmdlineArgsSpan args)
{
  SMFileUtil().createDirectoryAndParents("out");

  LSPTestRequestParams params =
    LSPTestRequestParams::getFromCmdLine(args);

  cout << "-------- synchronous --------\n";
  {
    LSPManagerTester tester(params);
    tester.testSynchronously();
  }

  cout << "-------- asynchronous --------\n";
  {
    LSPManagerTester tester(params);
    tester.testAsynchronously();
  }
}


// EOF
