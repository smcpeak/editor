// editor-global.cc
// code for editor-global.cc, and application main() function

#include "editor-global.h"                       // this module

// editor
#include "apply-command-dialog.h"                // ApplyCommandDialog
#include "command-runner.h"                      // CommandRunner
#include "connections-dialog.h"                  // ConnectionsDialog
#include "diagnostic-details-dialog.h"           // DiagnosticDetailsDialog
#include "editor-command.ast.gen.h"              // EditorCommand
#include "editor-navigation-options.h"           // EditorNavigationOptions
#include "editor-proxy-style.h"                  // EditorProxyStyle
#include "editor-version.h"                      // getEditorVersionString
#include "editor-widget.h"                       // EditorWidget
#include "editor-window.h"                       // EditorWindow
#include "event-recorder.h"                      // EventRecorder
#include "event-replay.h"                        // EventReplay
#include "json-rpc-reply.h"                      // JSON_RPC_Reply
#include "keybindings.doc.gen.h"                 // doc_keybindings
#include "line-index.h"                          // LineIndex
#include "lsp-conv.h"                            // convertLSPDiagsToTDD, toLSP_VersionNumber, lspSendUpdatedContents
#include "lsp-data.h"                            // LSP_PublishDiagnosticsParams
#include "lsp-get-code-lines.h"                  // lspGetCodeLinesFunction
#include "lsp-manager.h"                         // LSPManager, LSPDocumentInfo
#include "open-files-dialog.h"                   // OpenFilesDialog
#include "process-watcher.h"                     // ProcessWatcher
#include "recent-items-list.h"                   // RecentItemsList
#include "td-diagnostics.h"                      // TextDocumentDiagnostics (implicit)
#include "textinput.h"                           // TextInputDialog
#include "uri-util.h"                            // getFileURIPath
#include "vfs-query-sync.h"                      // readFileSynchronously

// smqtutil
#include "smqtutil/gdvalue-qt.h"                 // toGDValue(QString)
#include "smqtutil/qstringb.h"                   // qstringb
#include "smqtutil/qtguiutil.h"                  // showRaiseAndActivateWindow, messageBox
#include "smqtutil/qtutil.h"                     // toQString
#include "smqtutil/sync-wait.h"                  // SynchronousWaiter
#include "smqtutil/timer-event-loop.h"           // sleepWhilePumpingEvents

// smbase
#include "smbase/chained-cond.h"                 // smbase::cc::z_le_lt
#include "smbase/datetime.h"                     // localTimeString
#include "smbase/dev-warning.h"                  // g_devWarningHandler
#include "smbase/either.h"                       // smbase::Either
#include "smbase/exc.h"                          // smbase::{XBase, xformat}, EXN_CONTEXT
#include "smbase/exclusive-write-file.h"         // smbase::ExclusiveWriteFile
#include "smbase/gdv-ordered-map.h"              // gdv::GDVOrderedMap
#include "smbase/gdvalue-parser.h"               // gdv::GDValueParser
#include "smbase/gdvalue-set.h"                  // gdv::toGDValue(std::set)
#include "smbase/gdvalue-vector.h"               // gdv::toGDValue(std::vector)
#include "smbase/gdvalue.h"                      // gdv::toGDValue
#include "smbase/objcount.h"                     // CheckObjectCount
#include "smbase/save-restore.h"                 // SET_RESTORE, SetRestore
#include "smbase/set-util.h"                     // smbase::{setInsertUnique, setContains}
#include "smbase/sm-env.h"                       // smbase::{getXDGConfigHome, getXDGStateHome, envAsIntOr, envAsBool}
#include "smbase/sm-file-util.h"                 // SMFileUtil
#include "smbase/sm-is-equal.h"                  // smbase::is_equal
#include "smbase/sm-test.h"                      // PVAL
#include "smbase/string-util.h"                  // beginsWith, shellDoubleQuoteCommand
#include "smbase/stringb.h"                      // stringb
#include "smbase/strtokp.h"                      // StrtokParse
#include "smbase/sm-trace.h"                     // INIT_TRACE, etc.
#include "smbase/xassert-eq-container.h"         // XASSERT_EQUAL_SETS

// Qt
#include <QKeyEvent>
#include <QMenuBar>
#include <QMessageBox>
#include <QShortcutEvent>
#include <QStyleFactory>

// libc++
#include <algorithm>                             // std::max
#include <cstring>                               // std::{strlen, memcpy}
#include <deque>                                 // std::deque
#include <exception>                             // std::exception
#include <memory>                                // std::{unique_ptr, make_unique}
#include <iostream>                              // std::endl
#include <optional>                              // std::optional
#include <string>                                // std::string
#include <utility>                               // std::move

// libc
#include <stdlib.h>                              // atoi

using namespace gdv;
using namespace smbase;


// Tracing levels:
//   1. File or app
//   2. Keystroke
INIT_TRACE("editor-global");


// --------------------------- EditorGlobal ----------------------------
char const EditorGlobal::appName[] = "Editor";

int const EditorGlobal::MAX_NUM_RECENT_COMMANDS = 100;


EditorGlobal::EditorGlobal(int argc, char **argv)
  : QApplication(argc, argv),
    m_pixmaps(),
    m_documentList(),
    m_editorWindows(),
    m_editorLogFile(openEditorLogFile()),

    // This can also be set during command line processing.
    m_lspIsFakeServer(envAsBool("USE_FAKE_LSP_SERVER")),

    m_lspManager(),          // Set to non-null below.
    m_lspErrorMessages(),
    m_windowCounter(1),
    m_editorBuiltinFont(BF_EDITOR14),
    m_vfsConnections(),
    m_processes(),
    m_openFilesDialog(),
    m_applyCommandDialogs(),
    m_connectionsDialog(),
    m_diagnosticDetailsDialog(),
    m_recentCommands(),
    m_settings(),
    m_doNotSaveSettings(false),
    m_filenameInputDialogHistory(),
    m_recordInputEvents(false),
    m_eventFileTest()
{
  m_documentList.addObserver(this);

  // Optionally print the list of styles Qt supports.
  if (envAsBool("PRINT_QT_STYLES")) {
    QStringList keys = QStyleFactory::keys();
    cout << "style keys:\n";
    for (int i=0; i < keys.size(); i++) {
      cout << "  " << keys.at(i).toUtf8().constData() << endl;
    }
    QStyle *defaultStyle = this->style();
    cout << "default style: " << toString(defaultStyle->objectName()) << endl;
  }

  // Activate my own modification to the Qt style.  This works even
  // if the user overrides the default style, for example, by passing
  // "-style Windows" on the command line.
  //
  // Note that `QApplication` takes ownership of the style object, so
  // this is not a memory leak.
  this->setStyle(new EditorProxyStyle);

  // Choose the app font size.  For now the UI is very crude.
  {
    int fontSize = envAsIntOr(12, "EDITOR_APP_FONT_POINT_SIZE");
    QFont fontSpec = QApplication::font();
    TRACE1("setting app font point size to " << fontSize);
    fontSpec.setPointSize(fontSize);
    QApplication::setFont(fontSpec);
  }

  if (getenv("EDITOR_USE_LARGE_FONT")) {
    m_editorBuiltinFont = BF_COURIER24;
  }

  // Processing the command line.  Do this relatively early so it can
  // influence how `m_lspManager` is created.
  std::vector<std::string> filesToOpen;
  try {
    filesToOpen = processCommandLineOptions(argc, argv);
  }
  catch (...) {
    // Errors in command line processing are communicated with
    // exceptions, which we allow to propagate, after first shutting
    // down the connections in order to avoid spurious additional
    // complaints.
    m_vfsConnections.shutdownAll();
    throw;
  }

  // Do this after setting the font since it depends on it.
  installEditorStyleSheet(*this);

  // Establish the initial VFS connection before creating the first
  // EditorWindow, since the EW can issue VFS requests.
  QObject::connect(&m_vfsConnections, &VFS_Connections::signal_vfsFailed,
                   this, &EditorGlobal::on_vfsConnectionFailed);
  m_vfsConnections.connectLocal();

  // Create the LSP manager after processing the command line so the
  // effect of setting `m_lspIsFakeServer` in response to "-ev" and
  // "-record" will be effective.
  m_lspManager.reset(new LSPManager(
    !m_lspIsFakeServer /*useRealClangd*/,
    lspGetStderrLogFileInitialName(),
    (m_editorLogFile?
       &(m_editorLogFile->stream()) : nullptr)
  ));

  // Open the first window, initially showing the default "untitled"
  // file that 'fileDocuments' made in its constructor.
  EditorWindow *ed = createNewWindow(m_documentList.getDocumentAt(0));

  // The tests rely on the first window having this name.
  xassert(ed->objectName() == "window1");

  // In the initial window, open all the files from the command line.
  for (std::string const &path : filesToOpen) {
    ed->openOrSwitchToFile(HostAndResourceName::localFile(path));
  }

  // TODO: replacement?  Need to test on Linux.
#if 0
  // this gets the user's preferred initial geometry from
  // the -geometry command line, or xrdb database, etc.
  setMainWidget(ed);
  setMainWidget(NULL);    // but don't remember this
#endif // 0

  // instead, to quit the application, close all of the
  // toplevel windows
  QObject::connect(this, &EditorGlobal::lastWindowClosed,
                   this, &EditorGlobal::quit);

  QObject::connect(this, &EditorGlobal::focusChanged,
                   this, &EditorGlobal::focusChangedHandler);

  lspConnectSignals();

  showRaiseAndActivateWindow(ed);

  // This works around a weird problem with the menu bar, where it will
  // ignore the initially chosen font, but then change itself in
  // response to the *first* font change after startup, after which it
  // resumes ignoring font updates.
  //
  // Experimentation in `gui-tests` suggests this is related to setting
  // the global style sheet (above), which might conflict somehow.
  qApp->setFont(qApp->font());

  selfCheck();
}


