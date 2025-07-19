// lsp-manager.h
// `LSPManager` class.

#ifndef EDITOR_LSP_MANAGER_H
#define EDITOR_LSP_MANAGER_H

#include "lsp-manager-fwd.h"           // fwds for this module

#include "command-runner-fwd.h"        // CommandRunner
#include "lsp-client-fwd.h"            // LSPClient
#include "lsp-data-fwd.h"              // LSP_PublishDiagnosticsParams

#include <QObject>

#include "smbase/sm-noexcept.h"        // NOEXCEPT
#include "smbase/sm-file-util.h"       // SMFileUtil
#include "smbase/std-string-fwd.h"     // std::string

#include <list>                        // std::list
#include <memory>                      // std::unique_ptr
#include <string>                      // std::string


// Status of the LSPManager protocol.
enum LSPProtocolState {
  // ---- Normal lifecycle states ----

  // Normally, we transition through these in order, cycling back to
  // "inactive" at the end.

  // `LSPManager` is inactive; both of its pointers are null.  If we
  // were active previously, the old server process has terminated.
  LSP_PS_MANAGER_INACTIVE,

  // We have sent the "initialize" request, but not received its reply.
  LSP_PS_INITIALIZING,

  // We have received the "initialize" reply, and sent the "initialized"
  // notification.  The server is operating normally and can service
  // requests.
  LSP_PS_NORMAL,

  // We have sent the "shutdown" request, but not received a reply.
  LSP_PS_SHUTDOWN1,

  // We have sent the "exit" notification, but the server process has
  // not terminated.
  LSP_PS_SHUTDOWN2,

  // ---- Error states ----

  // Any of the above states except `LSP_PS_MANAGER_INACTIVE` can
  // transition to the error state.

  // `LSPClient` detected a protocol error.  We can't do anything more
  // with the server process.
  LSP_PS_PROTOCOL_ERROR,

  // ---- Broken states ----

  // These states should not occur, but I defined enumerators for them
  // since `checkStatus` reports them.  There is no place in the code
  // that emits `signal_changedProtocolState` for them since a
  // transition into these states is never expected.

  // The `LSPClient` is missing.  This is a broken state.
  LSP_PS_PROTOCOL_OBJECT_MISSING,

  // Despite the `CommandRunner` existing, it reports the server process
  // is not running.  This is a broken state.
  LSP_PS_SERVER_NOT_RUNNING,

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


// Information about a document that is currently "open" w.r.t. the LSP
// protocl.
class LSPDocumentInfo {
public:      // data
  // Absolute file name.
  std::string const m_fname;

  // The version number of the most recent document contents that were
  // sent to the server.
  int m_latestVersion;

public:      // methods
   ~LSPDocumentInfo();

   LSPDocumentInfo(std::string const &fname, int latestVersion);
};


// Act as the central interface between the editor and the LSP server.
//
// This is a higher-level wrapper than `LSPClient`.  `LSPClient`
// concerns itself with sending messages and receiving replies and
// notifications, really just at the JSON-RPC level.  This class
// packages those operations into bigger pieces, and tracks protocol
// state related to document analysis and LSP itself.
//
// But it is also different in that it owns the `CommandRunner` that
// manages the child process, whereas `LSPClient` does not.
//
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

  // File system queries, etc.
  SMFileUtil m_sfu;

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

  // Map from document name to its protocol state.  This has the set of
  // documents that are considered "open" w.r.t. the LSP protocol.
  //
  // Invariant: For all `k`, `m_documentInfo[k].m_fname == k`.
  std::map<std::string, LSPDocumentInfo> m_documentInfo;

  // Diagnostics messages we have decoded but have not been taken by the
  // client of this class.
  std::list<std::unique_ptr<LSP_PublishDiagnosticsParams>>
    m_pendingDiagnostics;

  // Error messages derived from unexpected protocol interactions that
  // don't break the protocol stream.
  std::list<std::string> m_pendingErrorMessages;

private:     // methods
  // Reset the state associated with the protocol.  This is done when we
  // shut down the server, and prepares for starting it again.
  void resetProtocolState();

  // Kill the server and return this object to its initial state.
  void forciblyShutDown();

  // Append `msg` to the pending messages and signal the client.
  void addErrorMessage(std::string &&msg);

private Q_SLOTS:
  // Slots to respond to similarly-named `LSPClient` signals.
  void on_hasPendingNotifications() NOEXCEPT;
  void on_hasReplyForID(int id) NOEXCEPT;
  void on_hasProtocolError() NOEXCEPT;
  void on_childProcessTerminated() NOEXCEPT;

  // We do not have `on_hasErrorData` because the server process is
  // started with its stderr redirected to a file.

public:      // methods
  ~LSPManager();

  // Create an inactive manager.  If `useRealClangd`, then run
  // `clangd` instead of `./lsp-test-server.py`.
  explicit LSPManager(
    bool useRealClangd,
    std::string lspStderrLogFname);

  // Check invariants, throwing an exception on failure.
  void selfCheck() const;

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

  // Report on the current status of the LSP server.
  std::string checkStatus() const;

  // Get basic protocol state.
  LSPProtocolState getProtocolState() const;

  // Get state plus an English description.
  LSPAnnotatedProtocolState getAnnotatedProtocolState() const;

  // True if `fname` is open w.r.t. the LSP protocol.  Requires that
  // `fname` be an absolute path.
  bool isFileOpen(std::string const &fname) const;

  // Send the "textDocument/didOpen" notification.  Requires that
  // `fname` be absolute, and that the file not already be open.
  void notify_textDocument_didOpen(
    std::string const &fname,
    std::string const &languageId,
    int version,
    std::string &&contents);

  // True if we have diagnostics ready for the client.
  bool hasPendingDiagnostics() const;

  // Take the next available diagnostics.  Requires
  // `hasPendingDiagnostics()`.
  std::unique_ptr<LSP_PublishDiagnosticsParams> takePendingDiagnostics();

  // True if we have errors to deliver.
  bool hasPendingErrorMessages() const;

  // How many error messages are pending.
  int numPendingErrorMessages() const;

  // Take the next available error.  Requires
  // `hasPendingErrorMessges()`.
  std::string takePendingErrorMessage();

Q_SIGNALS:
  // Emitted when the protocol state has (potentially) changed.  The
  // client must call `getProtocolState` to get the new state, which in
  // some cases will be the same as the previous state (which this class
  // does not keep track of).
  void signal_changedProtocolState();

  // Emitted when diagnostics arrive, so `hasPendingDiagnostics()` is
  // true.
  void signal_hasPendingDiagnostics();

  // Emitted when an error message is enqueued.
  void signal_hasPendingErrorMessages();
};


#endif // EDITOR_LSP_MANAGER_H
