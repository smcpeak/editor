// lsp-manager.cc
// Code for `lsp-manger.h`.

#include "lsp-manager.h"                         // this module

#include "command-runner.h"                      // CommandRunner
#include "lsp-data.h"                            // LSP_PublishDiagnosticsParams
#include "lsp-client.h"                          // LSPClient
#include "lsp-conv.h"                            // applyLSPDocumentChanges
#include "td-core.h"                             // TextDocumentCore
#include "uri-util.h"                            // makeFileURI, getFileURIPath

#include "smqtutil/qtutil.h"                     // toString(QString)

#include "smbase/container-util.h"               // smbase::contains
#include "smbase/datetime.h"                     // localTimeString
#include "smbase/exc.h"                          // GENERIC_CATCH_BEGIN/END, smbase::XFormat
#include "smbase/exclusive-write-file.h"         // smbase::ExclusiveWriteFile
#include "smbase/gdvalue-parser.h"               // gdv::GDValueParser
#include "smbase/gdvalue-set.h"                  // gdv::toGDValue(std::set)
#include "smbase/gdvalue.h"                      // gdv::GDValue
#include "smbase/list-util.h"                    // smbase::listMoveFront
#include "smbase/map-util.h"                     // smbase::{mapGetValueAt, mapKeySet}
#include "smbase/overflow.h"                     // safeToInt
#include "smbase/set-util.h"                     // smbase::{setInsert, setContains}
#include "smbase/sm-env.h"                       // smbase::envAsBool
#include "smbase/sm-file-util.h"                 // SMFileUtil
#include "smbase/sm-macros.h"                    // IMEMBFP, IMEMBMFP
#include "smbase/sm-trace.h"                     // INIT_TRACE, etc.
#include "smbase/string-util.h"                  // join
#include "smbase/stringb.h"                      // stringb
#include "smbase/xassert.h"                      // xassert, xassertPrecondition

#include <memory>                                // std::make_unique
#include <optional>                              // std::{optional, nullopt}
#include <string>                                // std::string
#include <utility>                               // std::move
#include <vector>                                // std::vector


using namespace gdv;
using namespace smbase;


INIT_TRACE("lsp-manager");


// ------------------------- LSPProtocolState --------------------------
DEFINE_ENUMERATION_TO_STRING_OR(
  LSPProtocolState,
  NUM_LSP_PROTOCOL_STATES,
  (
    "LSP_PS_MANAGER_INACTIVE",
    "LSP_PS_INITIALIZING",
    "LSP_PS_NORMAL",
    "LSP_PS_SHUTDOWN1",
    "LSP_PS_SHUTDOWN2",

    "LSP_PS_PROTOCOL_ERROR",

    "LSP_PS_PROTOCOL_OBJECT_MISSING",
    "LSP_PS_SERVER_NOT_RUNNING",
  ),
  "Unknown LSP"
)


// --------------------- LSPAnnotatedProtocolState ---------------------
LSPAnnotatedProtocolState::~LSPAnnotatedProtocolState()
{}


LSPAnnotatedProtocolState::LSPAnnotatedProtocolState(
  LSPProtocolState ps,
  std::string &&desc)
  : m_protocolState(ps),
    m_description(std::move(desc))
{}


// ----------------------------- LSP path ------------------------------
bool isValidLSPPath(std::string const &fname)
{
  SMFileUtil sfu;
  return sfu.isAbsolutePath(fname) &&
         sfu.hasNormalizedPathSeparators(fname);
}


std::string normalizeLSPPath(std::string const &fname)
{
  SMFileUtil sfu;
  std::string ret =
    sfu.normalizePathSeparators(sfu.getAbsolutePath(fname));
  xassert(isValidLSPPath(ret));
  return ret;
}


// -------------------------- LSPDocumentInfo --------------------------
LSPDocumentInfo::~LSPDocumentInfo()
{}


