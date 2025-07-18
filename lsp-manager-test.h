// lsp-manager-test.h
// `LSPManagerTester` class declaration for `lsp-manager-test.cpp`.

// See license.txt for copyright and terms of use.

#ifndef EDITOR_LSP_MANAGER_TEST_H
#define EDITOR_LSP_MANAGER_TEST_H

#include "lsp-manager.h"               // LSPManager
#include "lsp-test-request-params.h"   // LSPTestRequestParams

#include "smbase/sm-noexcept.h"        // NOEXCEPT

#include <QObject>


// Test harness for `LSPManager`.  Also serves as the recipients for its
// signals.
class LSPManagerTester : public QObject {
  Q_OBJECT;

public:      // data
  // The manager we are testing.
  LSPManager m_lspManager;

  // Request details from the command line.
  LSPTestRequestParams m_params;

  // In async mode, this means we got to succesful shutdown.
  bool m_done;

  // In async mode, this means we stopped early due to a failure.
  bool m_failed;

public:      // methods
  ~LSPManagerTester();

  LSPManagerTester(LSPTestRequestParams const &params);

  // Start the server process.
  void startServer();

  // Send "textDocument/didOpen" notification.
  void sendDidOpen();

  // Dequeue pending diagnostics.
  void takeDiagnostics();

  // Tell the server to shut down.
  void stopServer();

  // Print a message indicating the shut down finished.
  void acknowledgeShutdown();

  // Run the tests using explicit (but not busy) wait loops.
  void testSynchronously();

  // Dis/connect signals to `m_lspManager`.
  void connectSignals();
  void disconnectSignals();

  // Run the tests, reacting to signals only, not waiting.
  void testAsynchronously();

private Q_SLOTS:
  // Handle corresponding `LSPManager` signals.
  void on_changedProtocolState() NOEXCEPT;
  void on_hasPendingDiagnostics() NOEXCEPT;
  void on_hasPendingErrorMessages() NOEXCEPT;
};


#endif // EDITOR_LSP_MANAGER_TEST_H
