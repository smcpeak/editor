// lsp-manager-test.cc
// Tests for `lsp-manager`.

#include "lsp-manager-test.h"          // this module
#include "unit-tests.h"                // decl for my entry point

#include "lsp-manager.h"               // module under test

#include "lsp-conv.h"                  // convertLSPDiagsToTDD
#include "lsp-data.h"                  // LSP_PublishDiagnosticsParams
#include "lsp-symbol-request-kind.h"   // LSPSymbolRequestKind
#include "td-core.h"                   // TextDocumentCore
#include "td-diagnostics.h"            // TextDocumentDiagnostics
#include "td-obs-recorder.h"           // TextDocumentObservationRecorder
#include "uri-util.h"                  // makeFileURI

#include "smqtutil/qtutil.h"           // waitForQtEvent

#include "smbase/gdvalue.h"            // gdv::toGDValue
#include "smbase/overflow.h"           // safeToInt
#include "smbase/sm-env.h"             // smbase::envAsBool
#include "smbase/sm-file-util.h"       // SMFileUtil
#include "smbase/sm-macros.h"          // OPEN_ANONYMOUS_NAMESPACE
#include "smbase/sm-test.h"            // DIAG
#include "smbase/sm-trace.h"           // INIT_TRACE, etc.
#include "smbase/string-util.h"        // stringVectorFromPointerArray
#include "smbase/xassert.h"            // xassert

#include <memory>                      // std::unique_ptr
#include <string>                      // std::string
#include <vector>                      // std::vector


using namespace gdv;
using namespace smbase;


INIT_TRACE("lsp-manager-test");


LSPManagerTester::~LSPManagerTester()
{
  disconnectSignals();
}


LSPManagerTester::LSPManagerTester(
  LSPTestRequestParams const &params,
  std::ostream * NULLABLE protocolDiagnosticLog)
  : m_lspManager(
      params.m_useRealClangd,
      "out/lsp-manager-test-server-stderr.txt",
      protocolDiagnosticLog),
    m_params(params),
    m_done(false),
    m_failed(false),
    m_declarationRequestID(0),
    m_doc()
{
  m_doc.replaceWholeFileString(m_params.m_fileContents);

  // I do not connect the signals here because the synchronous tests are
  // meant to run without using signals.
}


void LSPManagerTester::startServer()
{
  m_lspManager.selfCheck();
  xassert(m_lspManager.getProtocolState() == LSP_PS_MANAGER_INACTIVE);

  bool success;
  std::string startResult = m_lspManager.startServer(success);
  DIAG("Start: " << startResult)
  xassert(success);

  DIAG("Status: " << m_lspManager.checkStatus());
  m_lspManager.selfCheck();

  DIAG("Initializing...");
}


void LSPManagerTester::sendDidOpen()
{
  DIAG("Sending didOpen...");
  m_lspManager.notify_textDocument_didOpen(
    m_params.m_fname,
    "cpp",
    m_doc.getVersionNumber(),
    m_doc.getWholeFileString());
  DIAG("Status: " << m_lspManager.checkStatus());
  m_lspManager.selfCheck();

  EXPECT_EQ(
    m_lspManager.getDocInfo(m_params.m_fname)->m_waitingForDiagnostics,
    true);

  DIAG("Waiting for diagnostics notification...");
}


std::unique_ptr<LSP_PublishDiagnosticsParams>
LSPManagerTester::takeDiagnostics()
{
  std::unique_ptr<LSP_PublishDiagnosticsParams> diags =
    m_lspManager.takePendingDiagnosticsFor(
      m_lspManager.getFileWithPendingDiagnostics());
  DIAG("Diagnostics: " << toGDValue(*diags).asIndentedString());

  EXPECT_EQ(
    m_lspManager.getDocInfo(m_params.m_fname)->m_waitingForDiagnostics,
    false);

  return diags;
}


void LSPManagerTester::sendDeclarationRequest()
{
  xassert(m_declarationRequestID == 0);

  DIAG("Sending declaration request...");
  m_declarationRequestID =
    m_lspManager.requestRelatedLocation(
      LSPSymbolRequestKind::K_DECLARATION,
      m_params.m_fname,
      TextMCoord(m_params.m_line, m_params.m_col));
  m_lspManager.selfCheck();

  DIAG("Status: " << m_lspManager.checkStatus());

  // The ID should not be pending yet.
  xassert(!m_lspManager.hasReplyForID(m_declarationRequestID));

  DIAG("Declaration request ID is " << m_declarationRequestID <<
       "; awaiting reply.");
}


