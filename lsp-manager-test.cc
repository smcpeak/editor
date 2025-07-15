// lsp-manager-test.cc
// Tests for `lsp-manager`.

#include "lsp-manager-test.h"          // this module

#include "lsp-manager.h"               // module under test

#include "smqtutil/qtutil.h"           // waitForQtEvent

#include "smbase/sm-env.h"             // smbase::envAsBool
#include "smbase/sm-file-util.h"       // SMFileUtil
#include "smbase/sm-macros.h"          // OPEN_ANONYMOUS_NAMESPACE
#include "smbase/sm-test.h"            // ARGS_MAIN
#include "smbase/trace.h"              // TRACE_ARGS
#include "smbase/xassert.h"            // xassert

#include <QCoreApplication>


using namespace smbase;


LSPManagerTester::~LSPManagerTester()
{}


LSPManagerTester::LSPManagerTester(LSPTestRequestParams const &params)
  : m_lspManager(
      params.m_useRealClangd,
      "out/lsp-manager-test-server-stderr.txt"),
    m_params(params)
{}


void LSPManagerTester::testSynchronously()
{
  xassert(m_lspManager.getProtocolState() == LSP_PS_MANAGER_INACTIVE);

  bool success;
  std::cout << "Start: " << m_lspManager.startServer(success) << "\n";
  xassert(success);

  std::cout << "Status: " << m_lspManager.checkStatus() << "\n";

  std::cout << "Initializing...\n";
  while (m_lspManager.getProtocolState() != LSP_PS_NORMAL) {
    waitForQtEvent();
    std::cout << "Status: " << m_lspManager.checkStatus() << "\n";
  }

  std::cout << "Stop: " << m_lspManager.stopServer() << "\n";
  while (m_lspManager.getProtocolState() != LSP_PS_MANAGER_INACTIVE) {
    waitForQtEvent();
    std::cout << "Status: " << m_lspManager.checkStatus() << "\n";
  }

  std::cout << "Stopped.\n";
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
