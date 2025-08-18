// lsp-manager-test.h
// `LSPManagerTester` class declaration for `lsp-manager-test.cpp`.

// See license.txt for copyright and terms of use.

#ifndef EDITOR_LSP_MANAGER_TEST_H
#define EDITOR_LSP_MANAGER_TEST_H

#include "lsp-manager.h"               // LSPManager
#include "lsp-test-request-params.h"   // LSPTestRequestParams

#include "smbase/sm-macros.h"          // NULLABLE
#include "smbase/sm-noexcept.h"        // NOEXCEPT

#include <QObject>

#include <iosfwd>                      // std::ostream


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

  // If non-zero, the ID of the `declaration` request we sent.
  int m_declarationRequestID;

public:      // methods
  ~LSPManagerTester();

  explicit LSPManagerTester(
    LSPTestRequestParams const &params,
    std::ostream * NULLABLE protocolDiagnosticLog);

  // Start the server process.
  void startServer();

  // Send "textDocument/didOpen" notification.
  void sendDidOpen();

  // Dequeue pending diagnostics.
  void takeDiagnostics();

  // Send "textDocument/declaration" request.
  void sendDeclarationRequest();

  // Take its reply, which must have been received.
  void takeDeclarationReply();

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
  void on_hasReplyForID(int id) NOEXCEPT;
  void on_hasPendingErrorMessages() NOEXCEPT;
};


#endif // EDITOR_LSP_MANAGER_TEST_H
