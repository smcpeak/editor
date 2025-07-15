// lsp-manager.h
// `LSPManager` class.

#ifndef EDITOR_LSP_MANAGER_H
#define EDITOR_LSP_MANAGER_H

#include "lsp-manager-fwd.h"           // fwds for this module

#include "command-runner-fwd.h"        // CommandRunner
#include "lsp-client-fwd.h"            // LSPClient

#include <QObject>

#include "smbase/sm-noexcept.h"        // NOEXCEPT
#include "smbase/std-string-fwd.h"     // std::string

#include <memory>                      // std::unique_ptr
#include <string>                      // std::string


// Status of the LSPManager protocol.
enum LSPProtocolState {
  // `LSPManager` is inactive; both of its pointers are null.
  LSP_PS_MANAGER_INACTIVE,

  // The `LSPClient` is missing.  This is a broken state.
  LSP_PS_PROTOCOL_OBJECT_MISSING,

  // `LSPClient` detected a protocol error.  We can't do anything more
  // with the server process.
  LSP_PS_PROTOCOL_ERROR,

  // Despite the `CommandRunner` existing, it reports the server process
  // is not running.  This is a broken state.
  LSP_PS_SERVER_NOT_RUNNING,

  // We have begun the initialization procedure but it is not finished,
  // so no requests can be sent yet.
  LSP_PS_INITIALIZING,

  // We have sent the "shutdown" request, but not received a reply.
  LSP_PS_SHUTDOWN1,

  // We have sent the "exit" notification, but the server process has
  // not terminated.
  LSP_PS_SHUTDOWN2,

  // The server is operating normally and can service requests.
  LSP_PS_NORMAL,

  NUM_LSP_PROTOCOL_STATES
};

// Return a string like "LSP_PS_MANAGER_INACTIVE".
char const *toString(LSPProtocolState ps);


// Protocol state and a human-readable description of the state, which
// can have information beyond what is in `m_protocolState`.
class LSPAnnotatedProtocolState {
public:      // data
  // Basic state.
  LSPProtocolState m_protocolState;

  // Annotation/description.
  std::string m_description;

public:
  ~LSPAnnotatedProtocolState();
  LSPAnnotatedProtocolState(LSPProtocolState ps, std::string &&desc);
};


// Act as the central interface between the editor and the LSP server.
class LSPManager : public QObject {
  Q_OBJECT;

private:     // data
  // True to run `clangd` instead of the test server instead.
  bool m_useRealClangd;

  // Name of the file to which the stderr of the LSP process will be
  // written.
  std::string m_lspStderrLogFname;

  // Object to manage the child process.  This is null until the server
  // has been started, and returns to null if it is stopped.
  std::unique_ptr<CommandRunner> m_commandRunner;

  // Protocol communicator.  null iff `m_commandRunner` is.
  std::unique_ptr<LSPClient> m_lsp;

  // If nonzero, then we have sent the "initialize" request with this
  // ID, but not yet received the corresponding reply.  In that state,
  // the LSP is not available to service other requests.
  int m_initializeRequestID;

  // If nonzero, we have sent the "shutdown" request but not received
  // its reply.
  int m_shutdownRequestID;

  // If true, we have sent the "exit" notification but the child has not
  // terminated.
  bool m_waitingForTermination;

private:     // methods
  // Reset the state associated with the protocol.  This is done when we
  // shut down the server, and prepares for starting it again.
  void resetProtocolState();

  // Kill the server and return this object to its initial state.
  void forciblyShutDown();

private Q_SLOTS:
  // Slots to respond to similarly-named `LSPClient` signals.
  void on_hasReplyForID(int id) NOEXCEPT;
  void on_hasPendingNotifications() NOEXCEPT;
  void on_childProcessTerminated() NOEXCEPT;

public:      // methods
  ~LSPManager();

  // Create an inactive manager.  If `useRealClangd`, then run
  // `clangd` instead of `./lsp-test-server.py`.
  explicit LSPManager(
    bool useRealClangd,
    std::string lspStderrLogFname);

  // Start the server process and initialize the protocol.  Return a
  // string suitable for display to the user regarding the success of
  // that activity.  The string may consist of multiple lines separated
  // by newlines, but there is no final newline.
  //
  // Sets `success` to true if the server process started successfully,
  // false otherwise.
  std::string startServer(bool /*OUT*/ &success);

  // Stop the server process.  Return a success report for the user.
  std::string stopServer();

  // Report on the current status of the LSP server.  This destructively
  // takes and reports any accumulated stderr messages that were
  // reported by the server.  (LSP servers do not normally report things
  // that way though.)
  std::string checkStatus();

  // Get basic protocol state.
  LSPProtocolState getProtocolState() const;

  // Get state plus an English description.
  LSPAnnotatedProtocolState getAnnotatedProtocolState() const;
};


#endif // EDITOR_LSP_MANAGER_H
