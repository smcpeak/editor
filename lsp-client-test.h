// lsp-client-test.h
// `LSPClientTester` class declaration for `lsp-client-test.cc`.

// See license.txt for copyright and terms of use.

#ifndef EDITOR_LSP_CLIENT_TEST_H
#define EDITOR_LSP_CLIENT_TEST_H

#include "lsp-data-fwd.h"              // LSP_PublishDiagnosticsParams
#include "lsp-client.h"                // LSPClient
#include "lsp-test-request-params.h"   // LSPTestRequestParams
#include "named-td.h"                  // NamedTextDocument

#include "smbase/sm-macros.h"          // NULLABLE
#include "smbase/sm-noexcept.h"        // NOEXCEPT

#include <QObject>

#include <functional>                  // std::function
#include <iosfwd>                      // std::ostream


// Test harness for `LSPClient`.  Also serves as the recipients for its
// signals.
class LSPClientTester : public QObject {
  Q_OBJECT;

public:      // types
  // States in the state machine that drives the asychronous
  // interaction.  Basically, for every place that the sync code would
  // wait, we have a distinct state.  The async code does not wait, so
  // it uses the `m_state` variable to know what to do next.
  //
  // We go through the states in the order listed, except where noted.
  enum State {
    // Initial state.
    S_INIT,

    // Called `m_lspClient.startServer()`, waiting for it to be ready.
    S_STARTING,

    // Sent initial content, waiting for the diagnostics.
    S_AWAITING_INITIAL_DIAGNOSTICS,

    // Sent a request for a symbol declaration location, waiting for
    // that.
    S_AWAITING_DECLARATION_REPLY,

    // Sent a request for initial document contents, waiting for it.
    S_AWAITING_INITIAL_CONTENTS,

    // Sent incrementally updated contents, waiting for diagnostics.
    S_AWAITING_UPDATED_DIAGNOSTICS,

    // Sent a request for document contents after an incremental update,
    // waiting for it.
    //
    // From this state, we will go back to the previous state if we have
    // not made the specifed number of edits.  Otherwise, we move to the
    // next state.
    S_AWAITING_UPDATED_CONTENTS,

    // Called `m_lspClient.stopServer()`, waiting for it to be stopped.
    S_STOPPING,

    // Test is complete.
    S_DONE,

    NUM_STATES
  };

public:      // class data
  // Name to use as the basis for the stderr log file name.
  static char const *s_lspStderrInitialName;

public:      // instance data
  // The client we are testing.
  LSPClient m_lspClient;

  // Request details from the command line.
  LSPTestRequestParams m_params;

  // Current asynchronous state.
  State m_state;

  // In async mode, this means we stopped early due to a failure.
  bool m_failed;

  // If non-zero, the ID of the `declaration` request we sent.
  int m_declarationRequestID;

  // If non-zero, the ID of the content request.
  int m_contentRequestID;

  // Number of random edits made.
  int m_numEditsMade;

  // Number of edits we want to make.
  int const m_numEditsToMake;

  // The document we will simulate editing and exchanging with the
  // server.
  NamedTextDocument m_doc;

public:      // methods
  ~LSPClientTester();

  explicit LSPClientTester(
    LSPTestRequestParams const &params,
    std::ostream * NULLABLE protocolDiagnosticLog);

  // Return a string like "S_INIT".
  static char const *toString(State state);

  // Transition to a new state.
  void setState(State newState);

  // Start the server process.
  void startServer();

  // Send "textDocument/didOpen" notification.
  void sendDidOpen();

  // Dequeue pending diagnostics and apply them to `m_doc`.
  void takeDiagnostics();

  // Check that `m_lspClient` and `m_doc` have the same contents.
  void checkClientContents() const;

  // Send "textDocument/declaration" request.
  void sendDeclarationRequest();

  // Take its reply, which must have been received.
  void takeDeclarationReply();

  // For the synchonous test, wait until `condition` becomes true.  If
  // the client stops running normally, throw.
  void waitUntil(std::function<bool()> condition);

  // Make a random edit to `m_doc`.
  void makeRandomEdit();

  // Send pending changes in `m_doc` to the LSP server.
  void sendUpdatedContents();

  // Send a request for the server to send back its copy of the
  // document contents.  Store the request ID in `m_contentRequestID`.
  void requestDocumentContents();

  // Process the reply for document contents.
  void processContentsReply();

  // Tell the server to shut down.
  void stopServer();

  // Print a message indicating the shut down finished.
  void acknowledgeShutdown();

  // Verify the final state is as it should be.
  void checkFinalState();

  // Run the tests using explicit (but not busy) wait loops.
  void testSynchronously();

  // Synchronously check that the server agrees about the document
  // contents.
  void syncCheckDocumentContents();

  // Dis/connect signals to `m_lspClient`.
  void connectSignals();
  void disconnectSignals();

  // Run the tests, reacting to signals only, not waiting.
  void testAsynchronously();

private Q_SLOTS:
  // Handle corresponding `LSPClient` signals.
  void on_changedProtocolState() NOEXCEPT;
  void on_hasPendingDiagnostics() NOEXCEPT;
  void on_hasReplyForID(int id) NOEXCEPT;
  void on_hasPendingErrorMessages() NOEXCEPT;
};


#endif // EDITOR_LSP_CLIENT_TEST_H