LSPDocumentInfo::LSPDocumentInfo(
  std::string const &fname,
  int lastSentVersion,
  std::string const &lastSentContentsString)
  : IMEMBFP(fname),
    IMEMBFP(lastSentVersion),
    m_lastSentContents(new TextDocumentCore),
    m_waitingForDiagnostics(false),
    m_pendingDiagnostics()
{
  m_lastSentContents->replaceWholeFileString(lastSentContentsString);

  selfCheck();
}


LSPDocumentInfo::LSPDocumentInfo(LSPDocumentInfo &&obj)
  : MDMEMB(m_fname),
    MDMEMB(m_lastSentVersion),
    MDMEMB(m_lastSentContents),
    MDMEMB(m_waitingForDiagnostics),
    MDMEMB(m_pendingDiagnostics)
{
  selfCheck();
}


void LSPDocumentInfo::selfCheck() const
{
  xassert(isValidLSPPath(m_fname));
  xassert(m_lastSentContents != nullptr);
}


LSPDocumentInfo::operator gdv::GDValue() const
{
  GDValue m(GDVK_TAGGED_ORDERED_MAP, "LSPDocumentInfo"_sym);
  GDV_WRITE_MEMBER_SYM(m_fname);
  GDV_WRITE_MEMBER_SYM(m_lastSentVersion);
  m.mapSetValueAtSym("lastSentContents_numLines", m_lastSentContents->numLines());
  GDV_WRITE_MEMBER_SYM(m_waitingForDiagnostics);
  m.mapSetValueAtSym("hasPendingDiagnostics", !!m_pendingDiagnostics);
  return m;
}


bool LSPDocumentInfo::hasPendingDiagnostics() const
{
  return !!m_pendingDiagnostics;
}


std::string LSPDocumentInfo::getLastSentContentsString() const
{
  return m_lastSentContents->getWholeFileString();
}


bool LSPDocumentInfo::lastContentsEquals(TextDocumentCore const &doc) const
{
  return *m_lastSentContents == doc;
}


// ---------------------------- LSPManager -----------------------------
void LSPManager::resetProtocolState()
{
  m_initializeRequestID = 0;
  m_shutdownRequestID = 0;
  m_waitingForTermination = false;
  m_serverCapabilities.reset();
  m_documentInfo.clear();
  m_filesWithPendingDiagnostics.clear();
  m_pendingErrorMessages.clear();
}


void LSPManager::forciblyShutDown()
{
  if (m_lsp) {
    // Disconnect signals.
    QObject::disconnect(m_lsp.get(), nullptr, this, nullptr);

    m_lsp.reset();
  }

  if (m_commandRunner) {
    QObject::disconnect(m_commandRunner.get(), nullptr, this, nullptr);

    m_commandRunner->killProcess();
    m_commandRunner.reset();
  }

  resetProtocolState();

  // Now in `LSP_PS_MANAGER_INACTIVE`.
  Q_EMIT signal_changedProtocolState();
}


void LSPManager::addErrorMessage(std::string &&msg)
{
  m_pendingErrorMessages.push_back(std::move(msg));
  Q_EMIT signal_hasPendingErrorMessages();
}