EditorGlobal::~EditorGlobal()
{
  // First get rid of the windows so I don't have other entities
  // watching documents and potentially getting confused and/or sending
  // signals I am not prepared for.
  m_editorWindows.deleteAll();

  lspDisconnectSignals();

  if (m_processes.isNotEmpty()) {
    // Now try to kill any running processes.  Do not wait for any of
    // them, among other things because I do not want to get any signals
    // during this loop since then I might modify 'm_processes' during
    // an ongoing iteration.
    FOREACH_OBJLIST_NC(ProcessWatcher, m_processes, iter) {
      ProcessWatcher *watcher = iter.data();
      TRACE1("dtor: killing: " << watcher);
      watcher->m_namedDoc = NULL;
      watcher->m_commandRunner.killProcessNoWait();
    }

    // Wait up to one second for all children to die.  Pump the event
    // queue while waiting so that as they die, I will receive
    // on_processTerminated signals and can reap them and remove them
    // from 'm_processes'.
    for (int waits=0; waits < 10 && m_processes.isNotEmpty(); waits++) {
      TRACE1("dtor: waiting 100ms #" << (waits+1));
      sleepWhilePumpingEvents(100 /*ms*/);
    }

    if (m_processes.isNotEmpty()) {
      // As things stand, this code is nearly impossible to reach
      // because every direct child process is /bin/sh, which is
      // generally quite cooperative.  It might spawn an unkillable
      // grandchild process, but at the moment I have no way to even try
      // to kill those.
      cerr << "Warning: Some child processes could not be killed." << endl;

      // Leak the remaining process objects rather than incurring a 30s
      // hang for each as the QProcess destructor runs.
      while (m_processes.isNotEmpty()) {
        ProcessWatcher *watcher = m_processes.removeFirst();

        // Before letting it go, disconnect my slot.
        // See doc/signals-and-dtors.txt.
        QObject::disconnect(watcher, NULL, this, NULL);

        // Now leak 'watcher'!
      }

      // We know this will cause leaks.  No need to alarm the user.
      CheckObjectCount::s_suppressLeakReports = true;
    }
  }

  m_documentList.removeObserver(this);

  m_vfsConnections.shutdownAll();

  // Disconnect all of the connections made in the constructor.
  // See doc/signals-and-dtors.txt.
  QObject::disconnect(this, 0, this, 0);
  QObject::disconnect(&m_vfsConnections, 0, this, 0);

  m_lspManager.reset();
}


void EditorGlobal::selfCheck() const
{
  EXN_CONTEXT("EditorGlobal::selfCheck");

  m_documentList.selfCheck();

  FOREACH_OBJLIST(EditorWindow, m_editorWindows, iter) {
    iter.data()->selfCheck();
  }

  xassert(m_lspManager);
  m_lspManager->selfCheck();

  // LSP manager and document list agree about what is open.
  if (m_lspManager->isRunningNormally()) {
    std::set<std::string> openLSPFiles =
      m_lspManager->getOpenFileNames();
    std::set<std::string> trackedFiles =
      m_documentList.getTrackingChangesFileNames();
    XASSERT_EQUAL_SETS(openLSPFiles, trackedFiles);
  }

  {
    // Count the LSP-open files we do and do not check, so I can manually
    // confirm nearly all are checked.
    int numChecked = 0;
    int numUnchecked = 0;

    // For all files open with the LSP server, if it is supposed to be up
    // to date in server manager, its copy should agree with the editor's
    // copy.
    for (int index=0; index < numDocuments(); ++index) {
      NamedTextDocument const *ntd = getDocumentByIndexC(index);
      EXN_CONTEXT(ntd->documentName());

      if (RCSerf<LSPDocumentInfo const> docInfo = lspGetDocInfo(ntd)) {
        if (is_equal(docInfo->m_lastSentVersion, ntd->getVersionNumber())) {
          xassert(docInfo->lastContentsEquals(ntd->getCore()));
          ++numChecked;
        }
        else {
          // The manager's version is behind, presumably because
          // continuous update is not enabled.  Don't check anything in
          // this case.
          ++numUnchecked;
        }
      }
    }
    TRACE1_GDVN_EXPRS("EditorGlobal::selfCheck", numChecked, numUnchecked);
  }

  {
    // Collect the set of widgets in all windows.
    std::set<EditorWidget*> allWidgets;
    FOREACH_OBJLIST(EditorWindow, m_editorWindows, iter) {
      setInsertUnique(allWidgets, iter.data()->editorWidget());
    }

    // That should be a superset of `m_recentEditorWidgets`.
    for (RCSerf<EditorWidget> const &recent :
           m_recentEditorWidgets.getListC()) {
      xassert(setContains(allWidgets, recent));
    }

    m_recentEditorWidgets.selfCheck();
  }

  m_vfsConnections.selfCheck();
}


static char const *optionsDescription =
  "options:\n"
  "  -help           Print this message and exit.\n"
  "  -version        Print the version and exit.\n"
  "  -ev=file.ev     Replay events in file.ev for testing.\n"
  "                  (Implies -no-settings.)\n"
  "  -testCommands=tests.gdvn\n"
  "                  Replay all tests in tests.gdvn.\n"
  "  -record         Record events to events.out.\n"
  "  -conn=hostname  Start with an active remote connection to hostname.\n"
  "  -no-settings    Do not read or write user settings.\n"
  "  -fake-lsp       Use `lsp-test-server.py` as the LSP server.\n"
  "\n"
  "With -ev, set envvar NOQUIT=1 to stop if failure and NOQUIT=0 to\n"
  "stop after replay regardless of failure.\n"
  ;


// This exception is thrown to cause the app to exit early.
DEFINE_XBASE_SUBCLASS(QuitAfterPrintingHelp);


