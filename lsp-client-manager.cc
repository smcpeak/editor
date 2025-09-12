// lsp-client-manager.cc
// Code for `lsp-client-manager` module.

#include "lsp-client-manager.h"                  // this module

#include "doc-name.h"                            // DocumentName
#include "fail-reason-opt.h"                     // FailReasonOpt
#include "host-name.h"                           // HostName
#include "json-rpc-reply.h"                      // JSON_RPC_Reply
#include "lsp-conv.h"                            // convertLSPDiagsToTDD, lspSendUpdatedContents
#include "lsp-data.h"                            // LSP_PublishDiagnosticsParams
#include "lsp-get-code-lines.h"                  // lspGetCodeLinesFunction
#include "named-td-list.h"                       // NamedTextDocumentList
#include "named-td.h"                            // NamedTextDocument
#include "td-diagnostics.h"                      // TextDocumentDiagnostics
#include "vfs-connections.h"                     // VFS_AbstractConnections

#include "smbase/exc.h"                          // smbase::xmessage, GENERIC_CATCH_{BEGIN,END}
#include "smbase/gdvalue.h"                      // gdv::GDValue
#include "smbase/set-util.h"                     // smbase::setInsertAll
#include "smbase/sm-is-equal.h"                  // smbase::is_equal
#include "smbase/sm-macros.h"                    // IMEMBFP
#include "smbase/sm-trace.h"                     // INIT_TRACE, etc.
#include "smbase/xassert-eq-container.h"         // XASSERT_EQUAL_SETS

#include <memory>                                // std::unique_ptr
#include <string>                                // std::string

using namespace gdv;
using namespace smbase;


INIT_TRACE("lsp-client-manager");


// -------------------------- ScopedLSPClient --------------------------
ScopedLSPClient::~ScopedLSPClient()
{}


ScopedLSPClient::ScopedLSPClient(
  LSPClientScope const &scope,
  bool useRealServer,
  std::string const &lspStderrLogFname,
  std::ostream * NULLABLE protocolDiagnosticLog)
:
  IMEMBFP(scope),
  m_client(useRealServer,
           lspStderrLogFname,
           protocolDiagnosticLog)
{
  selfCheck();
}


void ScopedLSPClient::selfCheck() const
{
  m_client.selfCheck();
}


std::set<DocumentName> ScopedLSPClient::getOpenDocumentNames() const
{
  std::set<std::string> fileNames = m_client.getOpenFileNames();

  // Augment each of the file names that `m_client` knows with the host
  // name that `m_scope` knows.
  std::set<DocumentName> documentNames;
  for (std::string const &fileName : fileNames) {
    documentNames.insert(
      DocumentName::fromFilename(m_scope.m_hostName, fileName));
  }

  return documentNames;
}


// ------------------------------ Global -------------------------------
LSPClientManager::~LSPClientManager()
{
  GENERIC_CATCH_BEGIN

  // As usual, disconnect before destorying.
  disconnectAllClientSignals();

  // If we still have connections, do not stop to perform graceful
  // shutdown, just kill them.  But do this explicitly with exception
  // protection to be safe.
  m_clients.clear();

  GENERIC_CATCH_END
}


LSPClientManager::LSPClientManager(
  NNRCSerf<NamedTextDocumentList> documentList,
  NNRCSerf<VFS_AbstractConnections> vfsConnections,
  bool useRealServer,
  std::string logFileDirectory,
  std::ostream * NULLABLE protocolDiagnosticLog)
:
  QObject(),
  SerfRefCount(),
  IMEMBFP(documentList),
  IMEMBFP(vfsConnections),
  IMEMBFP(useRealServer),
  IMEMBFP(logFileDirectory),
  IMEMBFP(protocolDiagnosticLog),
  m_clients(),
  m_lspErrorMessages()
{
  selfCheck();
}


