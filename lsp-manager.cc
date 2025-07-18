// lsp-manager.cc
// Code for `lsp-manger.h`.

#include "lsp-manager.h"               // this module

#include "command-runner.h"            // CommandRunner
#include "lsp-data.h"                  // LSP_PublishDiagnosticsParams
#include "lsp-client.h"                // LSPClient

#include "smqtutil/qtutil.h"           // toString(QString)

#include "smbase/container-util.h"     // smbase::contains
#include "smbase/exc.h"                // GENERIC_CATCH_BEGIN/END
#include "smbase/gdvalue-parser.h"     // gdv::GDValueParser
#include "smbase/gdvalue-set.h"        // gdv::toGDValue(std::set)
#include "smbase/gdvalue.h"            // gdv::GDValue
#include "smbase/list-util.h"          // smbase::listMoveFront
#include "smbase/overflow.h"           // safeToInt
#include "smbase/sm-file-util.h"       // SMFileUtil
#include "smbase/sm-trace.h"           // INIT_TRACE, etc.
#include "smbase/string-util.h"        // join
#include "smbase/stringb.h"            // stringb

#include <memory>                      // std::make_unique
#include <string>                      // std::string
#include <utility>                     // std::move
#include <vector>                      // std::vector


using namespace gdv;
using namespace smbase;


INIT_TRACE("lsp-manager");


// ------------------------- LSPProtocolState --------------------------
DEFINE_ENUMERATION_TO_STRING_OR(
  LSPProtocolState,
  NUM_LSP_PROTOCOL_STATES,
  (
    "LSP_PS_MANAGER_INACTIVE",
    "LSP_PS_PROTOCOL_OBJECT_MISSING",
    "LSP_PS_PROTOCOL_ERROR",
    "LSP_PS_SERVER_NOT_RUNNING",
    "LSP_PS_INITIALIZING",
    "LSP_PS_SHUTDOWN1",
    "LSP_PS_SHUTDOWN2",
    "LSP_PS_NORMAL",
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


// -------------------------- LSPDocumentInfo --------------------------
LSPDocumentInfo::~LSPDocumentInfo()
{}


LSPDocumentInfo::LSPDocumentInfo(
  std::string const &fname,
  int latestVersion)
  : m_fname(fname),
    m_latestVersion(latestVersion)
{}


// ---------------------------- LSPManager -----------------------------
void LSPManager::resetProtocolState()
{
  m_initializeRequestID = 0;
  m_shutdownRequestID = 0;
  m_waitingForTermination = false;
  m_documentInfo.clear();
  m_pendingDiagnostics.clear();
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
    m_commandRunner->killProcess();
    m_commandRunner.reset();
  }

  resetProtocolState();
}


void LSPManager::addErrorMessage(std::string &&msg)
{
  m_pendingErrorMessages.push_back(std::move(msg));
  Q_EMIT signal_hasPendingErrorMessages();
}


void LSPManager::on_hasPendingNotifications() NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  while (m_lsp->hasPendingNotifications()) {
    GDValue msg = m_lsp->takeNextNotification();
    TRACE1("received notification: " << msg.asString());

    GDValueParser msgParser(msg);

    try {
      msgParser.checkIsMap();
      std::string method = msgParser.mapGetValueAtStr("method").stringGet();
      GDValueParser params = msgParser.mapGetValueAtStr("params");
      params.checkIsMap();

      if (method == "textDocument/publishDiagnostics") {
        m_pendingDiagnostics.push_back(
          std::make_unique<LSP_PublishDiagnosticsParams>(
            params));
        Q_EMIT signal_hasPendingDiagnostics();
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
    m_lsp->takeReplyForID(id);
    m_initializeRequestID = 0;

    // Send "initialized" to complete the startup procedure.  There is
    // no reply to this so we simply assume we're ready now.
    m_lsp->sendNotification("initialized", GDVMap{});
  }

  else if (id == m_shutdownRequestID) {
    TRACE1("received shutdown reply");
    m_lsp->takeReplyForID(id);
    m_shutdownRequestID = 0;

    // Now, we send the "exit" notification, which should cause the
    // server process to terminate.
    m_lsp->sendNotification("exit", GDVMap{});
    m_waitingForTermination = true;
  }

  else {
    TRACE1("received reply with ID " << id);
    m_lsp->takeReplyForID(id);
    // TODO: Arrange for the client to be able to submit their own
    // requests and receive the replies.
  }

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


LSPManager::~LSPManager()
{
  forciblyShutDown();
}


LSPManager::LSPManager(
  bool useRealClangd,
  std::string lspStderrLogFname)
  : m_useRealClangd(useRealClangd),
    m_lspStderrLogFname(
      SMFileUtil().normalizePathSeparators(lspStderrLogFname)),
    m_commandRunner(),
    m_lsp(),
    m_sfu(),
    m_initializeRequestID(0),
    m_shutdownRequestID(0),
    m_waitingForTermination(false),
    m_documentInfo(),
    m_pendingDiagnostics(),
    m_pendingErrorMessages()
{}


void LSPManager::selfCheck() const
{
  // Either both are present or neither is.
  xassert(m_commandRunner.operator bool() ==
          m_lsp.operator bool());

  // The map keys agree with the associated values.
  for (auto const &kv : m_documentInfo) {
    xassert(kv.first == kv.second.m_fname);
  }
}


std::string LSPManager::startServer(bool /*OUT*/ &success)
{
  success = false;

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
  }
  else {
    // Need to use `env` due to cygwin symlink issues.
    m_commandRunner->setProgram("env");
    m_commandRunner->setArguments(QStringList() <<
      "python3" << "./lsp-test-server.py");
  }

  SMFileUtil().createParentDirectories(m_lspStderrLogFname);
  m_commandRunner->setStandardErrorFile(toQString(m_lspStderrLogFname));

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


  // ---- Start the LSP protocol communicator ----
  m_lsp.reset(new LSPClient(*m_commandRunner));

  // Connect the signals.
  QObject::connect(m_lsp.get(), &LSPClient::signal_hasPendingNotifications,
                   this,           &LSPManager::on_hasPendingNotifications);
  QObject::connect(m_lsp.get(), &LSPClient::signal_hasReplyForID,
                   this,           &LSPManager::on_hasReplyForID);
  QObject::connect(m_lsp.get(), &LSPClient::signal_childProcessTerminated,
                   this,           &LSPManager::on_childProcessTerminated);

  // Kick off the initialization process.
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

    { "capabilities", GDVMap{} },
  });

  success = true;
  return stringb("Server started.  Server PID is " <<
                 m_commandRunner->getChildPID() << ".");
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
      "There was a protocol error, so server was killed: " <<
      m_lsp->getProtocolError());
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
    m_shutdownRequestID = m_lsp->sendRequest("shutdown", GDVMap{});
    msgs.push_back("Initiated server shutdown.");
  }

  return join(msgs, "\n");
}