std::vector<std::string> EditorGlobal::processCommandLineOptions(
  int argc, char **argv)
{
  // Files to open specified on the command line.
  std::vector<std::string> filesToOpen;

  // True if we will open the user settings file.
  bool useSettings = true;

  SMFileUtil sfu;
  for (int i=1; i < argc; i++) {
    std::string arg(argv[i]);
    if (arg.empty()) {
      xformat("An empty command line argument is not allowed.");
    }

    else if (arg[0]=='-') {
      if (arg == "-help") {
        cout << "usage: " << argv[0] << " [options] [files...]\n\n"
             << optionsDescription;
        THROW(QuitAfterPrintingHelp(""));
      }

      else if (beginsWith(arg, "-ev=")) {
        // Replay a sequence of events as part of a test.
        m_eventFileTest = arg.substr(4, arg.length()-4);

        // We are going to run an automated test, so ignore user
        // settings.
        useSettings = false;

        // Only use the fake server with record/replay
        m_lspIsFakeServer = true;
      }

      else if (arg == "-record") {
        // Record events to seed a new test.
        m_recordInputEvents = true;

        // Since this is preparatory to an automated test, which will
        // not have user settings, turn them off here too.
        useSettings = false;

        // Only use the fake server with record/replay
        m_lspIsFakeServer = true;
      }

      else if (arg == "-fake-lsp") {
        // Allow easily setting this independent of other things.
        m_lspIsFakeServer = true;
      }

      else if (beginsWith(arg, "-conn=")) {
        // Open a connection to a specified host.
        std::string hostName = arg.substr(6, arg.length()-6);
        m_vfsConnections.connect(HostName::asSSH(hostName));
      }

      else if (arg == "-version") {
        cout << getEditorVersionString();    // Has a newline already.
        THROW(QuitAfterPrintingHelp(""));
      }

      else if (arg == "-no-settings") {
        // One reason to use this option is to do interactive
        // preliminary testing or event recording meant as preparation
        // for an automated test.
        useSettings = false;
      }

      // Remember to update the "-help" output after adding a new
      // option.

      else {
        xformat(stringb("Unknown option: " << quoted(arg) <<
                        ".  Try -help."));
      }
    }

    else {
      // Open all non-option files specified on the command line.
      std::string path = sfu.getAbsolutePath(arg);
      path = sfu.normalizePathSeparators(path);
      filesToOpen.push_back(path);
    }
  }

  if (useSettings) {
    loadSettingsFile_throwIfError();
  }
  else {
    // If we did not read the settings, we should not write them either
    // since that would effectively delete them.
    m_doNotSaveSettings = true;
  }

  return filesToOpen;
}


void EditorGlobal::on_vfsConnectionFailed(
  HostName hostName, std::string reason) NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  QMessageBox::warning(nullptr, "Connection Failed", qstringb(
    "The connection to " << hostName << " has failed.  Reads "
    "and writes will not work until this connection is restarted.  "
    "Error message: " << reason));

  GENERIC_CATCH_END
}


// ---------------------- Documents being edited -----------------------
NamedTextDocumentList const *EditorGlobal::documentList() const
{
  return &m_documentList;
}


int EditorGlobal::numDocuments() const
{
  return m_documentList.numDocuments();
}


NamedTextDocument const *EditorGlobal::getDocumentByIndexC(
  int index) const
{
  xassertPrecondition(cc::z_le_lt(index, numDocuments()));
  return m_documentList.getDocumentAtC(index);
}


NamedTextDocument *EditorGlobal::getDocumentByIndex(int index)
{
  return const_cast<NamedTextDocument*>(getDocumentByIndexC(index));
}


NamedTextDocument *EditorGlobal::createNewFile(std::string const &dir)
{
  return m_documentList.createUntitledDocument(dir);
}


NamedTextDocument const * NULLABLE
EditorGlobal::getFileWithNameC(DocumentName const &docName) const
{
  return m_documentList.findDocumentByNameC(docName);
}


NamedTextDocument * NULLABLE
EditorGlobal::getFileWithName(DocumentName &docName)
{
  return const_cast<NamedTextDocument *>(getFileWithNameC(docName));
}


bool EditorGlobal::hasFileWithName(DocumentName const &docName) const
{
  return getFileWithNameC(docName) != nullptr;
}


bool EditorGlobal::hasFileWithTitle(std::string const &title) const
{
  return m_documentList.findDocumentByTitleC(title) != NULL;
}


void EditorGlobal::trackNewDocumentFile(NamedTextDocument *f)
{
  m_documentList.addDocument(f);
}


void EditorGlobal::deleteDocumentFile(NamedTextDocument *file)
{
  lspCloseFile(file);
  m_documentList.removeDocument(file);
  delete file;
}


bool EditorGlobal::hasDocumentFile(NamedTextDocument const *ntd) const
{
  return m_documentList.hasDocument(ntd);
}


void EditorGlobal::makeDocumentTopmost(NamedTextDocument *f)
{
  m_documentList.moveDocument(f, 0);
}


bool EditorGlobal::reloadDocumentFile(QWidget *parentWidget,
                                      NamedTextDocument *doc)
{
  if (doc->hasFilename()) {
    SynchronousWaiter waiter(parentWidget);
    auto replyOrError(
      readFileSynchronously(&m_vfsConnections, waiter, doc->harn()));

    if (std::optional<std::string> errorMsg =
          getROEErrorMessage(replyOrError)) {
      messageBox(parentWidget, "Error", toQString(*errorMsg));
      return false;
    }

    std::unique_ptr<VFS_ReadFileReply> &rfr = replyOrError.left();
    if (!rfr) {
      // User canceled.
      return false;
    }

    // The error case should have been handled above.
    xassert(rfr->m_success);

    {
      // Have widgets ignore the notifications arising from the refresh
      // so their cursor position is not affected.
      SET_RESTORE(EditorWidget::s_ignoreTextDocumentNotificationsGlobally,
        true);

      doc->replaceFileAndStats(rfr->m_contents,
                               rfr->m_fileModificationTime,
                               rfr->m_readOnly);
    }

    // Among other things, we want to let the LSP status indicator
    // update itself to show that the file contents have changed since
    // the last LSP diagnostics were received.
    doc->notifyMetadataChange();
  }

  return true;
}


// ------------------------- Special documents -------------------------
NamedTextDocument * NULLABLE
EditorGlobal::findUntitledUnmodifiedDocument()
{
  return m_documentList.findUntitledUnmodifiedDocument();
}


NamedTextDocument *EditorGlobal::getOrCreateGeneratedDocument(
  std::string const &title,
  std::string const &contents)
{
  DocumentName docName;
  docName.setNonFileResourceName(HostName::asLocal(),
    title, SMFileUtil().currentDirectory());

  NamedTextDocument *doc = m_documentList.findDocumentByName(docName);
  if (!doc) {
    doc = new NamedTextDocument();
    doc->setDocumentName(docName);
    doc->m_title = uniqueTitleFor(docName);
    doc->appendString(contents);
    doc->noUnsavedChanges();
    doc->setReadOnly(true);
    trackNewDocumentFile(doc);
  }
  else {
    // TODO: I think I should reset the document contents here.
  }

  return doc;
}


NamedTextDocument *EditorGlobal::getOrCreateKeybindingsDocument()
{
  return getOrCreateGeneratedDocument(
    "Editor Keybindings",
    std::string(doc_keybindings, sizeof(doc_keybindings)-1));
}


// ---------------------- Multi-document queries -----------------------
std::string EditorGlobal::uniqueTitleFor(DocumentName const &docName) const
{
  return m_documentList.computeUniqueTitle(docName);
}


bool EditorGlobal::getInitialViewForFile(
  NamedTextDocument *ntd,
  NamedTextDocumentInitialView &view /*OUT*/)
{
  return m_documentList.notifyGetInitialView(ntd, view /*OUT*/);
}


void EditorGlobal::getUniqueDocumentDirectories(
  std::vector<HostAndResourceName> &dirs /*INOUT*/) const
{
  m_documentList.getUniqueDirectories(dirs);
}


// -------------------------- Editor windows ---------------------------
int EditorGlobal::numEditorWindows() const
{
  return m_editorWindows.count();
}


EditorWindow *EditorGlobal::createNewWindow(NamedTextDocument *initFile)
{
  EditorWindow *ed = new EditorWindow(this, initFile);
  ed->setObjectName(qstringb("window" << m_windowCounter++));

  // NOTE: caller still has to say 'ed->show()'!

  return ed;
}


void EditorGlobal::registerEditorWindow(EditorWindow *ew)
{
  m_editorWindows.append(ew);
  m_documentList.addObserver(ew);
}