void LSPClientManager::selfCheck() const
{
  // Check the set of open documents in the clients against the master
  // document list.
  {
    // Get the set of documents open across all LSP connections.
    std::set<DocumentName> openLSPDocs;
    for (auto const &kv : m_clients) {
      LSPClientScope const &scope = kv.first;
      ScopedLSPClient const &scopedClient = *(kv.second);

      // Check map key/value invariant.
      xassert(scopedClient.scope() == scope);

      // Add the elements open with this client.
      std::set<DocumentName> openWithClient =
        scopedClient.getOpenDocumentNames();
      std::size_t numInserted = setInsertAll(openLSPDocs, openWithClient);

      // All should have been inserted since no file should be open with
      // more than one LSP server.
      xassert(numInserted == openWithClient.size());
    }

    // LSP clients and the document list agree about what is open.
    std::set<DocumentName> trackedDocs =
      m_documentList->getTrackingChangesDocumentNames();

    XASSERT_EQUAL_SETS(openLSPDocs, trackedDocs);
  }

  // Check the contents of documents against the clients.
  {
    // Count the LSP-open files we do and do not check, so I can manually
    // confirm nearly all are checked.
    int numChecked = 0;
    int numUnchecked = 0;

    // For all files open with the LSP server, if it is supposed to be
    // up to date in LSP client, its copy should agree with the editor's
    // copy.
    for (int index=0; index < m_documentList->numDocuments(); ++index) {
      NamedTextDocument const *ntd =
        m_documentList->getDocumentAtC(index);
      EXN_CONTEXT(ntd->documentName());

      if (RCSerf<LSPDocumentInfo const> docInfo = getDocInfo(ntd)) {
        if (is_equal(docInfo->m_lastSentVersion,
                     ntd->getVersionNumber())) {
          xassert(docInfo->lastContentsEquals(ntd->getCore()));
          ++numChecked;
        }
        else {
          // The client's version is behind, presumably because
          // continuous update is not enabled.  Don't check anything in
          // this case.
          ++numUnchecked;
        }
      }
    }
    TRACE1_GDVN_EXPRS("LSPClientManager::selfCheck", numChecked, numUnchecked);
  }
}


void LSPClientManager::slot_changedProtocolState() noexcept
{
  GENERIC_CATCH_BEGIN

  // Relay, primarily to the LSP status widgets.
  Q_EMIT signal_changedProtocolState();

  GENERIC_CATCH_END
}


void LSPClientManager::slot_hasPendingDiagnostics() noexcept
{
  GENERIC_CATCH_BEGIN

  for (auto &kv : m_clients) {
    LSPClient &client = kv.second->client();

    takePendingDiagnostics(client);
  }

  GENERIC_CATCH_END
}


void LSPClientManager::takePendingDiagnostics(LSPClient &client)
{
  while (client.hasPendingDiagnostics()) {
    // Get some pending diagnostics.
    std::string fname = client.getFileWithPendingDiagnostics();
    std::unique_ptr<LSP_PublishDiagnosticsParams> lspDiags(
      client.takePendingDiagnosticsFor(fname));

    if (!lspDiags->m_version.has_value()) {
      // Just discard them.
      TRACE1("lsp: Received LSP diagnostics without a version.");
      continue;
    }

    // Convert to our internal format.
    std::unique_ptr<TextDocumentDiagnostics> tdd(
      convertLSPDiagsToTDD(lspDiags.get(), client.uriPathSemantics()));
    lspDiags.reset();

    DocumentName docName =
      DocumentName::fromFilename(HostName::asLocal(), fname);

    if (NamedTextDocument *doc =
          m_documentList->findDocumentByName(docName)) {
      doc->updateDiagnostics(std::move(tdd));
    }
    else {
      // This could happen if we notify the server of new contents and
      // then immediately close the document.
      TRACE1("lsp: Received LSP diagnostics for " << docName <<
             " but that file is not open in the editor.");
    }
  }
}


void LSPClientManager::slot_hasPendingErrorMessages() noexcept
{
  GENERIC_CATCH_BEGIN

  for (auto &kv : m_clients) {
    LSPClient &client = kv.second->client();

    while (client.hasPendingErrorMessages()) {
      addLSPErrorMessage(client.takePendingErrorMessage());
    }
  }

  Q_EMIT signal_hasPendingErrorMessages();

  GENERIC_CATCH_END
}


