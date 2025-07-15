// lsp-manager-test.cc
// Tests for `lsp-manager`.

#include "lsp-manager.h"               // module under test

#include "smqtutil/qtutil.h"           // waitForQtEvent

#include "smbase/sm-macros.h"          // OPEN_ANONYMOUS_NAMESPACE
#include "smbase/sm-test.h"            // ARGS_MAIN
#include "smbase/trace.h"              // TRACE_ARGS
#include "smbase/xassert.h"            // xassert

#include <QCoreApplication>


OPEN_ANONYMOUS_NAMESPACE


void test1()
{
  LSPManager lspManager(true /*useTestServer*/);
  xassert(lspManager.getProtocolState() == LSP_PS_MANAGER_INACTIVE);

  bool success;
  std::cout << "Start: " << lspManager.startServer(success) << "\n";
  xassert(success);

  std::cout << "Status: " << lspManager.checkStatus() << "\n";

  std::cout << "Initializing...\n";
  while (lspManager.getProtocolState() != LSP_PS_NORMAL) {
    waitForQtEvent();
    std::cout << "Status: " << lspManager.checkStatus() << "\n";
  }

  std::cout << "Stop: " << lspManager.stopServer() << "\n";
  while (lspManager.getProtocolState() != LSP_PS_MANAGER_INACTIVE) {
    waitForQtEvent();
    std::cout << "Status: " << lspManager.checkStatus() << "\n";
  }

  std::cout << "Stopped.\n";
}


void entry(int argc, char **argv)
{
  TRACE_ARGS();

  // Enable Qt event loop, etc.
  QCoreApplication app(argc, argv);

  test1();
}


CLOSE_ANONYMOUS_NAMESPACE


ARGS_TEST_MAIN


// EOF