void EditorGlobal::unregisterEditorWindow(EditorWindow *ew)
{
  m_documentList.removeObserver(ew);

  // This object might have already been removed, for example because
  // the EditorGlobal destructor is running, and is in the process of
  // removing elements from the list and destroying them.  Hence the
  // "IfPresent" part of this call.
  m_editorWindows.removeIfPresent(ew);
}


// --------------------------- Notification ----------------------------
void EditorGlobal::notifyDocumentAttributeChanged(
  NamedTextDocument *ntd)
{
  m_documentList.notifyAttributeChanged(ntd);
}


void EditorGlobal::broadcastEditorViewChanged()
{
  FOREACH_OBJLIST_NC(EditorWindow, m_editorWindows, w) {
    w.data()->editorViewChanged();
  }
}


void EditorGlobal::addDocumentListObserver(
  NamedTextDocumentListObserver *observer)
{
  m_documentList.addObserver(observer);
}


void EditorGlobal::removeDocumentListObserver(
  NamedTextDocumentListObserver *observer)
{
  m_documentList.removeObserver(observer);
}


// ---------------------- Running child processes ----------------------
// Return a document that was or will be populated by running 'command'
// in 'dir'.
NamedTextDocument *EditorGlobal::getCommandOutputDocument(
  HostName const &hostName, QString origDir, QString command)
{
  // Create a name based on the command and directory.
  std::string dir = SMFileUtil().stripTrailingDirectorySeparator(toString(origDir));
  std::string base = stringb(dir << "$ " << toString(command));
  DocumentName docName;
  docName.setNonFileResourceName(hostName, base, dir);

  NamedTextDocument *fileDoc = m_documentList.findDocumentByName(docName);
  if (!fileDoc) {
    // Nothing with this name, let's use it to make a new one.
    TRACE1("getCommandOutputDocument: making new document: " << docName);
    NamedTextDocument *newDoc = new NamedTextDocument();
    newDoc->setDocumentName(docName);
    newDoc->m_title = uniqueTitleFor(docName);
    trackNewDocumentFile(newDoc);
    return newDoc;
  }
  else {
    TRACE1("getCommandOutputDocument: reusing existing document: " << docName);
    return fileDoc;
  }
}


NamedTextDocument *EditorGlobal::launchCommand(
  HostName const &hostName,
  QString dir,
  bool prefixStderrLines,
  QString command,
  bool &stillRunning /*OUT*/)
{
  // Find or create a document to hold the result.
  NamedTextDocument *fileDoc =
    this->getCommandOutputDocument(hostName, dir, command);

  if (fileDoc->documentProcessStatus() == DPS_RUNNING) {
    // Just switch to the document with the running program.
    stillRunning = true;
    return fileDoc;
  }
  else {
    stillRunning = false;
  }

  // Remove the existing contents in case we are reusing an existing
  // document.
  fileDoc->clearContentsAndHistory();

  // Show the host, directory, and command at the top of the document.
  // Among other things, this is a helpful acknowledgment that something
  // is happening in case the process does not print anything right away
  // (or at all!).
  fileDoc->appendString(stringb("Hst: " << hostName << '\n'));
  fileDoc->appendString(stringb("Dir: " << toString(dir) << '\n'));
  fileDoc->appendString(stringb("Cmd: " << toString(command) << "\n\n"));

  // Make the watcher that will populate that file.
  ProcessWatcher *watcher = new ProcessWatcher(fileDoc);
  m_processes.prepend(watcher);
  watcher->m_prefixStderrLines = prefixStderrLines;
  QObject::connect(watcher, &ProcessWatcher::signal_processTerminated,
                   this,    &EditorGlobal::on_processTerminated);

  // Interpret the command string as a program and some arguments.
  CommandRunner &cr = watcher->m_commandRunner;
  configureCommandRunner(cr, hostName, dir, command);
  QString fullCommand = cr.getCommandLine();

  // If we are not going to prefix the lines, merge the output channels
  // so the interleaving is temporally accurate.
  if (!prefixStderrLines) {
    cr.mergeStderrIntoStdout();
  }

  // Launch the child process.
  cr.startAsynchronous();

  // Ensure that if the program tries to read from stdin, it will
  // immediately hit EOF rather than hanging.  This must be done *after*
  // starting the process.
  cr.closeInputChannel();

  TRACE1("launchCommand: " << GDValue(GDVOrderedMap{
    GDV_SKV_EXPR(dir),
    GDV_SKV_EXPR(command),
    GDV_SKV_EXPR(fullCommand),
    GDV_SKV_EXPR(fileDoc->documentName())
  }).asIndentedString());

  return fileDoc;
}


void EditorGlobal::configureCommandRunner(
  CommandRunner &cr,
  HostName const &hostName,
  QString dir,
  QString command)
{
  if (hostName.isLocal()) {
    cr.setWorkingDirectory(dir);
    cr.setShellCommandLine(command);
  }
  else {
    cr.setProgram("ssh");

    cr.setArguments(QStringList{
      // Never prompt for an SSH password.
      "-oBatchMode=yes",

      toQString(hostName.getSSHHostName()),

      // Interpret the command string with 'sh' on the far end.  This is
      // consistent with what CommandRunner::setShellCommandLine does.
      //
      // TODO: I would like to allow the user to configure which shell
      // is invoked in both situations.
      //
      // No!  The arguments to 'ssh' get concatenated as a string and
      // then implicitly passed to the login shell.  WTF?
      //"sh",
      //"-c",

      // Ah!  The ssh command line does not include a way to specify
      // the starting directory, which seems like a severe weakness.  I
      // will need to expand my server process.  In the meantime, use an
      // ugly an unreliable hack.
      qstringb("cd '" << toString(dir) << "' && ( " << command << " )")
    });
  }
}


void EditorGlobal::namedTextDocumentRemoved(
  NamedTextDocumentList const *documentList,
  NamedTextDocument *fileDoc) NOEXCEPT
{
  ProcessWatcher *watcher = this->findWatcherForDoc(fileDoc);
  if (watcher) {
    // Closing an output document.  Break the connection to the
    // document so it can go away safely, and start killing the
    // process.
    TRACE1("namedTextDocumentRemoved: killing watcher: " << watcher);
    watcher->m_namedDoc = NULL;
    watcher->m_commandRunner.killProcessNoWait();

    // This is a safe way to kill a child process.  We've detached it
    // from the document, which has been removed from the list and is
    // about to be deallocated, so we're good there.  And we're not
    // waiting for the process to exit, but we haven't forgotten about
    // it either, so we'll reap it if/when it dies.  Finally,
    // ProcessWatcher is servicing the output and error channels,
    // discarding any data that arrives, so we don't expend memory
    // without bound.
  }
}


std::string EditorGlobal::killCommand(NamedTextDocument *doc)
{
  ProcessWatcher *watcher = this->findWatcherForDoc(doc);
  if (!watcher) {
    if (doc->documentProcessStatus() == DPS_RUNNING) {
      DEV_WARNING("running process with no watcher");
      return stringb(
        "BUG: I lost track of the process that is or was producing the "
        "document " << doc->documentName() << "!  This should not happen.");
    }
    else {
      return stringb("Process " << doc->documentName() <<
                     " died before I could kill it.");
    }
  }
  else {
    return toString(watcher->m_commandRunner.killProcessNoWait());
  }
}


ProcessWatcher *EditorGlobal::findWatcherForDoc(NamedTextDocument *fileDoc)
{
  FOREACH_OBJLIST_NC(ProcessWatcher, m_processes, iter) {
    ProcessWatcher *watcher = iter.data();
    if (watcher->m_namedDoc == fileDoc) {
      return watcher;
    }
  }
  return NULL;
}