void LSPManager::handleIncomingDiagnostics(
  std::unique_ptr<LSP_PublishDiagnosticsParams> diags)
{
  std::string fname;
  try {
    fname = getFileURIPath(diags->m_uri);
  }
  catch (XFormat &x) {
    TRACE1("discarding received diagnostics with malformed URI " <<
           doubleQuote(diags->m_uri) << ": " << x.getMessage());
    return;
  }

  if (!diags->m_version.has_value()) {
    // Although not explained in the spec, it appears this happens
    // when a file is closed; the server sends a final notification
    // with no version and no diagnostics, presumably in order to
    // cause the editor to remove the diagnostics from its display.  I
    // do that when sending the "didClose" notification, so this
    // notification should be safe to ignore.
    TRACE1("discarding received diagnostics for " <<
           doubleQuote(fname) << " without a version number");
    return;
  }

  if (*diags->m_version < 0) {
    TRACE1("discarding received diagnostics for " <<
           doubleQuote(fname) << " with a negative version number");
    return;
  }

  if (!contains(m_documentInfo, fname)) {
    TRACE1("discarding received diagnostics for " <<
           doubleQuote(fname) << " that is not open (w.r.t. LSP)");
    return;
  }

  LSPDocumentInfo &docInfo = mapGetValueAt(m_documentInfo, fname);

  if (*diags->m_version != docInfo.m_lastSentVersion) {
    TRACE1("Discarding received diagnostics for " <<
           doubleQuote(fname) << " version " <<
           *diags->m_version << " because the last sent version is " <<
           docInfo.m_lastSentVersion);
    return;
  }

  TRACE1("Received diagnostics for " << doubleQuote(fname) <<
         " with version " << *diags->m_version <<
         " and numDiags=" << diags->m_diagnostics.size() << ".");

  docInfo.m_pendingDiagnostics = std::move(diags);
  docInfo.m_waitingForDiagnostics = false;

  setInsert(m_filesWithPendingDiagnostics, fname);

  Q_EMIT signal_hasPendingDiagnostics();
}


void LSPManager::on_hasPendingNotifications() NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  while (m_lsp->hasPendingNotifications()) {
    GDValue msg = m_lsp->takeNextNotification();
    TRACE2("received notification: " << msg.asIndentedString());

    GDValueParser msgParser(msg);

    try {
      msgParser.checkIsMap();
      std::string method = msgParser.mapGetValueAtStr("method").stringGet();
      GDValueParser paramsParser = msgParser.mapGetValueAtStr("params");
      paramsParser.checkIsMap();

      if (method == "textDocument/publishDiagnostics") {
        handleIncomingDiagnostics(
          std::make_unique<LSP_PublishDiagnosticsParams>(
            paramsParser));
      }
      else {
        addErrorMessage(stringb(
          "unhandled notification method: " << doubleQuote(method)));
      }
    }
    catch (XGDValueError &x) {
      addErrorMessage(stringb(
        "malformed notification " << msg.asString() << ": " << x.what()));
    }
  }

  GENERIC_CATCH_END
}


void LSPManager::on_hasReplyForID(int id) NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  if (id == m_initializeRequestID) {
    TRACE1("received initialize reply");
    m_serverCapabilities = m_lsp->takeReplyForID(id);
    m_initializeRequestID = 0;

    // Send "initialized" to complete the startup procedure.  There is
    // no reply to this so we simply assume we're ready now.
    m_lsp->sendNotification("initialized", GDVMap{});

    // Now in `LSP_PS_NORMAL`.
    Q_EMIT signal_changedProtocolState();
  }

  else if (id == m_shutdownRequestID) {
    TRACE1("received shutdown reply");
    m_lsp->takeReplyForID(id);         // Data is discarded.
    m_shutdownRequestID = 0;

    // Now, we send the "exit" notification, which should cause the
    // server process to terminate.
    m_lsp->sendNotification("exit", GDVMap{});
    m_waitingForTermination = true;

    // Now in `LSP_PS_SHUTDOWN2`.
    Q_EMIT signal_changedProtocolState();
  }

  else {
    TRACE1("received reply with ID " << id);

    // Relay to our client.
    Q_EMIT signal_hasReplyForID(id);
  }

  GENERIC_CATCH_END
}


void LSPManager::on_hasProtocolError() NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  TRACE1("on_hasProtocolError");

  // We are now in `LSP_PS_PROTOCOL_ERROR`.
  Q_EMIT signal_changedProtocolState();

  GENERIC_CATCH_END
}


void LSPManager::on_childProcessTerminated() NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  TRACE1("LSP server process terminated");

  // The child has already shut down, but we need to clean up the
  // associated objects and reset the protocol state.
  forciblyShutDown();

  GENERIC_CATCH_END
}