void LSPManagerTester::takeDeclarationReply()
{
  xassert(m_lspManager.hasReplyForID(m_declarationRequestID));
  GDValue reply = m_lspManager.takeReplyForID(m_declarationRequestID);
  xassert(!m_lspManager.hasReplyForID(m_declarationRequestID));
  m_lspManager.selfCheck();

  DIAG("Declaration reply: " << reply.asIndentedString());
  DIAG("Status: " << m_lspManager.checkStatus());
}


void LSPManagerTester::stopServer()
{
  std::string stopResult = m_lspManager.stopServer();
  DIAG("Stop: " << stopResult);

  DIAG("Status: " << m_lspManager.checkStatus());
  m_lspManager.selfCheck();

  DIAG("Waiting for shutdown...");
}


void LSPManagerTester::acknowledgeShutdown()
{
  DIAG("Stopped.");
  m_lspManager.selfCheck();
}


void LSPManagerTester::testSynchronously()
{
  startServer();

  while (m_lspManager.getProtocolState() != LSP_PS_NORMAL) {
    waitForQtEvent();
    TRACE1("Status: " << m_lspManager.checkStatus());
    m_lspManager.selfCheck();
  }

  sendDidOpen();

  while (!m_lspManager.hasPendingDiagnostics()) {
    waitForQtEvent();
    TRACE1("Status: " << m_lspManager.checkStatus());
    m_lspManager.selfCheck();
  }

  std::unique_ptr<LSP_PublishDiagnosticsParams> diags =
    takeDiagnostics();

  sendDeclarationRequest();

  while (!m_lspManager.hasReplyForID(m_declarationRequestID)) {
    waitForQtEvent();
    TRACE1("Status: " << m_lspManager.checkStatus());
    m_lspManager.selfCheck();
  }

  takeDeclarationReply();

  syncCheckDocumentContents();

  // Experiment with incremental edits.
  {
    // Get the manager's view of the document.
    RCSerf<LSPDocumentInfo const> docInfo =
      m_lspManager.getDocInfo(m_params.m_fname);
    xassert(docInfo->getLastSentContentsString() ==
            m_doc.getWholeFileString());

    // Set up a recorder for our copy.
    TextDocumentObservationRecorder recorder(m_doc);
    recorder.beginTrackingCurrentDoc();
    recorder.applyChangesToDiagnostics(
      convertLSPDiagsToTDD(diags.get()).get());
    xassert(recorder.isTracking(m_doc.getVersionNumber()));

    // Make a sample edit.
    m_doc.insertText(TextMCoord(5, 0), "hello", 5);

    // Get the recorded changes.
    TextDocumentChangeObservationSequence const &recordedChanges =
      recorder.getUnsentChanges();

    // Convert changes to the LSP format and package them into a
    // "didChange" params structure.
    LSP_DidChangeTextDocumentParams changeParams(
      LSP_VersionedTextDocumentIdentifier::fromFname(
        m_params.m_fname, m_doc.getVersionNumber()),
      convertRecordedChangesToLSPChanges(recordedChanges));

    // Send them to the server, and have the manager update its copy.
    m_lspManager.notify_textDocument_didChange(changeParams);

    // Check the manager's copy.
    xassert(docInfo->getLastSentContentsString() ==
            m_doc.getWholeFileString());

    // The recorder must also know this was sent.
    recorder.beginTrackingCurrentDoc();

    // Wait for the server to send diagnostics for the new version.
    while (!m_lspManager.hasPendingDiagnostics()) {
      waitForQtEvent();
      TRACE1("Status: " << m_lspManager.checkStatus());
      m_lspManager.selfCheck();
    }

    // Incorporate the reply.
    diags = takeDiagnostics();
    recorder.applyChangesToDiagnostics(
      convertLSPDiagsToTDD(diags.get()).get());

    // Now ask the server what it thinks the document looks like.
    syncCheckDocumentContents();

    // TODO: Do more edits.
  }

  stopServer();

  while (m_lspManager.getProtocolState() != LSP_PS_MANAGER_INACTIVE) {
    waitForQtEvent();
    TRACE1("Status: " << m_lspManager.checkStatus());
    m_lspManager.selfCheck();
  }

  acknowledgeShutdown();
}