void EditorGlobal::on_processTerminated(ProcessWatcher *watcher)
{
  TRACE1("on_processTerminated: terminated watcher: " << watcher);
  TRACE1("on_processTerminated: termination desc: " <<
    watcher->m_commandRunner.getTerminationDescription());

  // Get rid of this watcher.
  if (!m_processes.removeIfPresent(watcher)) {
    DEV_WARNING("ProcessWatcher terminated but not in m_processes.");

    // I'm not sure where this rogue watcher came from.  We're now
    // in recovery mode, so refrain from deallocating it.
  }
  else {
    // This calls ~QProcess which closes handles, deallocates I/O
    // buffers, and reaps the child process.
    //
    // In general, this is potentially dangerous since ~QProcess can
    // hang for up to 30s, but I should only get here after the child
    // process has died, meaning ~QProcess won't hang.  See
    // doc/qprocess-hangs.txt.
    delete watcher;
  }
}


// ------------------------------- Fonts -------------------------------
void EditorGlobal::setEditorBuiltinFont(BuiltinFont newFont)
{
  m_editorBuiltinFont = newFont;

  Q_EMIT signal_editorFontChanged();
}


// -------------------------- Macro recorder ---------------------------
void EditorGlobal::recordCommand(std::unique_ptr<EditorCommand> cmd)
{
  m_recentCommands.push_back(std::move(cmd));

  // Limit the number of recorded commands by discarding the oldest
  // ones.
  while (m_recentCommands.size() > MAX_NUM_RECENT_COMMANDS) {
    m_recentCommands.pop_front();
  }
}


EditorCommandVector EditorGlobal::getRecentCommands(int n) const
{
  EditorCommandVector ret;

  int numCommands = static_cast<int>(m_recentCommands.size());

  for (int i = std::max(0, numCommands - n); i < numCommands; ++i) {
    ret.push_back(std::unique_ptr<EditorCommand>(
      m_recentCommands[i]->clone()));
  }

  return ret;
}


// -------------------------- Editor settings --------------------------
/*static*/ std::string EditorGlobal::getEditorStateFileName(
  std::string const &globalAppStateDir,
  char const *fname)
{
  SMFileUtil sfu;
  std::string dir = sfu.normalizePathSeparators(globalAppStateDir);
  std::string combined = dir + "/sm-editor/" + fname;
  sfu.createParentDirectories(combined);
  return combined;
}


/*static*/ std::string EditorGlobal::getSettingsFileName()
{
  return getEditorStateFileName(
    getXDGConfigHome(), "editor-settings.gdvn");
}


bool EditorGlobal::saveSettingsFile(QWidget * NULLABLE parent) NOEXCEPT
{
  try {
    std::string fname = getSettingsFileName();
    EXN_CONTEXT("Saving " << doubleQuote(fname));

    SMFileUtil sfu;
    sfu.createDirectoryAndParents(sfu.splitPathDir(fname));

    // Convert settings to GDV.
    GDValue gdvSettings(m_settings);

    if (m_doNotSaveSettings) {
      TRACE1("saveSettingsFile: Not saving settings due to `m_doNotSaveSettings`.");
    }
    else {
      // Write as GDVN, atomically.
      sfu.atomicallyWriteFileAsString(fname, gdvSettings.asLinesString());

      TRACE1("saveSettingsFile: Wrote settings file: " << doubleQuote(fname));
    }

    return true;
  }
  catch (std::exception &e) {
    warningBox(parent, e.what());
    return false;
  }
}


bool EditorGlobal::loadSettingsFile(QWidget * NULLABLE parent) NOEXCEPT
{
  try {
    loadSettingsFile_throwIfError();
    return true;
  }
  catch (std::exception &e) {
    warningBox(parent, e.what());
    return false;
  }
}


void EditorGlobal::loadSettingsFile_throwIfError()
{
  std::string fname = getSettingsFileName();
  EXN_CONTEXT("Loading " << doubleQuote(fname));

  SMFileUtil sfu;
  if (sfu.pathExists(fname)) {
    GDValue gdvSettings = GDValue::readFromFile(fname);
    EditorSettings settings{GDValueParser(gdvSettings)};
    m_settings.swap(settings);

    TRACE1("loadSettingsFile: Loaded settings file: " << doubleQuote(fname));
  }
  else {
    TRACE1("loadSettingsFile: Settings file does not exist: " << doubleQuote(fname));
  }
}


void EditorGlobal::settings_addMacro(
  QWidget * NULLABLE parent,
  std::string const &name,
  EditorCommandVector const &commands)
{
  m_settings.addMacro(name, commands);
  saveSettingsFile(parent);
}


bool EditorGlobal::settings_deleteMacro(
  QWidget * NULLABLE parent,
  std::string const &name)
{
  if (m_settings.deleteMacro(name)) {
    saveSettingsFile(parent);
    return true;
  }
  else {
    return false;
  }
}


void EditorGlobal::settings_setMostRecentlyRunMacro(
  QWidget * NULLABLE parent,
  std::string const &name)
{
  m_settings.setMostRecentlyRunMacro(name);
  saveSettingsFile(parent);
}


std::string EditorGlobal::settings_getMostRecentlyRunMacro(
  QWidget * NULLABLE parent)
{
  std::string ret = m_settings.getMostRecentlyRunMacro();
  if (ret.empty()) {
    // The act of checking could have cleared it.
    saveSettingsFile(parent);
  }
  return ret;
}


bool EditorGlobal::settings_addHistoryCommand(
  QWidget * NULLABLE parent,
  EditorCommandLineFunction whichFunction,
  std::string const &cmd,
  bool useSubstitution,
  bool prefixStderrLines)
{
  if (m_settings.addHistoryCommand(whichFunction, cmd,
                                   useSubstitution, prefixStderrLines)) {
    saveSettingsFile(parent);
    return true;
  }
  else {
    return false;
  }
}


bool EditorGlobal::settings_removeHistoryCommand(
  QWidget * NULLABLE parent,
  EditorCommandLineFunction whichFunction,
  std::string const &cmd)
{
  if (m_settings.removeHistoryCommand(whichFunction, cmd)) {
    saveSettingsFile(parent);
    return true;
  }
  else {
    return false;
  }
}


void EditorGlobal::settings_setLeftWindowPos(
  QWidget * NULLABLE parent,
  WindowPosition const &pos)
{
  m_settings.setLeftWindowPos(pos);
  saveSettingsFile(parent);
}


void EditorGlobal::settings_setRightWindowPos(
  QWidget * NULLABLE parent,
  WindowPosition const &pos)
{
  m_settings.setRightWindowPos(pos);
  saveSettingsFile(parent);
}


void EditorGlobal::settings_setGrepsrcSearchesSubrepos(
  QWidget * NULLABLE parent,
  bool b)
{
  m_settings.setGrepsrcSearchesSubrepos(b);
  saveSettingsFile(parent);
}


// ------------------------------ Dialogs ------------------------------
NamedTextDocument *EditorGlobal::runOpenFilesDialog(QWidget *callerWindow)
{
  if (!m_openFilesDialog) {
    m_openFilesDialog.reset(new OpenFilesDialog(this));
  }
  return m_openFilesDialog->runDialog(callerWindow);
}


void EditorGlobal::showConnectionsDialog()
{
  if (!m_connectionsDialog) {
    m_connectionsDialog.reset(new ConnectionsDialog(&m_vfsConnections));
  }

  showRaiseAndActivateWindow(m_connectionsDialog.get());
}


ApplyCommandDialog &EditorGlobal::getApplyCommandDialog(
  EditorCommandLineFunction eclf)
{
  xassert(cc::z_le_lt(eclf, NUM_EDITOR_COMMAND_LINE_FUNCTIONS));
  if (!m_applyCommandDialogs[eclf]) {
    m_applyCommandDialogs[eclf].reset(
      new ApplyCommandDialog(this, eclf));
  }
  return *( m_applyCommandDialogs[eclf] );
}


RCSerf<DiagnosticDetailsDialog> EditorGlobal::getDiagnosticDetailsDialog()
{
  if (!m_diagnosticDetailsDialog) {
    m_diagnosticDetailsDialog =
      std::make_unique<DiagnosticDetailsDialog>();
  }
  return m_diagnosticDetailsDialog.get();
}


void EditorGlobal::warningBox(
  QWidget * NULLABLE parent,
  std::string const &str) const
{
  QMessageBox::warning(parent, toQString(appName), toQString(str));
}


void EditorGlobal::hideModelessDialogs()
{
  if (m_connectionsDialog) {
    m_connectionsDialog->hide();
  }
}


