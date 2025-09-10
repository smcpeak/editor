// lsp-client-manager.h
// `LSPClientManager`, for managing multiple `LSPClient`s.

// See license.txt for copyright and terms of use.

#ifndef EDITOR_LSP_CLIENT_MANAGER_H
#define EDITOR_LSP_CLIENT_MANAGER_H

#include "lsp-client-manager-fwd.h"    // fwds for this module

#include "doc-name-fwd.h"              // DocumentName [n]
#include "doc-type.h"                  // DocumentType
#include "host-file-line-fwd.h"        // HostFileLine [n]
#include "host-name.h"                 // HostName
#include "lsp-client.h"                // LSPClient
#include "named-td-fwd.h"              // NamedTextDocument [n]
#include "named-td-list-fwd.h"         // NamedTextDocumentList [n]
#include "vfs-connections-fwd.h"       // VFS_AbstractConnections [n]

#include "smqtutil/sync-wait-fwd.h"    // SynchronousWaiter [n]

#include "smbase/refct-serf.h"         // SerfRefCount, RCSerfOpt, NNRCSerf
#include "smbase/sm-macros.h"          // NO_OBJECT_COPIES
#include "smbase/std-memory-fwd.h"     // std::unique_ptr [n]
#include "smbase/std-optional-fwd.h"   // std::optional [n]
#include "smbase/std-string-fwd.h"     // std::string [n]
#include "smbase/std-vector-fwd.h"     // stdfwd::vector [n]

#include <QObject>

#include <iosfwd>                      // std::ostream [n]
#include <list>                        // std::list
#include <map>                         // std::map


// Describes the scope of a potential LSP client-server connection.
class LSPClientScope {
public:      // data
  // Host on which the LSP server is running.
  //
  // TODO: For now, this is always just the local host.
  HostName m_hostName;

  // Type of document that this server handles.
  //
  // Currently I assume each server only handles one document type,
  // but in the future I might need to generalize this.
  DocumentType m_documentType;

public:
  // ---- create-tuple-class: declarations for LSPClientScope +compare +gdvWrite +move
  /*AUTO_CTC*/ explicit LSPClientScope(HostName const &hostName, DocumentType const &documentType);
  /*AUTO_CTC*/ explicit LSPClientScope(HostName &&hostName, DocumentType &&documentType);
  /*AUTO_CTC*/ LSPClientScope(LSPClientScope const &obj) noexcept;
  /*AUTO_CTC*/ LSPClientScope(LSPClientScope &&obj) noexcept;
  /*AUTO_CTC*/ LSPClientScope &operator=(LSPClientScope const &obj) noexcept;
  /*AUTO_CTC*/ LSPClientScope &operator=(LSPClientScope &&obj) noexcept;
  /*AUTO_CTC*/ // For +compare:
  /*AUTO_CTC*/ friend int compare(LSPClientScope const &a, LSPClientScope const &b);
  /*AUTO_CTC*/ DEFINE_FRIEND_RELATIONAL_OPERATORS(LSPClientScope)
  /*AUTO_CTC*/ // For +gdvWrite:
  /*AUTO_CTC*/ operator gdv::GDValue() const;
  /*AUTO_CTC*/ std::string toString() const;
  /*AUTO_CTC*/ void write(std::ostream &os) const;
  /*AUTO_CTC*/ friend std::ostream &operator<<(std::ostream &os, LSPClientScope const &obj);

  // Return the scope applicable to `ntd`.
  static LSPClientScope forNTD(NamedTextDocument const *ntd);

  // Return: ::languageName(m_documentType)
  std::string languageName() const;

  // Return: m_hostName.toString()
  std::string hostString() const;

  // Return: "<languageName()> files on <hostString()> host"
  //
  // Example: "C++ files on local host"
  std::string description() const;
};


