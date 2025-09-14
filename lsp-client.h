// lsp-client.h
// `LSPClient`, which speaks the Language Server Protocol.

#ifndef EDITOR_LSP_CLIENT_H
#define EDITOR_LSP_CLIENT_H

#include "lsp-client-fwd.h"                      // fwds for this module

#include "command-runner-fwd.h"                  // CommandRunner [n]
#include "fail-reason-opt.h"                     // FailReasonOpt
#include "json-rpc-client-fwd.h"                 // JSON_RPC_Client [n]
#include "json-rpc-reply-fwd.h"                  // JSON_RPC_Error, JSON_RPC_Reply [n]
#include "line-index-fwd.h"                      // LineIndex [n]
#include "lsp-client-scope-fwd.h"                // LSPClientScope [n]
#include "lsp-data-fwd.h"                        // LSP_PublishDiagnosticsParams [n], etc.
#include "lsp-protocol-state.h"                  // LSPProtocolState
#include "lsp-symbol-request-kind.h"             // LSPSymbolRequestKind
#include "lsp-version-number.h"                  // LSP_VersionNumber
#include "td-core-fwd.h"                         // TextDocumentCore [n]
#include "textmcoord.h"                          // TextMCoord
#include "uri-util.h"                            // URIPathSemantics

#include <QObject>

#include "smbase/exclusive-write-file-fwd.h"     // smbase::ExclusiveWriteFile [n]
#include "smbase/gdvalue.h"                      // gdv::GDValue
#include "smbase/refct-serf.h"                   // SerfRefCount, RCSerf
#include "smbase/sm-noexcept.h"                  // NOEXCEPT
#include "smbase/sm-file-util.h"                 // SMFileUtil
#include "smbase/std-optional-fwd.h"             // std::optional [n]
#include "smbase/std-string-fwd.h"               // std::string [n]

#include <iosfwd>                                // std::ostream
#include <list>                                  // std::list
#include <memory>                                // std::unique_ptr
#include <set>                                   // std::set
#include <string>                                // std::string


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
  LSP_VersionNumber m_lastSentVersion;

  // The contents most recently sent.  They were labeled with
  // `m_lastSentVersion`.
  //
  // One reason we store this is to detect the case where we are trying
  // to refresh diagnostics for a file that has not changed (because
  // another file changed that affected its results).  Update: I'm not
  // currently doing that.  It might not actually be necessary.
  //
  // Another reason is to allow checking that the editor's idea of the
  // file contents agrees with the client object's after potentially
  // many incremental updates.
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
    LSP_VersionNumber lastSentVersion,
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

  // Return the text of the line at (0-based) `lineIndex` in the last
  // contents sent to the server.  If it is out of range, just report
  // that fact in the string.
  std::string getLastContentsCodeLine(LineIndex lineIndex) const;
};


// Portion of the LSP client functionality related to maintaining the
// state of tracked documents.  This part can be independently
// instantiated to help write tests.
//
// By itself, this class cannot add anything to its state.  A subclass
// has to do that.
class LSPClientDocumentState {
protected:   // data
  // Map from document file name (not URI) to its protocol state.  This
  // has the set of documents that are considered "open" w.r.t. the LSP
  // protocol.
  //
  // Invariant: For all `k`, `m_documentInfo[k].m_fname == k`.
  std::map<std::string, LSPDocumentInfo> m_documentInfo;

  // Set of files `f` for which `m_documentInfo[f].m_pendingDiagnostics`
  // is not null.
  std::set<std::string> m_filesWithPendingDiagnostics;

public:      // methods
  ~LSPClientDocumentState();

  // Initially, no documents are open.
  LSPClientDocumentState();

  // Assert invariants.
  void selfCheck() const;

  // Number of files open with the server.
  int numOpenFiles() const;

  // True if `fname` is open w.r.t. the LSP protocol.
  //
  // Requires: isValidLSPPath(fname)
  bool isFileOpen(std::string const &fname) const;