// ---------------------- Recent editor widgets ----------------------
void EditorGlobal::addRecentEditorWidget(EditorWidget *ew)
{
  m_recentEditorWidgets.add(ew);
}


void EditorGlobal::removeRecentEditorWidget(EditorWidget *ew)
{
  m_recentEditorWidgets.remove(ew);
}


EditorWidget *EditorGlobal::getOtherEditorWidget(EditorWidget *ew)
{
  return m_recentEditorWidgets.getRecentOther(ew);
}


EditorWidget *EditorGlobal::selectEditorWidget(
  EditorWidget *ew, EditorNavigationOptions opts)
{
  if (opts == EditorNavigationOptions::ENO_NORMAL) {
    return ew;
  }
  else {
    return getOtherEditorWidget(ew);
  }
}


// ------------------------------ Logging ------------------------------
/*static*/ std::unique_ptr<smbase::ExclusiveWriteFile>
  EditorGlobal::openEditorLogFile()
{
  std::unique_ptr<ExclusiveWriteFile> ret(
    tryCreateExclusiveWriteFile(getEditorLogFileInitialName()));
  if (ret) {
    ret->stream()
      << getEditorVersionString()    // Has label, ends with newline.
      << "Started at " << localTimeString() << ".\n";
    ret->stream().flush();
  }
  return ret;
}


/*static*/ std::string EditorGlobal::getEditorLogFileInitialName()
{
  return getEditorStateFileName(
    getXDGStateHome(), "editor.log");
}


std::optional<std::string> EditorGlobal::getEditorLogFileNameOpt() const
{
  if (m_editorLogFile) {
    return m_editorLogFile->getFname();
  }
  else {
    return std::nullopt;
  }
}


void EditorGlobal::log(std::string const &msg)
{
  // Always write the message to the trace output if active.
  TRACE1("log: " << msg);

  // Then log if we have a log file.
  if (m_editorLogFile) {
    m_editorLogFile->stream() << msg << std::endl;
  }
}


void EditorGlobal::logAndWarn(
  QWidget * NULLABLE parent,
  std::string const &dialogMessage,
  std::string const &logMessage)
{
  log(stringb(dialogMessage << "\n" << logMessage));

  std::string const followUp =
    (m_editorLogFile?
       stringb("See log file in " <<
               doubleQuote(m_editorLogFile->getFname()) <<
               " for more details.") :
       std::string("Logging is currently disabled, so additional "
                   "detail is not available."));

  warningBox(parent, stringb(dialogMessage << "\n" << followUp));
}


// ---------------------------- LSP Global -----------------------------
void EditorGlobal::lspConnectSignals()
{
  // Connect LSP signals.
  QObject::connect(
    m_lspManager.get(), &LSPManager::signal_hasPendingDiagnostics,
    this,              &EditorGlobal::on_lspHasPendingDiagnostics);
  QObject::connect(
    m_lspManager.get(), &LSPManager::signal_hasPendingErrorMessages,
    this,              &EditorGlobal::on_lspHasPendingErrorMessages);
  QObject::connect(
    m_lspManager.get(), &LSPManager::signal_changedProtocolState,
    this,              &EditorGlobal::on_lspChangedProtocolState);
}


void EditorGlobal::lspDisconnectSignals()
{
  // Shut down the LSP server.
  QObject::disconnect(m_lspManager.get(), nullptr, this, nullptr);
  {
    std::string shutdownMsg = m_lspManager->stopServer();
    TRACE1("dtor: LSP Manager stopServer() returned: " << shutdownMsg);
  }
}


void EditorGlobal::on_lspHasPendingDiagnostics() NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  while (m_lspManager->hasPendingDiagnostics()) {
    // Get some pending diagnostics.
    std::string fname = m_lspManager->getFileWithPendingDiagnostics();
    std::unique_ptr<LSP_PublishDiagnosticsParams> lspDiags(
      m_lspManager->takePendingDiagnosticsFor(fname));

    if (!lspDiags->m_version.has_value()) {
      // Just discard them.
      TRACE1("lsp: Received LSP diagnostics without a version.");
      continue;
    }

    // Convert to our internal format.
    std::unique_ptr<TextDocumentDiagnostics> tdd(
      convertLSPDiagsToTDD(lspDiags.get()));
    lspDiags.reset();

    DocumentName docName =
      DocumentName::fromFilename(HostName::asLocal(), fname);

    if (NamedTextDocument *doc = getFileWithName(docName)) {
      doc->updateDiagnostics(std::move(tdd));
    }
    else {
      // This could happen if we notify the server of new contents and
      // then immediately close the document.
      TRACE1("lsp: Received LSP diagnostics for " << docName <<
             " but that file is not open in the editor.");
    }
  }

  GENERIC_CATCH_END
}


void EditorGlobal::on_lspHasPendingErrorMessages() NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  while (m_lspManager->hasPendingErrorMessages()) {
    lspAddErrorMessage(m_lspManager->takePendingErrorMessage());
  }

  GENERIC_CATCH_END
}


void EditorGlobal::on_lspChangedProtocolState() NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  // Relay, primarily to the LSP status widgets.
  Q_EMIT signal_lspChangedProtocolState();

  GENERIC_CATCH_END
}


LSPManager const *EditorGlobal::lspManagerC()
{
  return m_lspManager.get();
}


/*static*/ std::string EditorGlobal::lspGetStderrLogFileInitialName()
{
  return getEditorStateFileName(
    getXDGStateHome(), "lsp-server.log");
}


std::optional<std::string> EditorGlobal::lspStartServer()
{
  return m_lspManager->startServer();
}


LSPProtocolState EditorGlobal::lspGetProtocolState() const
{
  return m_lspManager->getProtocolState();
}


bool EditorGlobal::lspIsRunningNormally() const
{
  return m_lspManager->isRunningNormally();
}


bool EditorGlobal::lspIsInitializing() const
{
  return lspGetProtocolState() == LSP_PS_INITIALIZING;
}


std::string EditorGlobal::lspExplainAbnormality() const
{
  return m_lspManager->explainAbnormality();
}


NamedTextDocument *
EditorGlobal::lspGetOrCreateServerCapabilitiesDocument()
{
  return getOrCreateGeneratedDocument(
    "LSP Server Capabilities",
    m_lspManager->getServerCapabilities().asLinesString());
}


void EditorGlobal::lspAddErrorMessage(std::string &&msg)
{
  // I'm thinking this should also emit a signal, although right now I
  // don't have any component prepared to receive it.
  m_lspErrorMessages.push_back(std::move(msg));
}


std::string EditorGlobal::lspGetServerStatus() const
{
  std::ostringstream oss;

  oss << "Using fake server: " << GDValue(lspIsFakeServer()) << ".\n";

  oss << "Status: " << m_lspManager->checkStatus() << "\n";

  oss << "Has pending diagnostics: "
      << GDValue(m_lspManager->hasPendingDiagnostics()) << ".\n";

  if (std::size_t n = m_lspErrorMessages.size()) {
    oss << n << " errors:\n";
    for (std::string const &m : m_lspErrorMessages) {
      oss << "  " << m << "\n";
    }
  }

  return oss.str();
}


std::string EditorGlobal::lspStopServer()
{
  std::string report = m_lspManager->stopServer();

  // With the server shut down, all files are effectively closed w.r.t.
  // the LSP protocol.  Stop tracking changes for all files.
  m_documentList.allFilesStopTrackingChanges();

  return report;
}


std::optional<std::vector<std::string>> EditorGlobal::lspGetCodeLines(
  SynchronousWaiter &waiter,
  std::vector<HostFileLine> const &locations)
{
  return lspGetCodeLinesFunction(
    waiter,
    locations,
    *m_lspManager,
    m_vfsConnections);
}


// --------------------------- LSP Per-file ----------------------------
bool EditorGlobal::lspFileIsOpen(NamedTextDocument const *ntd) const
{
  return
    lspIsRunningNormally() &&
    ntd->isCompatibleWithLSP() &&
    m_lspManager->isFileOpen(ntd->filename());
}