void LSPManager::on_errorDataReady() NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  if (m_commandRunner->hasErrorData()) {
    QByteArray data = m_commandRunner->takeErrorData();
    if (m_lspStderrFile) {
      TRACE2("Copying " << data.size() << " bytes of stderr data "
             "to LSP stderr log file.");
      m_lspStderrFile->stream().write(data.data(), data.size());
      m_lspStderrFile->stream().flush();
    }
    else {
      TRACE2("Discarding " << data.size() << " bytes of stderr data "
             "because there is no LSP stderr log file.");
    }
  }

  GENERIC_CATCH_END
}


LSPManager::~LSPManager()
{
  // Don't send a signal due to the forcible shutdown.
  QObject::disconnect(this, nullptr, nullptr, nullptr);

  forciblyShutDown();
}


LSPManager::LSPManager(
  bool useRealClangd,
  std::string const &lspStderrLogFname,
  std::ostream * NULLABLE protocolDiagnosticLog)
  : IMEMBFP(useRealClangd),
    m_lspStderrFile(),
    IMEMBFP(protocolDiagnosticLog),
    m_commandRunner(),
    m_lsp(),
    m_sfu(),
    m_initializeRequestID(0),
    m_shutdownRequestID(0),
    m_waitingForTermination(false),
    m_documentInfo(),
    m_pendingErrorMessages()
{
  SMFileUtil sfu;
  std::string const fname =
    sfu.normalizePathSeparators(lspStderrLogFname);
  sfu.createParentDirectories(fname);
  m_lspStderrFile = tryCreateExclusiveWriteFile(fname);

  if (m_lspStderrFile) {
    TRACE1("Server log file: " << m_lspStderrFile->getFname());
    m_lspStderrFile->stream() << "Started LSP manager at " <<
      localTimeString() << "\n";
    m_lspStderrFile->stream().flush();
  }

  selfCheck();
}


void LSPManager::selfCheck() const
{
  // Either both are present or neither is.
  xassert(m_commandRunner.operator bool() ==
          m_lsp.operator bool());

  if (m_lsp) {
    m_lsp->selfCheck();
  }

  // Set of files for which we observe pending diagnostics.
  std::set<std::string> filesWithPending;

  // The map keys agree with the associated values.
  for (auto const &kv : m_documentInfo) {
    std::string const &fname = kv.first;
    LSPDocumentInfo const &docInfo = kv.second;

    xassert(fname == docInfo.m_fname);
    if (docInfo.m_pendingDiagnostics) {
      setInsert(filesWithPending, fname);
    }

    docInfo.selfCheck();
  }

  xassert(filesWithPending == m_filesWithPendingDiagnostics);
}