  // Return the set of names for which `isFileOpen` would return true.
  //
  // Ensures: return.size() == numOpenFiles()
  std::set<std::string> getOpenFileNames() const;

  // Get the document details for `fname`, or nullptr if it is not open.
  // This pointer is invalidated if `this` object changes.
  //
  // Requires: isValidLSPPath(fname)
  RCSerf<LSPDocumentInfo const> getDocInfo(
    std::string const &fname) const;
};


// Act as the central interface between the editor and the LSP server.
//
// The state transitions for the LSPClient as a whole, and for each of
// the files individually, are summarized in the diagram
// doc/lsp-state-diagram.ded.png .
//
class LSPClient : public QObject,
                  public LSPClientDocumentState,
                  public SerfRefCount {
  Q_OBJECT;

private:     // data
  // True to run `clangd` instead of the test server instead.
  bool m_useRealServer;

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

  // JSON-RPC protocol communicator.  null iff `m_commandRunner` is.
  std::unique_ptr<JSON_RPC_Client> m_lsp;

  // File system queries, etc.
  SMFileUtil m_sfu;

  // If nonzero, then we have sent the "initialize" request with this
  // ID, but not yet received the corresponding reply.  In that state,
  // the LSP is not available to service other requests.
  //
  // TODO: Make a wrapped integer class for request IDs.
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

  // Error messages derived from unexpected protocol interactions that
  // don't break the protocol stream.
  std::list<std::string> m_pendingErrorMessages;

  // If set, there was a protocol failure in the LSP layer that prevents
  // further communication.  This is distinct from a protocol error in
  // the JSON-RPC layer, which would be recorded inside the `m_lsp`
  // object.  The string here is intended for presentation to the user.
  std::optional<std::string> m_lspClientProtocolError;

  // How should URIs be interpreted?  Initially `NORMAL`.
  URIPathSemantics m_uriPathSemantics;

private:     // methods
  // Clear the set of open documents.
  //
  // Ensures: numOpenFiles() == 0
  void resetDocumentState();

  // Reset the state associated with the protocol.  This is done when we
  // shut down the server, and prepares for starting it again.
  //
  // Ensures: numOpenFiles() == 0
  void resetProtocolState();

  // Kill the server and return this object to its initial state.
  //
  // Modifies `m_lsp`, among others.
  void forciblyShutDown();

  // Append `msg` to the pending messages and signal the client.
  void addErrorMessage(std::string &&msg);

  // Record `error` as a protocol error that stops further
  // communication.  It was the response to `requestName`.
  void recordLSPProtocolError(
    JSON_RPC_Error const &error, char const *requestName);

  // Handle newly-arrived `diags`.
  void handleIncomingDiagnostics(
    std::unique_ptr<LSP_PublishDiagnosticsParams> diags);

  // Set `m_commandRunner` to run the LSP server program for `scope`.
  void configureCommandRunner(LSPClientScope const &scope);

  // Append `msg` to the file that holds the server's stderr.  A
  // timestamp is prepended, a newline appended, and the file
  // immediately flushed.  This should generally only be used when we
  // know the server process is not running.
  void logToLSPStderr(std::string const &msg);

  // Main logic of `stopServer`.
  std::string innerStopServer();

private Q_SLOTS:
  // Slots to respond to similarly-named `JSON_RPC_Client` signals.
  void on_hasPendingNotifications() NOEXCEPT;
  void on_hasReplyForID(int id) NOEXCEPT;
  void on_hasProtocolError() NOEXCEPT;
  void on_childProcessTerminated() NOEXCEPT;

  // Slots for CommandRunner signals.
  void on_errorDataReady() NOEXCEPT;

public:      // methods
  ~LSPClient();

  /* Create an inactive client.  `startServer()` still has to be called
     to make it do anything.

     If `useRealServer`, then (when `startServer()` is called) run the
     actual LSP server program (e.g., `clangd`) instead of
     `./lsp-test-server.py`.

     `lspStderrLogFname` gives the initial name of the file to write the
     LSP stderr to, but a different name may be chosen if the first is
     already in use.

     `protocolDiagnosticLog` is where (if anywhere) to send diagnostic
     details for protocol violations.
  */
  explicit LSPClient(
    bool useRealServer,
    std::string const &lspStderrLogFname,
    std::ostream * NULLABLE protocolDiagnosticLog);

  // Check invariants, throwing an exception on failure.
  void selfCheck() const;

  // Dump a large number of details of this object for diagnostic
  // purposes.
  operator gdv::GDValue() const;

  // Get how URIs should be interpreted.  This is only set after the
  // server has started since it is determined by the LSP scope.
  URIPathSemantics uriPathSemantics() const
    { return m_uriPathSemantics; }

  // Do the things that `uri-util` does, using `m_uriPathSemantics`.
  std::string makeFileURI(std::string_view fname) const;
  std::string getFileURIPath(std::string const &uri) const;

  // Get the actual name of the stderr log file, which may be different
  // from what was passed to the ctor.  It may also be absent because
  // there is an envvar that can suppress its creation entirely.
  std::optional<std::string> lspStderrLogFname() const;

  // Start the server process for `scope` and initialize the protocol.
  // On success, return nullopt.
  //
  // If this attempt fails, return a string suitable for display to the
  // user regarding what happened.  The string may consist of multiple
  // lines separated by newlines, but there is no final newline.
  //
  FailReasonOpt startServer(LSPClientScope const &scope);

  // Stop the server process.  Return a success report for the user.
  //
  // Ensures: numOpenFiles() == 0
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

  // True if we have begun the process of initializing the LSP server,
  // but that has not resolved as either a success or failure.
  bool isInitializing() const;

  // When `!isRunningNormally()`, this is a human-readable string
  // explaining what is abnormal about the current state.  If the
  // server is running normally, the string says so.
  std::string explainAbnormality() const;

  gdv::GDValue getServerCapabilities() const
    { return m_serverCapabilities; }

  // Send the "textDocument/didOpen" notification.
  //
  // Requires: isRunningNormally()
  // Requires: isValidLSPPath(fname)
  // Requires: !isFileOpen(fname)
  void notify_textDocument_didOpen(
    std::string const &fname,
    std::string const &languageId,
    LSP_VersionNumber version,
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
    LSP_VersionNumber version,
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

  // Take the pending reply for `id`, thus removing it from the client
  // object.  This yields just the "result" part of the JSONRPC reply.
  //
  // Requires: isRunningNormally()
  // Requires: hasReplyForID(id)
  JSON_RPC_Reply takeReplyForID(int id);

  // If the reply for `id` is ready, discard it.  If not, arrange to
  // discard it when it arrives.
  //
  // TODO: Send a cancelation to the server.
  //
  // Requires: isRunningNormally()
  void cancelRequestWithID(int id);

  // Send notification `method` with `params`.
  //
  // Requires: isRunningNormally()
  void sendNotification(
    std::string const &method,
    gdv::GDValue const &params);

Q_SIGNALS:
  // Emitted when the protocol state has (potentially) changed.  The
  // client must call `getProtocolState` to get the new state, which in
  // some cases will be the same as the previous state (which this class
  // does not keep track of).
  void signal_changedProtocolState();

  // Emitted when `numOpenFiles()` changes.
  void signal_changedNumOpenFiles();

  // Emitted when diagnostics arrive, so `hasPendingDiagnostics()` is
  // true.
  void signal_hasPendingDiagnostics();

  // Emitted when an error message is enqueued.
  void signal_hasPendingErrorMessages();

  // Emitted when a reply to request `id` is received.
  void signal_hasReplyForID(int id);
};


#endif // EDITOR_LSP_CLIENT_H