void LSPClientManager::addLSPErrorMessage(std::string &&msg)
{
  // I'm thinking this should also emit a signal, although right now I
  // don't have any component prepared to receive it.
  m_lspErrorMessages.push_back(std::move(msg));
}


void LSPClientManager::slot_hasReplyForID(int id) noexcept
{
  GENERIC_CATCH_BEGIN

  Q_EMIT signal_hasReplyForID(id);

  GENERIC_CATCH_END
}


// ----------------------------- Per-scope -----------------------------
void LSPClientManager::connectSignals(LSPClient *client)
{
  // Connect LSP signals.
  QObject::connect(
    client,    &LSPClient::signal_changedProtocolState,
    this, &LSPClientManager::slot_changedProtocolState);
  QObject::connect(
    client,    &LSPClient::signal_hasPendingDiagnostics,
    this, &LSPClientManager::slot_hasPendingDiagnostics);
  QObject::connect(
    client,    &LSPClient::signal_hasPendingErrorMessages,
    this, &LSPClientManager::slot_hasPendingErrorMessages);
  QObject::connect(
    client,    &LSPClient::signal_hasReplyForID,
    this, &LSPClientManager::slot_hasReplyForID);
}


void LSPClientManager::disconnectSignals(LSPClient *client)
{
  QObject::disconnect(client, nullptr, this, nullptr);
}


void LSPClientManager::disconnectAllClientSignals()
{
  for (auto &kv : m_clients) {
    LSPClient &client = kv.second->client();
    disconnectSignals(&client);
  }
}


RCSerfOpt<LSPClient const> LSPClientManager::getClientOptC(
  NamedTextDocument const *ntd) const
{
  LSPClientScope scope = LSPClientScope::forNTD(ntd);
  auto it = m_clients.find(scope);
  if (it != m_clients.end()) {
    return RCSerf<LSPClient const>(& (*it).second->client() );
  }
  else {
    return {};
  }
}


RCSerfOpt<LSPClient> LSPClientManager::getClientOpt(
  NamedTextDocument const *ntd)
{
  // Use the `const` lookup implementation.
  RCSerfOpt<LSPClient const> ret = getClientOptC(ntd);

  // Then cast it away.
  return RCSerfOpt<LSPClient>(const_cast<LSPClient*>(ret.ptr()));
}


NNRCSerf<LSPClient const> LSPClientManager::getClientC(
  NamedTextDocument const *ntd) const
{
  RCSerfOpt<LSPClient const> ret = getClientOptC(ntd);
  xassert(ret != nullptr);
  return ret.ptr();
}


NNRCSerf<LSPClient> LSPClientManager::getClient(
  NamedTextDocument const *ntd)
{
  NNRCSerf<LSPClient const> ret = getClientC(ntd);
  return const_cast<LSPClient*>(ret.ptr());
}


std::string LSPClientManager::makeStderrLogFileInitialName(
  LSPClientScope const &scope) const
{
  return stringb(
    m_logFileDirectory << "/"
    "lsp-server-" <<

    // The fact that this name is not necessarily unique is not a
    // problem because the log file infrastructure will add a suffix, if
    // needed, to ensure the name is unique on disk (not already in use
    // by any process).
    scope.semiUniqueIDString() <<

    ".log");
}


NNRCSerf<LSPClient> LSPClientManager::getOrCreateClient(
  NamedTextDocument const *ntd)
{
  LSPClientScope scope = LSPClientScope::forNTD(ntd);

  {
    auto it = m_clients.find(scope);
    if (it != m_clients.end()) {
      return NNRCSerf<LSPClient>(& (*it).second->client() );
    }
  }

  if (!scope.m_hostName.isLocal()) {
    xmessage("Cannot create LSP client for non-local host.");
  }

  if (!lspLanguageIdForDTOpt(scope.m_documentType)) {
    xmessage(stringb(
      "This editor does not know how to run an LSP server for " <<
      scope.languageName() << "."));
  }

  // Build a new LSP client object and store a pointer to it inside the
  // map.  But this does not start the server.
  auto itAndInserted =
    m_clients.emplace(
      std::move(scope),
      std::make_unique<ScopedLSPClient>(
        scope,
        m_useRealServer,
        makeStderrLogFileInitialName(scope),
        m_protocolDiagnosticLog
      ));

  LSPClient &client = (*(itAndInserted.first)).second->client();

  connectSignals(&client);

  return NNRCSerf<LSPClient>(&client);
}


