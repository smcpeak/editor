// lsp-manager.h
// `LSPManager` class.

#ifndef EDITOR_LSP_MANAGER_H
#define EDITOR_LSP_MANAGER_H

#include "lsp-manager-fwd.h"                     // fwds for this module

#include "command-runner-fwd.h"                  // CommandRunner
#include "lsp-client-fwd.h"                      // LSPClient
#include "lsp-data-fwd.h"                        // LSP_PublishDiagnosticsParams, etc.
#include "lsp-symbol-request-kind.h"             // LSPSymbolRequestKind
#include "td-core-fwd.h"                         // TextDocumentCore
#include "textmcoord.h"                          // TextMCoord

#include <QObject>

#include "smbase/exclusive-write-file-fwd.h"     // smbase::ExclusiveWriteFile
#include "smbase/gdvalue.h"                      // gdv::GDValue
#include "smbase/refct-serf.h"                   // SerfRefCount, RCSerf
#include "smbase/sm-noexcept.h"                  // NOEXCEPT
#include "smbase/sm-file-util.h"                 // SMFileUtil
#include "smbase/std-optional-fwd.h"             // std::optional
#include "smbase/std-string-fwd.h"               // std::string

#include <iosfwd>                                // std::ostream
#include <list>                                  // std::list
#include <memory>                                // std::unique_ptr
#include <set>                                   // std::set
#include <string>                                // std::string


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


// True if `fname` is *absolute* and exclusively uses *forward* slashes
// as path separators.
bool isValidLSPPath(std::string const &fname);

// Return a path that is absolute and normalized.
std::string normalizeLSPPath(std::string const &fname);


// Information about a document that is currently "open" w.r.t. the LSP
// protocol.
class LSPDocumentInfo : public SerfRefCount {
public:      // data
  // Absolute file name.
  //
  // Invariant: isValidLSPPath(m_fname)
  std::string const m_fname;

  // The version number of the most recent document contents that were
  // sent to the server.
  //
  // TODO: This should have type `LSP_VersionNumber`.
  int m_lastSentVersion;

  // The contents most recently sent.  They were labeled with
  // `m_lastSentVersion`.
  //
  // One reason we store this is to detect the case where we are trying
  // to refresh diagnostics for a file that has not changed (because
  // another file changed that affected its results).  Update: I'm not
  // currently doing that.  It might not actually be necessary.
  //
  // Another reason is to allow checking that the editor's idea of the
  // file contents agrees with the manager's after potentially many
  // incremental updates.
  //
  // Never null.
  //
  std::unique_ptr<TextDocumentCore> m_lastSentContents;

  // True when we have sent updated contents but not received the
  // associated diagnostics.  Initially false.
  bool m_waitingForDiagnostics;

  // Diagnostics that were received for this file but have not yet been
  // taken by the client.  Initially empty.
  std::unique_ptr<LSP_PublishDiagnosticsParams> m_pendingDiagnostics;

public:      // methods
  ~LSPDocumentInfo();

  LSPDocumentInfo(
    std::string const &fname,
    int lastSentVersion,
    std::string const &lastSentContentsString);

  // I only want to use the move ctor.
  LSPDocumentInfo(LSPDocumentInfo const &obj) = delete;

  LSPDocumentInfo(LSPDocumentInfo &&obj);

  // Assert invariants.
  void selfCheck() const;

  // Debug info, primarily.
  operator gdv::GDValue() const;

  // True if `m_pendingDiagnostics` is not null.
  bool hasPendingDiagnostics() const;

  // Get `m_lastSentContents` as a string.
  std::string getLastSentContentsString() const;

  // True if `m_lastSentContents` equals `doc`.
  bool lastContentsEquals(TextDocumentCore const &doc) const;
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
// The state transitions for the LSPManager as a whole, and for each of
// the files individually, are summarized in the diagram
// doc/lsp-state-diagram.ded.png .
//
class LSPManager : public QObject {
  Q_OBJECT;

private:     // data
  // True to run `clangd` instead of the test server instead.
  bool m_useRealClangd;

  // The file to which we send the server's stderr.  Can be null
  // depending on an envvar.
  std::unique_ptr<smbase::ExclusiveWriteFile> m_lspStderrFile;

  // If something goes wrong on the protocol level, debugging details
  // will be logged here.  If it is null, those details will just be
  // discarded.
  std::ostream * NULLABLE m_protocolDiagnosticLog;

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

  // Server's announced capabilities, as an LSP InitializeResult object.
  // This is null if we haven't received the capabilities.
  //
  // TODO: This is not a good way to store this.  I should parse it like
  // other LSP data.
  gdv::GDValue m_serverCapabilities;

  // Map from document file name (not URI) to its protocol state.  This
  // has the set of documents that are considered "open" w.r.t. the LSP
  // protocol.
  //
  // Invariant: For all `k`, `m_documentInfo[k].m_fname == k`.
  std::map<std::string, LSPDocumentInfo> m_documentInfo;

  // Set of files `f` for which `m_documentInfo[f].m_pendingDiagnostics`
  // is not null.
  std::set<std::string> m_filesWithPendingDiagnostics;

  // Error messages derived from unexpected protocol interactions that
  // don't break the protocol stream.
  std::list<std::string> m_pendingErrorMessages;

private:     // methods
  // Reset the state associated with the protocol.  This is done when we
  // shut down the server, and prepares for starting it again.
  void resetProtocolState();

  // Kill the server and return this object to its initial state.
  //
  // Modifies `m_lsp`, among others.
  void forciblyShutDown();

  // Append `msg` to the pending messages and signal the client.
  void addErrorMessage(std::string &&msg);