std::string LSPManager::checkStatus()
{
  LSPAnnotatedProtocolState aps = getAnnotatedProtocolState();

  // Start with the protocol state.
  std::vector<std::string> msgs;
  msgs.push_back(stringb("Protocol state: " << toString(aps.m_protocolState)));
  msgs.push_back(aps.m_description);

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

  // Dequeue any error messages.
  if (int n = numPendingErrorMessages()) {
    msgs.push_back(stringb(
      "There are " << n << " pending error messages:"));

    int i = 0;
    while (hasPendingErrorMessages()) {
      msgs.push_back(stringb(
        "  " << ++i << ": " << takePendingErrorMessage()));
    }
  }

  msgs.push_back(stringb(
    "Server stderr is in " << doubleQuote(m_lspStderrLogFname) << "."));

  return join(msgs, "\n");
}


LSPProtocolState LSPManager::getProtocolState() const
{
  return getAnnotatedProtocolState().m_protocolState;
}


LSPAnnotatedProtocolState LSPManager::getAnnotatedProtocolState() const
{
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
        "There was a protocol error, please restart: " <<
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


bool LSPManager::isFileOpen(std::string const &fname) const
{
  xassert(m_sfu.isAbsolutePath(fname));
  return contains(m_documentInfo, fname);
}


void LSPManager::notify_textDocument_didOpen(
  std::string const &fname,
  std::string const &languageId,
  int version,
  std::string &&contents)
{
  xassert(!isFileOpen(fname));

  m_lsp->sendNotification("textDocument/didOpen", GDVMap{
    {
      "textDocument",
      GDVMap{
        { "uri", LSPClient::makeFileURI(fname) },
        { "languageId", languageId },
        { "version", version },
        { "text", GDValue(std::move(contents)) },
      }
    }
  });

  m_documentInfo.insert({fname,
    LSPDocumentInfo(fname, version)});
}


bool LSPManager::hasPendingDiagnostics() const
{
  return !m_pendingDiagnostics.empty();
}


std::unique_ptr<LSP_PublishDiagnosticsParams>
LSPManager::takePendingDiagnostics()
{
  xassert(hasPendingDiagnostics());
  return listMoveFront(m_pendingDiagnostics);
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


// EOF