FailReasonOpt LSPClientManager::startServer(
  NamedTextDocument const *ntd)
{
  try {
    return getOrCreateClient(ntd)->startServer(
      LSPClientScope::forNTD(ntd));
  }
  catch (XBase &x) {
    return x.getMessage();
  }
}


LSPProtocolState LSPClientManager::getProtocolState(
  NamedTextDocument const *ntd) const
{
  if (auto c = getClientOptC(ntd)) {
    return c->getProtocolState();
  }
  else {
    // From the client's point of view, creating the LSP client object
    // should be invisible.  If we created it right now, it would be
    // inactive.  So just say it is inactive.
    return LSP_PS_CLIENT_INACTIVE;
  }
}


bool LSPClientManager::isRunningNormally(
  NamedTextDocument const *ntd) const
{
  if (auto c = getClientOptC(ntd)) {
    return c->isRunningNormally();
  }
  else {
    return false;
  }
}


bool LSPClientManager::isInitializing(
  NamedTextDocument const *ntd) const
{
  return getProtocolState(ntd) == LSP_PS_INITIALIZING;
}


std::string LSPClientManager::explainAbnormality(
  NamedTextDocument const *ntd) const
{
  if (auto c = getClientOptC(ntd)) {
    return c->explainAbnormality();
  }
  else {
    // This is the message returned for `LSP_PS_CLIENT_INACTIVE`.
    return "The LSP server has not been started.";
  }
}


std::string LSPClientManager::getServerStatus(
  NamedTextDocument const *ntd) const
{
  std::ostringstream oss;

  oss << "Using fake server: " << GDValue(!useRealServer()) << ".\n";

  LSPClientScope scope = LSPClientScope::forNTD(ntd);
  oss << "LSP scope: " << scope.description() << ".\n";

  if (auto client = getClientOptC(ntd)) {
    oss << "Status: " << client->checkStatus() << "\n";

    oss << "Has pending diagnostics: "
        << GDValue(client->hasPendingDiagnostics()) << ".\n";

    if (std::size_t n = m_lspErrorMessages.size()) {
      oss << n << " errors:\n";
      for (std::string const &m : m_lspErrorMessages) {
        oss << "  " << m << "\n";
      }
    }
  }

  else {
    oss << "There is no LSP client object for this document's scope.";
  }

  return oss.str();
}


std::string LSPClientManager::stopServer(
  NamedTextDocument const *ntd)
{
  LSPClientScope scope = LSPClientScope::forNTD(ntd);
  auto it = m_clients.find(scope);
  if (it != m_clients.end()) {
    LSPClient &client = (*it).second->client();

    // Reset LSP data for all files that are open with `client`.
    for (int i=0; i < m_documentList->numDocuments(); ++i) {
      NamedTextDocument *doc = m_documentList->getDocumentAt(i);
      if (fileIsOpen_inClient(doc, &client)) {
        resetDocumentLSPData(doc);
      }
    }

    // Now stop the server.
    std::string shutdownMsg = client.stopServer();
    TRACE1("stopServer: LSPClient::stopServer() returned: " << shutdownMsg);

    // We do not remove the map entry.  First, the `LSPClient` object
    // needs to stick around to process the shutdown sequence, which has
    // only just begun.  Second, even after shutdown, we want to keep
    // that object since it is fine to reuse it.

    // We also do not disconnect signals here since we want to track
    // shutdown progress, and if the server is re-started, we would not
    // reconnect (since connection happens on creation).

    return shutdownMsg;
  }

  else {
    return "There is no LSP client object for this document's scope.";
  }
}