RCSerf<LSPDocumentInfo const> EditorGlobal::lspGetDocInfo(
  NamedTextDocument const *doc) const
{
  if (lspFileIsOpen(doc)) {
    return m_lspManager->getDocInfo(doc->filename());
  }
  else {
    return nullptr;
  }
}


void EditorGlobal::lspOpenFile(
  NamedTextDocument *ntd, std::string const &languageId)
{
  xassertPrecondition(!lspFileIsOpen(ntd));

  // This can throw `XNumericConversion`.
  LSP_VersionNumber version =
    LSP_VersionNumber::fromTDVN(ntd->getVersionNumber());

  m_lspManager->notify_textDocument_didOpen(
    ntd->filename(),
    languageId,
    version,
    ntd->getWholeFileString());

  ntd->beginTrackingChanges();

  xassertPostcondition(lspFileIsOpen(ntd));
}


void EditorGlobal::lspUpdateFile(NamedTextDocument *ntd)
{
  xassertPrecondition(lspFileIsOpen(ntd));
  lspSendUpdatedContents(*m_lspManager, *ntd);
}


void EditorGlobal::lspCloseFile(NamedTextDocument *ntd)
{
  if (lspFileIsOpen(ntd)) {
    m_lspManager->notify_textDocument_didClose(ntd->filename());

    // Clear the diagnostics.
    ntd->updateDiagnostics(nullptr);

    // Since `fname` is now closed w.r.t. LSP, we should stop tracking
    // its changes.
    ntd->stopTrackingChanges();
  }
}


// ---------------------------- LSP Queries ----------------------------
void EditorGlobal::lspCancelRequestWithID(int id)
{
  xassertPrecondition(lspIsRunningNormally());

  m_lspManager->cancelRequestWithID(id);
}


bool EditorGlobal::lspHasReplyForID(int id) const
{
  xassertPrecondition(lspIsRunningNormally());

  return m_lspManager->hasReplyForID(id);
}


JSON_RPC_Reply EditorGlobal::lspTakeReplyForID(int id)
{
  xassertPrecondition(lspIsRunningNormally());
  xassertPrecondition(lspHasReplyForID(id));

  return m_lspManager->takeReplyForID(id);
}


int EditorGlobal::lspRequestRelatedLocation(
  LSPSymbolRequestKind lsrk,
  NamedTextDocument const *ntd,
  TextMCoord coord)
{
  xassertPrecondition(lspFileIsOpen(ntd));

  return m_lspManager->requestRelatedLocation(
    lsrk, ntd->filename(), coord);
}


int EditorGlobal::lspSendArbitraryRequest(
  std::string const &method,
  gdv::GDValue const &params)
{
  xassertPrecondition(lspIsRunningNormally());

  return m_lspManager->sendRequest(method, params);
}


void EditorGlobal::lspSendArbitraryNotification(
  std::string const &method,
  gdv::GDValue const &params)
{
  xassertPrecondition(lspIsRunningNormally());

  m_lspManager->sendNotification(method, params);
}


// --------------------- Qt infrastructure-related ---------------------
void EditorGlobal::focusChangedHandler(QWidget *from, QWidget *to)
{
  TRACE2("focus changed from " << qObjectDesc(from) <<
         " to " << qObjectDesc(to));

  if (!from && to && qobject_cast<QMenuBar*>(to)) {
    TRACE2("focus arrived at menu bar from alt-tab");
    QWidget *p = to->parentWidget();
    if (p) {
      // This is part of a workaround for an apparent Qt bug: if I
      // press Alt, the menu bar gets focus.  If then press Alt+Tab,
      // another window gets focus.  If then press Alt+Tab again, my
      // window gets focus again.  So far so good.
      //
      // Except the menu bar still has focus from the earlier Alt!  And
      // pressing Alt again does not help; I have to Tab out of there.
      //
      // The fix is in two parts.  First, we recognize the buggy focus
      // transition here: 'from' is null, meaning focus came from
      // another window (including another window in my application),
      // and 'to' is a QMenuBar.  Then we reassign focus to the menu
      // bar's parent, which will be EditorWindow.
      //
      // Finally, EditorWindow has its EditorWidget as a focus proxy,
      // so focus automatically goes to it instead.
      //
      // Found the bug in Qt tracker:
      // https://bugreports.qt.io/browse/QTBUG-44405
      TRACE2("setting focus to " << qObjectDesc(p));
      p->setFocus(Qt::ActiveWindowFocusReason);
    }
    else {
      TRACE2("menu has no parent?");
    }
  }
}


static std::string objectDesc(QObject const *obj)
{
  if (!obj) {
    return "NULL";
  }

  std::ostringstream sb;
  sb << "{name=\"" << obj->objectName()
     << "\" path=\"" << qObjectPath(obj)
     << "\" addr=" << (void*)obj
     << " class=" << obj->metaObject()->className()
     << "}";
  return sb.str();
}


// For debugging, this function allows me to inspect certain events as
// they are dispatched.
bool EditorGlobal::notify(QObject *receiver, QEvent *event)
{
  static int s_eventCounter=0;
  int const eventNo = s_eventCounter++;

  QEvent::Type const type = event->type();

  if (type == QEvent::KeyPress) {
    if (QKeyEvent const *keyEvent =
          dynamic_cast<QKeyEvent const *>(event)) {
      TRACE2("notifyInput: " << eventNo << ": "
        "KeyPress to " << objectDesc(receiver) <<
        ": ts=" << keyEvent->timestamp() <<
        " key=" << keysString(*keyEvent) <<
        " acc=" << keyEvent->isAccepted() <<
        " focus=" << objectDesc(QApplication::focusWidget()));

      bool ret = this->QApplication::notify(receiver, event);

      TRACE2("notifyInput: " << eventNo << ": returns " << ret <<
        ", acc=" << keyEvent->isAccepted());

      return ret;
    }
  }

  if (type == QEvent::Shortcut) {
    if (QShortcutEvent const *shortcutEvent =
          dynamic_cast<QShortcutEvent const *>(event)) {
      TRACE2("notifyInput: " << eventNo << ": "
        "Shortcut to " << objectDesc(receiver) <<
        ": ambig=" << shortcutEvent->isAmbiguous() <<
        " id=" << shortcutEvent->shortcutId() <<
        " keys=" << shortcutEvent->key().toString() <<
        " acc=" << shortcutEvent->isAccepted() <<
        " focus=" << objectDesc(QApplication::focusWidget()));

      bool ret = this->QApplication::notify(receiver, event);

      TRACE2("notifyInput: " << eventNo << ": returns " << ret <<
        ", acc=" << shortcutEvent->isAccepted());

      return ret;
    }
  }

  // This is normally too noisy.
  if (false && type == QEvent::Resize) {
    if (QResizeEvent const *resizeEvent =
          dynamic_cast<QResizeEvent const *>(event)) {
      TRACE2("notifyInput: " << eventNo << ": "
        "ResizeEvent to " << objectDesc(receiver) <<
        ": spontaneous=" << resizeEvent->spontaneous() <<
        " oldSize=" << toString(resizeEvent->oldSize()) <<
        " size=" << toString(resizeEvent->size()));
    }
  }

  return this->QApplication::notify(receiver, event);
}


void EditorGlobal::slot_broadcastSearchPanelChanged(
  SearchAndReplacePanel *panel) NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  TRACE2("slot_broadcastSearchPanelChanged");
  FOREACH_OBJLIST_NC(EditorWindow, m_editorWindows, iter) {
    iter.data()->searchPanelChanged(panel);
  }

  GENERIC_CATCH_END
}


// ------------------------- Global functions --------------------------
std::string serializeECV(EditorCommandVector const &commands)
{
  std::ostringstream oss;

  for (auto const &cmdptr : commands) {
    oss << toGDValue(*cmdptr).asString() << "\n";
  }

  return oss.str();
}


EditorCommandVector cloneECV(EditorCommandVector const &commands)
{
  EditorCommandVector dest;

  for (auto const &cmdptr : commands) {
    dest.push_back(std::unique_ptr<EditorCommand>(
      cmdptr->clone()));
  }

  return dest;
}


