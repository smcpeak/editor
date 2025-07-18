// lsp-manager-test.cc
// Tests for `lsp-manager`.

#include "lsp-manager-test.h"          // this module

#include "lsp-manager.h"               // module under test

#include "lsp-data.h"                  // LSP_PublishDiagnosticsParams

#include "smqtutil/qtutil.h"           // waitForQtEvent

#include "smbase/gdvalue.h"            // gdv::toGDValue
#include "smbase/sm-env.h"             // smbase::envAsBool
#include "smbase/sm-file-util.h"       // SMFileUtil
#include "smbase/sm-macros.h"          // OPEN_ANONYMOUS_NAMESPACE
#include "smbase/sm-test.h"            // ARGS_MAIN
#include "smbase/trace.h"              // TRACE_ARGS
#include "smbase/xassert.h"            // xassert

#include <QCoreApplication>


using namespace gdv;
using namespace smbase;

using std::cout;


LSPManagerTester::~LSPManagerTester()
{}


LSPManagerTester::LSPManagerTester(LSPTestRequestParams const &params)
  : m_lspManager(
      params.m_useRealClangd,
      "out/lsp-manager-test-server-stderr.txt"),
    m_params(params)
{}


void LSPManagerTester::startServer()
{
  m_lspManager.selfCheck();
  xassert(m_lspManager.getProtocolState() == LSP_PS_MANAGER_INACTIVE);

  bool success;
  cout << "Start: " << m_lspManager.startServer(success) << "\n";
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


void entry(int argc, char **argv)
{
  TRACE_ARGS();

  // Enable Qt event loop, etc.
  QCoreApplication app(argc, argv);

  SMFileUtil().createDirectoryAndParents("out");

  LSPTestRequestParams params =
    LSPTestRequestParams::getFromCmdLine(argc, argv);

  LSPManagerTester tester(params);
  tester.testSynchronously();
}


ARGS_TEST_MAIN


// EOF