std::optional<std::string> LSPManager::startServer()
{
  // ---- Start the server process ----
  if (m_commandRunner) {
    return "Server process has already been started and not stopped.";
  }

  // There shouldn't be an LSP object because its `CommandReference`
  // reference would be dangling.
  xassert(!m_lsp);

  m_commandRunner.reset(new CommandRunner());
  if (m_useRealClangd) {
    m_commandRunner->setProgram("clangd");
    if (envAsBool("CLANGD_VERBOSE_LOG")) {
      // Causes more details to be written to its stderr log file.
      m_commandRunner->setArguments(QStringList() <<
        "--log=verbose");
    }
  }
  else {
    // Need to use `env` due to cygwin symlink issues.
    m_commandRunner->setProgram("env");
    m_commandRunner->setArguments(QStringList() <<
      "python3" << "./lsp-test-server.py");
  }

  /* Although the goal is to send the server process stderr to
     `m_lspStderrLogFname`, the mutual exclusion mechanism that prevents
     log stomping when multiple editor processes are running does not
     allow us to use `setStandardErrorFile`.  This is because:

     * On Windows, we have a `HANDLE`, and `QProcess` cannot accept that
       for redirection (it only accepts a file name), and opening the
       file again using its name would fail due to the exclusion.

     * On Linux, we have a file descriptor, and again `QProcess` cannot
       accept that.  Furthermore, we are currently using a type of file
       locking that is not inherited by child processes, so the server
       would not be allowed to write to it even if we could pass a file
       descriptor.

     So, we process the stderr bytes ourslves in this process.  That has
     the downside of sometimes losing the last few lines when we run the
     destructor without first shutting down the server cleanly with
     `stopServer`.
  */

  TRACE1("Starting server process: " <<
         toString(m_commandRunner->getCommandLine()));
  m_commandRunner->startAsynchronous();

  // Synchronously wait for the process to start.  Starting the server
  // is an uncommon task, and we want reliable and immediate knowledge
  // of whether it started.
  if (!m_commandRunner->waitForStarted(5000 /*ms*/)) {
    std::string ret = stringb(
      "Failed to start server process: " <<
      toString(m_commandRunner->getErrorMessage()));

    m_commandRunner.reset();

    return ret;
  }
  TRACE1("Server process started, pid=" <<
         m_commandRunner->getChildPID());


  // ---- Start the LSP protocol communicator ----
  m_lsp.reset(new LSPClient(*m_commandRunner, m_protocolDiagnosticLog));

  // Connect the signals.
  QObject::connect(m_lsp.get(), &LSPClient::signal_hasPendingNotifications,
                   this,           &LSPManager::on_hasPendingNotifications);
  QObject::connect(m_lsp.get(), &LSPClient::signal_hasReplyForID,
                   this,           &LSPManager::on_hasReplyForID);
  QObject::connect(m_lsp.get(), &LSPClient::signal_hasProtocolError,
                   this,           &LSPManager::on_hasProtocolError);
  QObject::connect(m_lsp.get(), &LSPClient::signal_childProcessTerminated,
                   this,           &LSPManager::on_childProcessTerminated);

  QObject::connect(
    m_commandRunner.get(), &CommandRunner::signal_errorDataReady,
    this,                         &LSPManager::on_errorDataReady);

  // Kick off the initialization process.
  TRACE1("Sending initialize request.");
  m_initializeRequestID = m_lsp->sendRequest("initialize", GDVMap{
    // It seems `clangd` ignores this.
    { "processId", GDValue() },

    // This isn't entirely ignored, but it is only used for the
    // "workspace/symbol" request, and even then, only plays a
    // disambiguation role.  Since my intention is to run a single
    // `clangd` server process per machine, it doesn't make sense to
    // initialize it with any particular global "workspace" directory,
    // so I leave this null.
    { "rootUri", GDValue() },

    {
      "capabilities", GDVMap{
        {
          "textDocument", GDVMap{
            {
              "publishDiagnostics", GDVMap{
                // With this, diagnostics will have "relatedInformation"
                // rather than piling all of the info into the
                // "message".
                { "relatedInformation", true },
              },
            },
          },
        },
      },
    },
  });

  // Now in `LSP_PS_INITIALIZING`.
  Q_EMIT signal_changedProtocolState();

  return std::nullopt;
}


std::string LSPManager::stopServer()
{
  if (!m_lsp) {
    if (m_commandRunner) {
      forciblyShutDown();
      return "LSP was gone, but CommandRunner was not?  Killed process.";
    }
    else {
      return "Server is not running.";
    }
  }

  xassert(m_commandRunner);

  if (m_lsp->hasProtocolError()) {
    std::string msg = m_lsp->getProtocolError();
    forciblyShutDown();
    return stringb(
      "There was a protocol error, so server was killed: " << msg);
  }

  std::vector<std::string> msgs;

  if (m_initializeRequestID) {
    forciblyShutDown();
    msgs.push_back("The server did not respond to the request to "
                   "initialize, so it was killed.");
  }

  else if (m_shutdownRequestID) {
    forciblyShutDown();
    msgs.push_back("The server did not respond to a previous request "
                   "to shutdown, so it was killed.");
  }

  else if (m_waitingForTermination) {
    forciblyShutDown();
    msgs.push_back("The server did not shut down in response to the "
                   "\"exit\" notification, so it was killed.");
  }

  else {
    // This should lead to receiving a shutdown reply, which will
    // trigger the next shutdown phase.
    TRACE1("Sending shutdown request.");
    m_shutdownRequestID = m_lsp->sendRequest("shutdown", GDVMap{});
    msgs.push_back("Initiated server shutdown.");

    // Now in `LSP_PS_SHUTDOWN1`.
    Q_EMIT signal_changedProtocolState();
  }

  return join(msgs, "\n");
}