// Respond to a failed DEV_WARNING.
static void editorDevWarningHandler(char const *file, int line,
                                    char const *msg)
{
  cerr << "DEV_WARNING: " << file << ':' << line << ": " << msg << endl;

  if (getenv("ABORT_ON_DEV_WARNING")) {
    // This is useful when I'm minimizing an input that causes a
    // warning to fire, so I don't want recovery.
    cerr << "Aborting due to ABORT_ON_DEV_WARNING." << endl;
    abort();
  }

  static bool prompted = false;
  if (!prompted) {
    prompted = true;
    messageBox_details(NULL, "Developer Warning Fired",
      "A warning meant for this application's developer has fired.  "
      "The details were written to the standard error output (the "
      "console).  Please report them to the maintainer.\n"
      "\n"
      "Although this application will try to keep running, "
      "beware that the warning might indicate future instability.  "
      "This message will only appear once per session, but all "
      "warnings are written to the error output.",

      qstringb(file << ':' << line << ": " << msg));
  }
}


// Possibly print counts of allocated objects.  Return their sum.
static int printObjectCountsIf(char const *when, bool print)
{
  if (print) {
    cout << "Counts " << when << ':' << endl;
  }

  // Current count of outstanding objects.
  int sum = 0;

  #define PRINT_COUNT(var) \
    sum += (var);          \
    if (print) {           \
      PVAL(var);           \
    }

  PRINT_COUNT(EditorWidget::s_objectCount);
  PRINT_COUNT(EditorWindow::s_objectCount);
  PRINT_COUNT(NamedTextDocument::s_objectCount);
  PRINT_COUNT(TextDocument::s_objectCount);
  PRINT_COUNT(TextDocumentEditor::s_objectCount);

  #undef PRINT_COUNT

  return sum;
}

static int maybePrintObjectCounts(char const *when)
{
  static bool b = envAsBool("PRINT_OBJECT_COUNTS");
  return printObjectCountsIf(when, b);
}


// Defined in `smbase/trace.h`.  But I can't include that file because
// it conflicts with `sm-trace.h`.
void traceAddFromEnvVar();


static int innerMain(int argc, char **argv)
{
  // Not implemented in smbase for mingw.
  //printSegfaultAddrs();

  // I still have some modules using the older `trace` facility, and
  // this is needed to allow them to respond to the "TRACE" envvar.
  traceAddFromEnvVar();

  int ret;
  {
    try {
      // Suppress "Unable to set geometry", and provide advice about
      // "platform plugin" errors.
      installSMQtUtilMessageHandler();

      EditorGlobal app(argc, argv);

      Owner<EventRecorder> recorder;
      if (app.m_recordInputEvents) {
        recorder = new EventRecorder("events.out");
      }

      if (!app.m_eventFileTest.empty()) {
        // Automated GUI test.
        g_abortUponDevWarning = true;
        cout << "running test: " << app.m_eventFileTest << endl;
        EventReplay replay(app.m_eventFileTest);
        std::string error = replay.runTest();
        if (error.empty()) {
          cout << "test passed" << endl;

          // Check all invariants before declaring victory.
          app.selfCheck();

          ret = 0;   // It could still fail, depending on object counts.
        }
        else {
          // In the failure case, EventReplay prints the failure.
          ret = 2;
        }

        if (char const *noQuit = getenv("NOQUIT")) {
          // If we are going to return at least $NOQUIT, keep the app
          // running so I can inspect its state.  Often I will use
          // NOQUIT=1 to stop iff there is a test failure.  NOQUIT=0
          // will stop unconditionally.
          if (ret >= atoi(noQuit)) {
            cout << "leaving app running due to NOQUIT" << endl;
            app.exec();
          }
        }
      }

      else {
        // Run the app normally.
        SetRestore< void (*)(char const*, int, char const *) >
          restorer(g_devWarningHandler, &editorDevWarningHandler);
        ret = app.exec();
      }
    }
    catch (QuitAfterPrintingHelp &) {
      return 0;
    }
    catch (XBase &x) {
      cerr << x.why() << endl;
      return 2;
    }

    maybePrintObjectCounts("before EditorGlobal destruction");
  }

  int remaining = maybePrintObjectCounts("after EditorGlobal destruction");
  if (remaining != 0) {
    // Force the counts to be printed so we know more about the problem.
    printObjectCountsIf("after EditorGlobal destruction", true);

    cout << "WARNING: Allocated objects at end is " << remaining
         << ", not zero!\n"
         << "There is a leak or use-after-free somewhere." << endl;

    // Ensure this causes a test failure if it happens during an
    // automated test.
    ret = 4;
  }

  return ret;
}


// Return a dynamically-allocated, NUL-terminated array holding the
// contents of `s`.
static char *dupString(std::string const &s)
{
  char *ret = new char[s.size() + 1];
  std::memcpy(ret, s.data(), s.size());
  ret[s.size()] = 0;
  return ret;
}


// Run `command` as if it were this program's `argc/argv`.  Return
// non-zero if the attempt fails.
static int runOneCommand(std::vector<std::string> const &command)
{
  std::cout << "Command: "
            << shellDoubleQuoteCommand(command) << "\n";

  // The tests unfortunately have some race conditions I have not
  // been able to fully eliminate, so try each one up to 3 times.
  //
  // 2025-09-01: It has been a while since I saw a failure.  Let's try
  // setting the default value to 1 so we do not retry at all.
  int attempts = 0;
  int const retryLimit = envAsIntOr(1, "EDITOR_TEST_RETRIES");

  while (true) {
    ++attempts;

    // The `argc/argv` passed to the `QApplication` ctor must be
    // mutable, so make copies of everything.
    char **argv = new char*[command.size()+1];
    std::vector<char*> argvPointers;
    for (std::size_t i=0; i < command.size(); ++i) {
      argv[i] = dupString(command.at(i));
      argvPointers.push_back(argv[i]);
    }
    argv[command.size()] = nullptr;

    // Run the constructed command line.
    int result = innerMain(command.size()+1, argv);

    // Free the `argv` array.
    delete[] argv;

    // Free the individual strings.  We do not read `argv` to find
    // them because `QApplication` can modify the `argv` array.
    for (char *p : argvPointers) {
      delete[] p;
    }

    if (result == 0) {
      // Test passed.
      return 0;
    }
    else {
      if (attempts < retryLimit) {
        std::cout << "Attempt " << attempts << " of " << retryLimit
                  << " failed (code=" << result << "), retrying...\n";
      }
      else {
        std::cout << "All " << retryLimit
                  << " attempts failed, stopping with code "
                  << result << "\n";
        return result;
      }
    }
  }

  // Not reached.
}


// Read `cmdsFname` as GDVN and treat its contents as a sequence of
// command lines to run, in sequence, as if they were this program's
// command line.  The expectation is these are automated GUI tests.
//
// It is faster to run all the tests in one process if possible rather
// than starting a new process for each.
static int runCommandList(std::string const &cmdsFname)
{
  try {
    std::cout << "Executing commands from " << cmdsFname << "\n";

    // Parse the file as GDVN and parse the GDV as a sequence of
    // sequences of strings.
    GDValue commandGDV = GDValue::readFromFile(cmdsFname);
    std::vector<std::vector<std::string>> allCommands =
      gdvpTo<decltype(allCommands)>(GDValueParser(commandGDV));

    // Treat each sequence of strings as a command line.
    for (std::vector<std::string> const &command : allCommands) {
      int result = runOneCommand(command);
      if (result != 0) {
        return result;
      }
    }

    // All tests passed.
    return 0;
  }

  catch (std::exception &x) {
    // At least for now, if a test fails with an exception, I do not
    // treat that as retryable.
    std::cout << "Test failed: " << x.what() << "\n";
    return 2;
  }
}


int main(int argc, char **argv)
{
  if (argc >= 2) {
    std::string firstArg(argv[1]);
    char const *prefix = "-testCommands=";
    if (beginsWith(firstArg, prefix)) {
      std::string cmdsFname = firstArg.substr(std::strlen(prefix));
      return runCommandList(cmdsFname);
    }
  }

  return innerMain(argc, argv);
}


// EOF