  // Handle newly-arrived `diags`.
  void handleIncomingDiagnostics(
    std::unique_ptr<LSP_PublishDiagnosticsParams> diags);

private Q_SLOTS:
  // Slots to respond to similarly-named `LSPClient` signals.
  void on_hasPendingNotifications() NOEXCEPT;
  void on_hasReplyForID(int id) NOEXCEPT;
  void on_hasProtocolError() NOEXCEPT;
  void on_childProcessTerminated() NOEXCEPT;

  // Slots for CommandRunner signals.
  void on_errorDataReady() NOEXCEPT;

public:      // methods
  ~LSPManager();

  // Create an inactive manager.  If `useRealClangd`, then run
  // `clangd` instead of `./lsp-test-server.py`.
  explicit LSPManager(
    bool useRealClangd,
    std::string const &lspStderrLogFname,
    std::ostream * NULLABLE protocolDiagnosticLog);

  // Check invariants, throwing an exception on failure.
  void selfCheck() const;

  // Start the server process and initialize the protocol.  On success,
  // return nullopt.
  //
  // If this attempt fails, return a string suitable for display to the
  // user regarding what happened.  that activity.  The string may
  // consist of multiple lines separated by newlines, but there is no
  // final newline.
  //
  std::optional<std::string> startServer();

  // Stop the server process.  Return a success report for the user.
  std::string stopServer();

  // Report on the current status of the LSP server.  This string
  // describes the protocol state plus the status of various internal
  // queues.
  std::string checkStatus() const;

  // Get basic protocol state.
  LSPProtocolState getProtocolState() const;

  // Return a human-readable string describing the protocol state.
  std::string describeProtocolState() const;

  // Get state plus an English description.
  LSPAnnotatedProtocolState getAnnotatedProtocolState() const;

  // True if the server is running normally.  This is a requirement to
  // send requests and notifications.
  bool isRunningNormally() const;

  // When `!isRunningNormally()`, this is a human-readable string
  // explaining what is abnormal about the current state.  If the
  // server is running normally, the string says so.
  std::string explainAbnormality() const;

  gdv::GDValue getServerCapabilities() const
    { return m_serverCapabilities; }

  // True if `fname` is open w.r.t. the LSP protocol.
  //
  // Requires: isValidLSPPath(fname)
  bool isFileOpen(std::string const &fname) const;

  // Return the set of names for which `isFileOpen` would return true.
  std::set<std::string> getOpenFileNames() const;

  // Get the document details for `fname`, or nullptr if it is not open.
  // This pointer is invalidated if `this` object changes.
  //
  // Requires: isValidLSPPath(fname)
  RCSerf<LSPDocumentInfo const> getDocInfo(
    std::string const &fname) const;

  // Send the "textDocument/didOpen" notification.
  //
  // Requires: isRunningNormally()
  // Requires: isValidLSPPath(fname)
  // Requires: !isFileOpen(fname)
  void notify_textDocument_didOpen(
    std::string const &fname,
    std::string const &languageId,
    int version,
    std::string &&contents);

  // Send the "textDocument/didChange" notification.
  //
  // Requires: isRunningNormally()
  // Requires: isFileOpen(params.getFname())
  void notify_textDocument_didChange(
    LSP_DidChangeTextDocumentParams const &params);

  // Convenience method for updating the entire document.
  void notify_textDocument_didChange_all(
    std::string const &fname,
    int version,
    std::string &&contents);

  // Send the "textDocument/didClose" notification.
  //
  // Requires: isRunningNormally()
  // Requires: isFileOpen(fname)
  // Ensures:  !isFileOpen(fname)
  void notify_textDocument_didClose(
    std::string const &fname);

  // True if we have any diagnostics ready for the client.
  bool hasPendingDiagnostics() const;

  // True if `fname` in particular has pending diagnostics.
  //
  // Requires: isValidLSPPath(fname)
  bool hasPendingDiagnosticsFor(std::string const &fname) const;

  // Get the first file that has pending diagnostics.
  std::string getFileWithPendingDiagnostics() const;

  // Take the pending diagnostics for `fname`.
  //
  // Requires: hasPendingDiagnosticsFor(fname)
  std::unique_ptr<LSP_PublishDiagnosticsParams>
  takePendingDiagnosticsFor(std::string const &fname);

  // True if we have errors to deliver.
  bool hasPendingErrorMessages() const;

  // How many error messages are pending.
  int numPendingErrorMessages() const;

  // Take the next available error.
  //
  // Requires: hasPendingErrorMessges()
  std::string takePendingErrorMessage();

  // Request information about the declaration at `position`.  Returns
  // the request ID.
  //
  // Requires: isRunningNormally()
  // Requires: isFileOpen(fname)
  int requestRelatedLocation(
    LSPSymbolRequestKind lsrk,
    std::string const &fname,
    TextMCoord position);

  // Send request `method` with `params`, returning the request ID.
  //
  // Requires: isRunningNormally()
  int sendRequest(
    std::string const &method,
    gdv::GDValue const &params);

  // True if we have a reply for request `id`.
  //
  // Requires: isRunningNormally()
  bool hasReplyForID(int id) const;

  // Take the pending reply for `id`, thus removing it from the manager
  // object.  This yields just the "result" part of the JSONRPC reply.
  //
  // Requires: isRunningNormally()
  // Requires: hasReplyForID(id)
  gdv::GDValue takeReplyForID(int id);

  // If the reply for `id` is ready, discard it.  If not, arrange to
  // discard it when it arrives.
  //
  // TODO: Send a cancelation to the server.
  //
  // Requires: isRunningNormally()
  void cancelRequestWithID(int id);

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

  // Emitted when a reply to request `id` is received.
  void signal_hasReplyForID(int id);
};


#endif // EDITOR_LSP_MANAGER_H