std::optional<std::vector<std::string>> LSPClientManager::getCodeLines(
  NamedTextDocument const *ntd,
  SynchronousWaiter &waiter,
  std::vector<HostFileLine> const &locations)
{
  if (auto client = getClientOptC(ntd)) {
    return lspGetCodeLinesFunction(
      waiter,
      locations,
      *client,
      *m_vfsConnections);
  }
  else {
    return {};
  }
}


// --------------------------- LSP Per-file ----------------------------
bool LSPClientManager::fileIsOpen(NamedTextDocument const *ntd) const
{
  return fileIsOpen_inClient(ntd, nullptr);
}


bool LSPClientManager::fileIsOpen_inClient(
  NamedTextDocument const *ntd,
  LSPClient const * NULLABLE inClient) const
{
  if (auto client = getClientOptC(ntd)) {
    return
      (!inClient || inClient == client) &&
      client->isRunningNormally() &&
      ntd->isCompatibleWithLSP() &&
      client->isFileOpen(ntd->filename());
  }

  return false;
}


RCSerf<LSPDocumentInfo const> LSPClientManager::getDocInfo(
  NamedTextDocument const *ntd) const
{
  if (fileIsOpen(ntd)) {
    return getClientC(ntd)->getDocInfo(ntd->filename());
  }
  else {
    return nullptr;
  }
}


void LSPClientManager::openFile(
  NamedTextDocument *ntd, std::string const &languageId)
{
  xassertPrecondition(!fileIsOpen(ntd));

  // This can throw `XNumericConversion`.
  LSP_VersionNumber version =
    LSP_VersionNumber::fromTDVN(ntd->getVersionNumber());

  NNRCSerf<LSPClient> client = getOrCreateClient(ntd);
  client->notify_textDocument_didOpen(
    ntd->filename(),
    languageId,
    version,
    ntd->getWholeFileString());

  ntd->beginTrackingChanges();

  xassertPostcondition(fileIsOpen(ntd));
}


void LSPClientManager::updateFile(NamedTextDocument *ntd)
{
  xassertPrecondition(fileIsOpen(ntd));

  lspSendUpdatedContents(*(getClient(ntd)), *ntd);
}


void LSPClientManager::closeFile(NamedTextDocument *ntd)
{
  if (fileIsOpen(ntd)) {
    getClient(ntd)->notify_textDocument_didClose(ntd->filename());

    resetDocumentLSPData(ntd);
  }
}


void LSPClientManager::resetDocumentLSPData(NamedTextDocument *ntd)
{
  ntd->discardLanguageServicesData();
}


// ---------------------------- LSP Queries ----------------------------
void LSPClientManager::cancelRequestWithID(
  NamedTextDocument const *ntd,
  int id)
{
  xassertPrecondition(isRunningNormally(ntd));

  getClient(ntd)->cancelRequestWithID(id);
}


bool LSPClientManager::hasReplyForID(
  NamedTextDocument const *ntd,
  int id) const
{
  xassertPrecondition(isRunningNormally(ntd));

  return getClientC(ntd)->hasReplyForID(id);
}


JSON_RPC_Reply LSPClientManager::takeReplyForID(
  NamedTextDocument const *ntd,
  int id)
{
  xassertPrecondition(isRunningNormally(ntd));
  xassertPrecondition(hasReplyForID(ntd, id));

  return getClient(ntd)->takeReplyForID(id);
}


int LSPClientManager::requestRelatedLocation(
  NamedTextDocument const *ntd,
  LSPSymbolRequestKind lsrk,
  TextMCoord coord)
{
  xassertPrecondition(fileIsOpen(ntd));

  return getClient(ntd)->requestRelatedLocation(
    lsrk, ntd->filename(), coord);
}


int LSPClientManager::sendArbitraryRequest(
  NamedTextDocument const *ntd,
  std::string const &method,
  gdv::GDValue const &params)
{
  xassertPrecondition(isRunningNormally(ntd));

  return getClient(ntd)->sendRequest(method, params);
}


void LSPClientManager::sendArbitraryNotification(
  NamedTextDocument const *ntd,
  std::string const &method,
  gdv::GDValue const &params)
{
  xassertPrecondition(isRunningNormally(ntd));

  getClient(ntd)->sendNotification(method, params);
}


// EOF