std::string LSPManager::checkStatus() const
{
  // Start with the protocol state.
  std::vector<std::string> msgs;
  msgs.push_back(describeProtocolState());

  if (m_lsp) {
    // Then summarize the pending/outstanding messages.
    if (int n = m_lsp->numPendingNotifications()) {
      msgs.push_back(stringb("Number of pending notifications: " << n));
    }

    if (std::set<int> ids = m_lsp->getOutstandingRequestIDs(); !ids.empty()) {
      msgs.push_back(stringb("Outstanding requests: " << toGDValue(ids)));
    }

    if (std::set<int> ids = m_lsp->getPendingReplyIDs(); !ids.empty()) {
      msgs.push_back(stringb("Pending replies: " << toGDValue(ids)));
    }
  }

  // Pending error messages.
  if (int n = numPendingErrorMessages()) {
    msgs.push_back(stringb(
      "There are " << n << " pending error messages:"));

    int i = 0;
    for (std::string const &msg : m_pendingErrorMessages) {
      msgs.push_back(stringb(
        "  " << ++i << ": " << msg));
    }
  }

  if (m_lspStderrFile) {
    msgs.push_back(stringb(
      "Server stderr is in " <<
      doubleQuote(m_lspStderrFile->getFname()) << "."));
  }
  else {
    msgs.push_back(stringb(
      "Server stderr is being discarded."));
  }

  return join(msgs, "\n");
}


LSPProtocolState LSPManager::getProtocolState() const
{
  return getAnnotatedProtocolState().m_protocolState;
}


std::string LSPManager::describeProtocolState() const
{
  LSPAnnotatedProtocolState aps = getAnnotatedProtocolState();
  return stringb(
    toString(aps.m_protocolState) << ": " <<
    aps.m_description);
}


LSPAnnotatedProtocolState LSPManager::getAnnotatedProtocolState() const
{
  // This conditions checked here must be kept synchronized with
  // `isRunningNormally`.

  if (!m_commandRunner) {
    xassert(!m_lsp);
    return LSPAnnotatedProtocolState(
      LSP_PS_MANAGER_INACTIVE,
      "LSP manager is inactive.");
  }

  if (!m_lsp) {
    return LSPAnnotatedProtocolState(
      LSP_PS_PROTOCOL_OBJECT_MISSING,
      "Server process is running, but the LSP protocol object is "
      "missing!  Stop+start the server to fix things.");
  }

  if (m_lsp->hasProtocolError()) {
    return LSPAnnotatedProtocolState(
      LSP_PS_PROTOCOL_ERROR,
      stringb(
        "There was an LSP protocol error: " <<
        m_lsp->getProtocolError()));
  }

  if (!m_lsp->isChildRunning()) {
    return LSPAnnotatedProtocolState(
      LSP_PS_SERVER_NOT_RUNNING,
      "Although the CommandRunner object is active and no protocol "
      "error has been reported, CR indicates that the child is not "
      "running.  Stop+start the server to fix things.");
  }

  if (m_initializeRequestID) {
    return LSPAnnotatedProtocolState(
      LSP_PS_INITIALIZING,
      stringb(
        "The \"initialize\" request has been sent (ID=" <<
        m_initializeRequestID << ") but is outstanding."));
  }

  else if (m_shutdownRequestID) {
    return LSPAnnotatedProtocolState(
      LSP_PS_SHUTDOWN1,
      stringb(
        "The \"shutdown\" request has been sent (ID=" <<
        m_shutdownRequestID << ") but is outstanding."));
  }

  else if (m_waitingForTermination) {
    return LSPAnnotatedProtocolState(
      LSP_PS_SHUTDOWN2,
      "The \"exit\" notification has been sent, but the server "
      "process has not yet terminated.");
  }

  else {
    return LSPAnnotatedProtocolState(
      LSP_PS_NORMAL,
      "The LSP server is running normally.");
  }
}