// A single client connection and its scope.
class ScopedLSPClient : public SerfRefCount {
  NO_OBJECT_COPIES(ScopedLSPClient);

private:     // data
  // Scope this connection is used for.
  LSPClientScope const m_scope;

  // The actual LSP client-server connection.
  LSPClient m_client;

public:      // methods
  ~ScopedLSPClient();

  // Make a new `LSPClient` associated with `scope`.
  //
  // The arguments after `scope` are the same as for the `LSPClient`
  // ctor.
  //
  // Note: This does not start the server process.  One must call
  // `client().startServer()` to do that.
  explicit ScopedLSPClient(
    LSPClientScope const &scope,
    bool useRealServer,
    std::string const &lspStderrLogFname,
    std::ostream * NULLABLE protocolDiagnosticLog);

  // Assert invariants.
  void selfCheck() const;

  LSPClientScope const &scope() const
    { return m_scope; }

  LSPClient const &client() const
    { return m_client; }
  LSPClient &client()
    { return m_client; }

  // Get the set of open documents (including host info) w.r.t. the LSP
  // server for this connection.
  std::set<DocumentName> getOpenDocumentNames() const;
};


// Manages multiple `LSPClient`s.
//
// Among other things, this aggregates the signals emitted by all of
// the managed objects, distributing them to all of this class's
// listeners.
class LSPClientManager : public QObject,
                         public SerfRefCount {
  Q_OBJECT
  NO_OBJECT_COPIES(LSPClientManager);

private:     // data
  // List of all documents.  This is used to look up documents by name
  // when diagnostics arrive from an LSP server, and then pass those
  // diagnostics to the relevant document object.
  NNRCSerf<NamedTextDocumentList> const m_documentList;

  // Access to the file system, which is needed to handle all-uses
  // queries that refer to occurrences in files that are not open.
  NNRCSerf<VFS_AbstractConnections> const m_vfsConnections;

  // If true, we start the normal server when the time comes.
  // Otherwise, we start the fake test server.
  bool const m_useRealServer;

  // Directory in which the log files are created.
  std::string const m_logFileDirectory;

  // Optional stream to log protocol diagnostics to.  If this is
  // nullptr, the diagnostics are simply discarded.
  std::ostream * NULLABLE const m_protocolDiagnosticLog;

  /* All existing clients.

     Invariant: For all `s` in `m_clients`:
       m_clients[s]->scope() == s

     Invariant: There is no document simultaneously open in more than
     one client.

     Invariant: If all client objects are running normally, then the set
     of documents open across all clients is the same as the set of
     documents in `m_documentList` that are tracking changes.

     Invariant: For all files that are open in the client, if the most
     recently sent version is the same as the version in
     `m_documentList`, then the client and document agree about the
     contents.
  */
  std::map<LSPClientScope, std::unique_ptr<ScopedLSPClient>> m_clients;

  // List of LSP protocol errors.  For now, these just accumulate.
  //
  // TODO: Send them to the log file.
  std::list<std::string> m_lspErrorMessages;

private:     // methods
  // Take any pending diagnostics from `client` and distribute them to
  // the documents in `m_documentList`.
  void takePendingDiagnostics(LSPClient &client);

  // Append an LSP error message.
  void addLSPErrorMessage(std::string &&msg);

  // Connect the signals from `client` to `this`.
  void connectSignals(LSPClient *client);

  // Disconnect signals from `client`.
  void disconnectSignals(LSPClient *client);

  // Disconnect all client signals.
  void disconnectAllClientSignals();

  // Get the client object for `ntd`, asserting it exists.
  NNRCSerf<LSPClient const> getClientC(
    NamedTextDocument const *ntd) const;
  NNRCSerf<LSPClient> getClient(
    NamedTextDocument const *ntd);

  // Return the name of the log file to use to save the stderr of an LSP
  // server for `scope`.  This name is "initial" in the sense that if it
  // is already in use then we will pick a related name that is not in
  // use.
  std::string makeStderrLogFileInitialName(
    LSPClientScope const &scope) const;

  // True if `ntd` is open in `inClient` specifically.  Or, if
  // `inClient` is nullptr, then open in any client.
  bool fileIsOpen_inClient(
    NamedTextDocument const *ntd,
    LSPClient const * NULLABLE inClient) const;

  // Reset `ntd` to the state it should have when it is not open w.r.t.
  // an LSP server.
  void resetDocumentLSPData(NamedTextDocument *ntd);

private Q_SLOTS:
  // Slots to receive signals from `m_clients`.
  void slot_changedProtocolState() noexcept;
  void slot_hasPendingDiagnostics() noexcept;
  void slot_hasPendingErrorMessages() noexcept;
  void slot_hasReplyForID(int id) noexcept;

public:      // methods
  ~LSPClientManager();

  // Initially no clients.
  explicit LSPClientManager(
    NNRCSerf<NamedTextDocumentList> documentList,
    NNRCSerf<VFS_AbstractConnections> vfsConnections,
    bool useRealServer,
    std::string logFileDirectory,
    std::ostream * NULLABLE protocolDiagnosticLog);

  // Assert invariants.
  void selfCheck() const;

  bool useRealServer() const
    { return m_useRealServer; }

  // ---------------------------- Per-scope ----------------------------
  // Each of the following queries accepts an `ntd` parameter that, at
  // least, specifies which LSP server to talk to.

  // If we have an `LSPClient` appropriate for `ntd`, return a pointer
  // to it.  Otherwise return nullptr.
  RCSerfOpt<LSPClient const> getClientOptC(
    NamedTextDocument const *ntd) const;

  // Same, but non-const.
  RCSerfOpt<LSPClient> getClientOpt(
    NamedTextDocument const *ntd);

  // If we have an `LSPClient` for `ntd`, return it.  Otherwise create
  // one.  Throws an exception if doing so is not possible.
  NNRCSerf<LSPClient> getOrCreateClient(
    NamedTextDocument const *ntd);

  // Start the LSP server.  Return an explanation string on failure.
  std::optional<std::string> startServer(
    NamedTextDocument const *ntd);

  // Get the LSP protocol state.
  LSPProtocolState getProtocolState(
    NamedTextDocument const *ntd) const;

  // True if the LSP connection is normal.
  //
  // This does *not* imply that `ntd` itself is open on the server.
  bool isRunningNormally(
    NamedTextDocument const *ntd) const;

  // True if we have begun the process of initializing the LSP server,
  // but that has not resolved as either a success or failure.
  bool isInitializing(
    NamedTextDocument const *ntd) const;

  // Return a string that explains why `!isRunningNormally(ntd)`.  If it
  // is in fact running normally, say so.
  std::string explainAbnormality(
    NamedTextDocument const *ntd) const;

  // Return a string summarizing the overall LSP state.  (This is a
  // temporary substitute for better error reporting.)
  std::string getServerStatus(
    NamedTextDocument const *ntd) const;

  // Stop the LSP server, presumably as part of resetting it.  Return a
  // human-readable string describing what happened during the attempt.
  std::string stopServer(
    NamedTextDocument const *ntd);

  /* Get all of the code lines for `locations`.  The returned vector has
     one result for each element of `locations`.  If there is a problem
     with a particular file, just encode that in the returned string, as
     this is going straight to the user.

     This may perform a synchronous wait, in which case it will use
     `waiter`, possibly multiple times.  It returns `nullopt` if the
     wait is canceled at any point (no partial results are returned).

     This is not `const` because it causes state transitions within
     `m_vfsConnections`, although the expectation is there is no durable
     change after this call.

     Ensures: if return then return->size() == locations.size()
  */
  std::optional<stdfwd::vector<std::string>> getCodeLines(
    NamedTextDocument const *ntd,
    SynchronousWaiter &waiter,
    stdfwd::vector<HostFileLine> const &locations);

  // ---------------------------- Per-file -----------------------------
  // These queries accept an `ntd` that specifically refers to that
  // document, not just its LSP scope.

  // True if `ntd` is open w.r.t. the LSP server.
  bool fileIsOpen(NamedTextDocument const *ntd) const;

  // If `ntd` is "open" w.r.t. the LSP server, return a pointer to its
  // details.  Otherwise return nullptr.
  RCSerfOpt<LSPDocumentInfo const> getDocInfo(
    NamedTextDocument const *ntd) const;

  // Open `ntd` with the server as `languageId`.
  //
  // This can throw `XNumericConversion` if the version of `ntd` cannot
  // be expressed as an LSP version.
  //
  // Requires: !fileIsOpen(ntd)
  // Ensures:  fileIsOpen(ntd)
  void openFile(
    NamedTextDocument *ntd, std::string const &languageId);

  // Update `ntd` with the server.
  //
  // This can throw `XNumericConversion` if the version of `ntd` cannot
  // be expressed as an LSP version.  It can also throw `XMessage` if
  // some unexpected version number issues arise.
  //
  // Requires: fileIsOpen(ntd)
  void updateFile(NamedTextDocument *ntd);

  // Close `ntd` if it is open.  This also discards any diagnostics it
  // may have and tells it to stop tracking changes.
  //
  // Ensures: !fileIsOpen(ntd)
  void closeFile(NamedTextDocument *ntd);

  // --------------------------- LSP Queries ---------------------------
  // All of these queries accept `ntd`.  In some cases it refers
  // specifically to that file, and for others it merely specifies the
  // LSP scope.

  // Cancel request `id` if it is outstanding.  Discard any reply that
  // has already been received.
  //
  // Requires: isRunningNormally(ntd)
  void cancelRequestWithID(
    NamedTextDocument const *ntd,
    int id);

  // True if we have a reply for `id` waiting to be taken.
  //
  // Requires: isRunningNormally(ntd)
  bool hasReplyForID(
    NamedTextDocument const *ntd,
    int id) const;

  // Take and return the reply for `id`.
  //
  // Requires: isRunningNormally(ntd)
  // Requires: hasReplyForID(ntd, id)
  JSON_RPC_Reply takeReplyForID(
    NamedTextDocument const *ntd,
    int id);

  // Issue an `lsrk` request for information about the symbol at `coord`
  // in `ntd`.  Returns the request ID.
  //
  // Requires: fileIsOpen(ntd)
  int requestRelatedLocation(
    NamedTextDocument const *ntd,
    LSPSymbolRequestKind lsrk,
    TextMCoord coord);

  // Send an arbitrary request, returning the request ID.
  //
  // Requires: isRunningNormally(ntd)
  int sendArbitraryRequest(
    NamedTextDocument const *ntd,
    std::string const &method,
    gdv::GDValue const &params);

  // Send an arbitrary notification.
  //
  // Requires: isRunningNormally(ntd)
  void sendArbitraryNotification(
    NamedTextDocument const *ntd,
    std::string const &method,
    gdv::GDValue const &params);

Q_SIGNALS:
  // `LSPClient` signals.  Whenever one `LSPClient` emits one of these
  // signals, the `LSPClientManager` emits that same signal.

  // Indicates some client changed protocol state, which is useful to
  // the LSP status widget.
  void signal_changedProtocolState();

  // Some client had an error message.  Currently nothing listens to
  // this signal since this class just accumulates the errors (and then
  // ignores them...).
  void signal_hasPendingErrorMessages();

  // A reply was received.  Currently nothing listens to this because
  // code that sends a request always waits for the reply synchronously.
  void signal_hasReplyForID(int id);

  // `hasPendingDiagnostics` is missing from the above because this
  // class handles that signal itself.
};


#endif // EDITOR_LSP_CLIENT_MANAGER_H