void LSPManagerTester::syncCheckDocumentContents()
{
  DIAG("Sending getTextDocumentContents request");
  GDValue params(GDVMap{
    { "textDocument", GDVMap {
      { "uri", makeFileURI(m_params.m_fname) },
      { "version", m_doc.getVersionNumber() },
    }},
  });
  int id = m_lspManager.sendRequest(
    "$/getTextDocumentContents", params);

  // Wait for the reply.
  DIAG("Waiting for getTextDocumentContents reply, id=" << id);
  while (!m_lspManager.hasReplyForID(id)) {
    waitForQtEvent();
    TRACE1("Status: " << m_lspManager.checkStatus());
    m_lspManager.selfCheck();
  }

  GDValue reply = m_lspManager.takeReplyForID(id);

  std::string text = reply.mapGetValueAt("text").stringGet();
  xassert(text == m_doc.getWholeFileString());

  int version = reply.mapGetValueAt("version").smallIntegerGet();
  xassert(version == safeToInt(m_doc.getVersionNumber()));
}


void LSPManagerTester::connectSignals()
{
  QObject::connect(
    &m_lspManager, &LSPManager::signal_changedProtocolState,
    this,        &LSPManagerTester::on_changedProtocolState);
  QObject::connect(
    &m_lspManager, &LSPManager::signal_hasPendingDiagnostics,
    this,        &LSPManagerTester::on_hasPendingDiagnostics);
  QObject::connect(
    &m_lspManager, &LSPManager::signal_hasReplyForID,
    this,        &LSPManagerTester::on_hasReplyForID);
  QObject::connect(
    &m_lspManager, &LSPManager::signal_hasPendingErrorMessages,
    this,        &LSPManagerTester::on_hasPendingErrorMessages);
}


void LSPManagerTester::disconnectSignals()
{
  QObject::disconnect(&m_lspManager, nullptr, this, nullptr);
}


void LSPManagerTester::testAsynchronously()
{
  connectSignals();

  startServer();

  xassert(m_lspManager.getProtocolState() == LSP_PS_INITIALIZING);

  // The immediate next state is `LSP_PS_NORMAL`.

  // Meanwhile, pump the event queue until we are completely done.
  while (!m_done && !m_failed) {
    waitForQtEvent();
    //DIAG("Status: " << m_lspManager.checkStatus());
    m_lspManager.selfCheck();
  }

  acknowledgeShutdown();

  // This is also (harmlessly redundantly) done in the destructor.
  disconnectSignals();

  xassert(!m_failed);
}


void LSPManagerTester::on_changedProtocolState() NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  LSPProtocolState state = m_lspManager.getProtocolState();

  DIAG("changedProtocolState to: " << toString(state));

  switch (state) {
    default:
      // Ignore.
      break;

    case LSP_PS_NORMAL:
      sendDidOpen();
      // Await the diagnostics notification.
      break;

    case LSP_PS_MANAGER_INACTIVE:
      m_done = true;
      break;
  }

  GENERIC_CATCH_END
}


void LSPManagerTester::on_hasPendingDiagnostics() NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  takeDiagnostics();

  sendDeclarationRequest();

  GENERIC_CATCH_END
}


void LSPManagerTester::on_hasReplyForID(int id) NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  if (id == m_declarationRequestID) {
    DIAG("Received diagnostic reply ID " << id);

    takeDeclarationReply();

    stopServer();

    // Await `LSP_PS_MANAGER_INACTIVE`.
  }

  else {
    DIAG("Received unexpected reply ID " << id);
    m_failed = true;
  }

  GENERIC_CATCH_END
}


void LSPManagerTester::on_hasPendingErrorMessages() NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  DIAG("LSPManager reports errors.  Status:");
  DIAG(m_lspManager.checkStatus());

  m_failed = true;

  GENERIC_CATCH_END
}


// Called from unit-tests.cc.
void test_lsp_manager(CmdlineArgsSpan args)
{
  SMFileUtil().createDirectoryAndParents("out");

  LSPTestRequestParams params =
    LSPTestRequestParams::getFromCmdLine(args);

  VPVAL(params.m_fname);
  VPVAL(params.m_line);
  VPVAL(params.m_col);
  VPVAL(params.m_useRealClangd);

  {
    DIAG("-------- synchronous --------");
    LSPManagerTester tester(params, &std::cout);
    tester.testSynchronously();
  }

  if (!envAsBool("SYNC_ONLY")) {
    DIAG("-------- asynchronous --------");
    LSPManagerTester tester(params, &std::cout);
    tester.testAsynchronously();
  }
}


// EOF