bool LSPManager::isRunningNormally() const
{
  // This set of conditions must be kept synchronized with the code in
  // `getAnnotatedProtocolState`.
  return
    m_commandRunner &&
    m_lsp &&
    !m_lsp->hasProtocolError() &&
    m_lsp->isChildRunning() &&
    !m_initializeRequestID &&
    !m_shutdownRequestID &&
    !m_waitingForTermination &&
    true;
}


std::string LSPManager::explainAbnormality() const
{
  // This is less about debugging than informing, so it does not include
  // the symbolic name of the protocol state.
  LSPAnnotatedProtocolState aps = getAnnotatedProtocolState();
  return aps.m_description;
}


bool LSPManager::isFileOpen(std::string const &fname) const
{
  xassertPrecondition(isValidLSPPath(fname));
  return contains(m_documentInfo, fname);
}


std::set<std::string> LSPManager::getOpenFileNames() const
{
  return mapKeySet(m_documentInfo);
}


RCSerf<LSPDocumentInfo const> LSPManager::getDocInfo(
  std::string const &fname) const
{
  xassertPrecondition(isValidLSPPath(fname));

  auto it = m_documentInfo.find(fname);
  if (it == m_documentInfo.end()) {
    return nullptr;
  }
  else {
    return &( (*it).second );
  }
}


void LSPManager::notify_textDocument_didOpen(
  std::string const &fname,
  std::string const &languageId,
  int version,
  std::string &&contents)
{
  xassertPrecondition(isRunningNormally());
  xassertPrecondition(isValidLSPPath(fname));
  xassertPrecondition(!isFileOpen(fname));

  TRACE1("Sending didOpen for " << doubleQuote(fname) <<
         " with initial version " << version << ".");

  m_lsp->sendNotification("textDocument/didOpen", GDVMap{
    {
      "textDocument",
      GDVMap{
        { "uri", makeFileURI(fname) },
        { "languageId", languageId },
        { "version", version },
        { "text", GDValue(contents /*copy*/) },
      }
    }
  });

  // `emplace` would be better here, but GCC rejects (Clang accepts).
  m_documentInfo.insert(std::make_pair(fname,
    LSPDocumentInfo(fname, version, contents)));

  //m_documentInfo.emplace(fname,
  //  fname, version, std::move(contents));

  // We expect to get diagnostics back for the initial version.
  mapGetValueAt(m_documentInfo, fname).m_waitingForDiagnostics = true;
}


void LSPManager::notify_textDocument_didChange(
  LSP_DidChangeTextDocumentParams const &params)
{
  xassertPrecondition(isRunningNormally());

  std::string fname = params.getFname();
  xassertPrecondition(isFileOpen(fname));

  TRACE1("Sending didChange for " << toGDValue(params.m_textDocument));

  m_lsp->sendNotification("textDocument/didChange", toGDValue(params));

  LSPDocumentInfo &docInfo = mapGetValueAt(m_documentInfo, fname);

  applyLSPDocumentChanges(params, *docInfo.m_lastSentContents);

  docInfo.m_lastSentVersion = params.m_textDocument.m_version;
  docInfo.m_waitingForDiagnostics = true;
}


void LSPManager::notify_textDocument_didChange_all(
  std::string const &fname,
  int version,
  std::string &&contents)
{
  std::list<LSP_TextDocumentContentChangeEvent> changes;
  std::optional<LSP_Range> noRange;
  changes.push_back(LSP_TextDocumentContentChangeEvent(
    std::move(noRange),
    std::move(contents)));

  LSP_DidChangeTextDocumentParams params(
    LSP_VersionedTextDocumentIdentifier::fromFname(
      fname,
      version),
    std::move(changes));

  notify_textDocument_didChange(std::move(params));
}


void LSPManager::notify_textDocument_didClose(
  std::string const &fname)
{
  xassertPrecondition(isRunningNormally());
  xassertPrecondition(isFileOpen(fname));

  TRACE1("Sending didClose for " << doubleQuote(fname) << ".");
  m_lsp->sendNotification("textDocument/didClose", GDVMap{
    {
      "textDocument",
      GDVMap{
        { "uri", makeFileURI(fname) },
      }
    },
  });

  mapRemoveExisting(m_documentInfo, fname);
  xassert(!isFileOpen(fname));
}


bool LSPManager::hasPendingDiagnostics() const
{
  return !m_filesWithPendingDiagnostics.empty();
}


bool LSPManager::hasPendingDiagnosticsFor(std::string const &fname) const
{
  xassertPrecondition(isValidLSPPath(fname));
  return setContains(m_filesWithPendingDiagnostics, fname);
}


std::string LSPManager::getFileWithPendingDiagnostics() const
{
  xassertPrecondition(hasPendingDiagnostics());
  auto it = m_filesWithPendingDiagnostics.cbegin();
  return *it;
}


std::unique_ptr<LSP_PublishDiagnosticsParams>
LSPManager::takePendingDiagnosticsFor(std::string const &fname)
{
  xassertPrecondition(hasPendingDiagnosticsFor(fname));

  LSPDocumentInfo &docInfo = mapGetValueAt(m_documentInfo, fname);
  xassert(docInfo.hasPendingDiagnostics());

  setRemoveExisting(m_filesWithPendingDiagnostics, fname);

  return std::move(docInfo.m_pendingDiagnostics);
}


bool LSPManager::hasPendingErrorMessages() const
{
  return !m_pendingErrorMessages.empty();
}


int LSPManager::numPendingErrorMessages() const
{
  return safeToInt(m_pendingErrorMessages.size());
}


std::string LSPManager::takePendingErrorMessage()
{
  xassert(hasPendingErrorMessages());
  return listMoveFront(m_pendingErrorMessages);
}


int LSPManager::requestRelatedLocation(
  LSPSymbolRequestKind lsrk,
  std::string const &fname,
  TextMCoord position)
{
  xassertPrecondition(isRunningNormally());
  xassertPrecondition(isFileOpen(fname));

  char const *requestName = toRequestName(lsrk);

  return sendRequest(requestName, GDVMap{
    {
      "textDocument",
      GDVMap{
        { "uri", makeFileURI(fname) },
      }
    },
    {
      "position",
      GDVMap{
        { "line", position.m_line },
        { "character", position.m_byteIndex },
      }
    },
  });
}


int LSPManager::sendRequest(
  std::string const &method,
  gdv::GDValue const &params)
{
  xassertPrecondition(isRunningNormally());

  TRACE1("Sending request " << doubleQuote(method) <<
         ": " << params.asIndentedString());
  return m_lsp->sendRequest(method, params);
}


bool LSPManager::hasReplyForID(int id) const
{
  xassertPrecondition(isRunningNormally());
  return m_lsp->hasReplyForID(id);
}


gdv::GDValue LSPManager::takeReplyForID(int id)
{
  xassertPrecondition(isRunningNormally());
  xassertPrecondition(hasReplyForID(id));

  GDValue ret = m_lsp->takeReplyForID(id);
  TRACE2("reply " << id << ": " << ret.asIndentedString());

  return ret;
}


void LSPManager::cancelRequestWithID(int id)
{
  xassertPrecondition(isRunningNormally());
  m_lsp->cancelRequestWithID(id);
}


// EOF
