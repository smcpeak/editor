// editor-global.cc
// code for editor-global.cc, and application main() function

#include "editor-global.h"             // this module

// editor
#include "connections-dialog.h"        // ConnectionsDialog
#include "editor-widget.h"             // EditorWidget
#include "event-recorder.h"            // EventRecorder
#include "event-replay.h"              // EventReplay
#include "process-watcher.h"           // ProcessWatcher
#include "textinput.h"                 // TextInputDialog

// smqtutil
#include "qtutil.h"                    // toQString
#include "timer-event-loop.h"          // sleepWhilePumpingEvents

// smbase
#include "dev-warning.h"               // g_devWarningHandler
#include "objcount.h"                  // CheckObjectCount
#include "sm-file-util.h"              // SMFileUtil
#include "strtokp.h"                   // StrtokParse
#include "strutil.h"                   // prefixEquals
#include "sm-test.h"                   // PVAL
#include "trace.h"                     // TRACE

// Qt
#include <QKeyEvent>
#include <QMenuBar>
#include <QMessageBox>
#include <QShortcutEvent>
#include <QStyleFactory>

// libc
#include <stdlib.h>                    // atoi


// ---------------- EditorProxyStyle ----------------
int EditorProxyStyle::pixelMetric(
  PixelMetric metric,
  const QStyleOption *option,
  const QWidget *widget) const
{
  if (metric == PM_MaximumDragDistance) {
    // The standard behavior is when the mouse is dragged too far away
    // from the scrollbar, it jumps back to its original position. I
    // find that behavior annoying and useless.  This disables it.
    return -1;
  }

  return QProxyStyle::pixelMetric(metric, option, widget);
}


// --------------------------- EditorGlobal ----------------------------
EditorGlobal::EditorGlobal(int argc, char **argv)
  : QApplication(argc, argv),
    m_pixmaps(),
    m_documentList(),
    m_windows(),
    m_recordInputEvents(false),
    m_eventFileTest(),
    m_filenameInputDialogHistory(),
    m_vfsConnections(),
    m_processes(),
    m_openFilesDialog(NULL),
    m_connectionsDialog()
{
  m_documentList.addObserver(this);

  // Optionally print the list of styles Qt supports.
  if (tracingSys("style")) {
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
  this->setStyle(new EditorProxyStyle);

  // Set the scrollbars to have a darker thumb.  Otherwise this is
  // meant to imitate the Windows 10 scrollbars.  (That is just for
  // consistency with other apps; I don't think the design is good.)
  //
  // Changing the color of the thumb requires basically re-implementing
  // the entire scrollbar visuals, unfortunately.  This specification is
  // based on the examples in the Qt docs at:
  //
  //   http://doc.qt.io/qt-5/stylesheet-examples.html#customizing-qscrollbar
  //
  // but I then modified it quite a bit.
  string borderColor("#C0C0C0");
  this->setStyleSheet(qstringb(
    "QScrollBar:vertical {"
    "  background: white;"
    "  width: 17px;"
    "  margin: 17px 0 17px 0;"     // top left right bottom?
    "}"
    "QScrollBar::handle:vertical {"
    "  border: 1px solid #404040;"
    "  background: #808080;"
    "  min-height: 20px;"
    "}"
    "QScrollBar::add-line:vertical {"
    "  border: 1px solid " << borderColor << ";"
    "  background: white;"
    "  height: 17px;"
    "  subcontrol-position: bottom;"
    "  subcontrol-origin: margin;"
    "}"
    "QScrollBar::sub-line:vertical {"
    "  border: 1px solid " << borderColor << ";"
    "  background: white;"
    "  height: 17px;"
    "  subcontrol-position: top;"
    "  subcontrol-origin: margin;"
    "}"
    "QScrollBar::up-arrow:vertical {"
    "  image: url(:/pix/scroll-up-arrow.png);"
    "  width: 15px;"
    "  height: 15px;"
    "}"
    "QScrollBar::down-arrow:vertical {"
    "  image: url(:/pix/scroll-down-arrow.png);"
    "  width: 15px;"
    "  height: 15px;"
    "}"
    "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {"
    "  border-left: 1px solid " << borderColor << ";"
    "  border-right: 1px solid " << borderColor << ";"
    "  background: none;"
    "}"
  ));

  // Establish the initial VFS connection before creating the first
  // EditorWindow, since the EW can issue VFS requests.
  QObject::connect(&m_vfsConnections, &VFS_Connections::signal_failed,
                   this, &EditorGlobal::on_connectionFailed);
  m_vfsConnections.connectLocal();

  // Open the first window, initially showing the default "untitled"
  // file that 'fileDocuments' made in its constructor.
  EditorWindow *ed = createNewWindow(m_documentList.getDocumentAt(0));

  // this caption is immediately replaced with another one, at the
  // moment, since I call fileNewFile() right away
  //ed.setCaption("An Editor");

  try {
    processCommandLineOptions(ed, argc, argv);
  }
  catch (...) {
    // Errors in command line processing are communicated with
    // exceptions, which we allow to propagate, after first shutting
    // down the connections in order to avoid spurious additional
    // complaints.
    m_vfsConnections.shutdownAll();
    throw;
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

  ed->show();

  // This is a partial mitigation to a weird bug where, on Windows,
  // the editor window sometimes does not get focus when I start it.
  // See details in notes.txt.
  //
  // This call does not fix the bug, rather it makes the icon in the
  // taskbar flash when it happens, reducing the likelihood that I
  // will start typing into the wrong window.
  ed->activateWindow();
}


EditorGlobal::~EditorGlobal()
{
  // First get rid of the windows so I don't have other entities
  // watching documents and potentially getting confused and/or sending
  // signals I am not prepared for.
  m_windows.deleteAll();

  if (m_processes.isNotEmpty()) {
    // Now try to kill any running processes.  Do not wait for any of
    // them, among other things because I do not want to get any signals
    // during this loop since then I might modify 'm_processes' during
    // an ongoing iteration.
    FOREACH_OBJLIST_NC(ProcessWatcher, m_processes, iter) {
      ProcessWatcher *watcher = iter.data();
      TRACE("process", "in ~EditorGlobal, killing: " << watcher);
      watcher->m_namedDoc = NULL;
      watcher->m_commandRunner.killProcessNoWait();
    }

    // Wait up to one second for all children to die.  Pump the event
    // queue while waiting so that as they die, I will receive
    // on_processTerminated signals and can reap them and remove them
    // from 'm_processes'.
    for (int waits=0; waits < 10 && m_processes.isNotEmpty(); waits++) {
      TRACE("process", "in ~EditorGlobal, waiting 100ms #" << (waits+1));
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
}


void EditorGlobal::processCommandLineOptions(
  EditorWindow *ed, int argc, char **argv)
{
  SMFileUtil sfu;
  for (int i=1; i < argc; i++) {
    string arg(argv[i]);
    if (arg.empty()) {
      xformat("An empty command line argument is not allowed.");
    }

    else if (arg[0]=='-') {
      if (prefixEquals(arg, "-ev=")) {
        // Replay a sequence of events as part of a test.
        m_eventFileTest = arg.substring(4, arg.length()-4);
      }

      else if (arg == "-record") {
        // Record events to seed a new test.
        m_recordInputEvents = true;
      }

      else if (prefixEquals(arg, "-conn=")) {
        // Open a connection to a specified host.
        string hostName = arg.substring(6, arg.length()-6);
        m_vfsConnections.connect(HostName::asSSH(hostName));
      }

      else {
        xformat(stringb("Unknown option: " << arg));
      }
    }

    else {
      // Open all non-option files specified on the command line.
      string path = sfu.getAbsolutePath(arg);
      path = sfu.normalizePathSeparators(path);
      ed->fileOpenFile(HostName::asLocal(), path);
    }
  }
}


EditorWindow *EditorGlobal::createNewWindow(NamedTextDocument *initFile)
{
  static int windowCounter = 1;

  EditorWindow *ed = new EditorWindow(this, initFile);
  ed->setObjectName(qstringb("window" << windowCounter++));

  // NOTE: caller still has to say 'ed->show()'!

  return ed;
}


NamedTextDocument *EditorGlobal::createNewFile(string const &dir)
{
  return m_documentList.createUntitledDocument(dir);
}


bool EditorGlobal::hasFileWithName(DocumentName const &docName) const
{
  return m_documentList.findDocumentByNameC(docName) != NULL;
}


bool EditorGlobal::hasFileWithTitle(string const &title) const
{
  return m_documentList.findDocumentByTitleC(title) != NULL;
}


string EditorGlobal::uniqueTitleFor(DocumentName const &docName) const
{
  return m_documentList.computeUniqueTitle(docName);
}


void EditorGlobal::trackNewDocumentFile(NamedTextDocument *f)
{
  m_documentList.addDocument(f);
}


void EditorGlobal::deleteDocumentFile(NamedTextDocument *file)
{
  m_documentList.removeDocument(file);
  delete file;
}


NamedTextDocument *EditorGlobal::runOpenFilesDialog(QWidget *callerWindow)
{
  if (!m_openFilesDialog.get()) {
    m_openFilesDialog = new OpenFilesDialog(&m_documentList);
  }
  return m_openFilesDialog->runDialog(callerWindow);
}


// Return a document to be populated by running 'command' in 'dir'.
// The name must be unique, but we will reuse an existing document if
// its process has terminated.
NamedTextDocument *EditorGlobal::getNewCommandOutputDocument(
  HostName const &hostName, QString origDir, QString command)
{
  // Come up with a unique named based on the command and directory.
  string dir = SMFileUtil().stripTrailingDirectorySeparator(toString(origDir));
  string base = stringb(dir << "$ " << toString(command));
  for (int n = 1; n < 100; n++) {
    DocumentName docName;
    docName.setNonFileResourceName(hostName,
      (n==1? base : stringb(base << " (" << n << ')')), dir);

    NamedTextDocument *fileDoc = m_documentList.findDocumentByName(docName);
    if (!fileDoc) {
      // Nothing with this name, let's use it to make a new one.
      TRACE("process", "making new document: " << docName);
      NamedTextDocument *newDoc = new NamedTextDocument();
      newDoc->setDocumentName(docName);
      newDoc->m_title = uniqueTitleFor(docName);
      trackNewDocumentFile(newDoc);
      return newDoc;
    }

    if (fileDoc->documentProcessStatus() != DPS_FINISHED) {
      // Not a finished process output document, keep looking.
      continue;
    }

    // Safety check.  I could remove this later.
    ProcessWatcher *watcher = this->findWatcherForDoc(fileDoc);
    xassert(!watcher);

    // This is a left-over document from a previous run.  Re-use it.
    TRACE("process", "reusing existing document: " << docName);
    fileDoc->clearContentsAndHistory();
    return fileDoc;
  }

  // Maybe something went haywire creating commands?
  xfailure("Hit limit of 100 same-named command output documents!");
}


NamedTextDocument *EditorGlobal::launchCommand(
  HostName const &hostName, QString dir,
  bool prefixStderrLines, QString command)
{
  // Find or create a document to hold the result.
  NamedTextDocument *fileDoc =
    this->getNewCommandOutputDocument(hostName, dir, command);

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

  // Launch the child process.
  watcher->m_commandRunner.startAsynchronous();

  TRACE("process", "dir: " << toString(dir));
  TRACE("process", "cmd: " << toString(command));
  TRACE("process", "fullCommand: " << fullCommand);
  TRACE("process", "fileDoc: " << fileDoc->documentName());
  TRACE("process", "started watcher: " << watcher);

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


string EditorGlobal::killCommand(NamedTextDocument *doc)
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


void EditorGlobal::namedTextDocumentRemoved(
  NamedTextDocumentList *documentList,
  NamedTextDocument *fileDoc) NOEXCEPT
{
  ProcessWatcher *watcher = this->findWatcherForDoc(fileDoc);
  if (watcher) {
    // Closing an output document.  Break the connection to the
    // document so it can go away safely, and start killing the
    // process.
    TRACE("process", "killing watcher: " << watcher);
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


void EditorGlobal::on_processTerminated(ProcessWatcher *watcher)
{
  TRACE("process", "terminated watcher: " << watcher);
  TRACE("process", "termination desc: " <<
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


void EditorGlobal::on_connectionFailed(
  HostName hostName, string reason) NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  QMessageBox::warning(nullptr, "Connection Failed", qstringb(
    "The connection to " << hostName << " has failed.  Reads "
    "and writes will not work until this connection is restarted.  "
    "Error message: " << reason));

  GENERIC_CATCH_END
}


void EditorGlobal::focusChangedHandler(QWidget *from, QWidget *to)
{
  TRACE("focus", "focus changed from " << qObjectDesc(from) <<
                 " to " << qObjectDesc(to));

  if (!from && to && qobject_cast<QMenuBar*>(to)) {
    TRACE("focus", "focus arrived at menu bar from alt-tab");
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
      TRACE("focus", "setting focus to " << qObjectDesc(p));
      p->setFocus(Qt::ActiveWindowFocusReason);
    }
    else {
      TRACE("focus", "menu has no parent?");
    }
  }
}


void EditorGlobal::slot_broadcastSearchPanelChanged(
  SearchAndReplacePanel *panel) NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  TRACE("sar", "EditorGlobal::slot_broadcastSearchPanelChanged");
  FOREACH_OBJLIST_NC(EditorWindow, m_windows, iter) {
    iter.data()->searchPanelChanged(panel);
  }

  GENERIC_CATCH_END
}


void EditorGlobal::showConnectionsDialog()
{
  if (!m_connectionsDialog) {
    m_connectionsDialog.reset(new ConnectionsDialog(&m_vfsConnections));
  }

  // Sequence from:
  // https://stackoverflow.com/questions/7817334/qt-correct-way-to-show-display-raise-window

  // Bring it to the front.
  m_connectionsDialog->raise();

  // Give it focus.
  m_connectionsDialog->activateWindow();

  // Make it visible, un-minimize.
  m_connectionsDialog->showNormal();
}


void EditorGlobal::hideModelessDialogs()
{
  if (m_connectionsDialog) {
    m_connectionsDialog->hide();
  }
}


static string objectDesc(QObject const *obj)
{
  if (!obj) {
    return "NULL";
  }

  stringBuilder sb;
  sb << "{name=\"" << obj->objectName()
     << "\" path=\"" << qObjectPath(obj)
     << "\" addr=" << (void*)obj
     << " class=" << obj->metaObject()->className()
     << "}";
  return sb;
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
      TRACE("notifyInput", eventNo << ": "
        "KeyPress to " << objectDesc(receiver) <<
        ": ts=" << keyEvent->timestamp() <<
        " key=" << keysString(*keyEvent) <<
        " acc=" << keyEvent->isAccepted() <<
        " focus=" << objectDesc(QApplication::focusWidget()));

      bool ret = this->QApplication::notify(receiver, event);

      TRACE("notifyInput", eventNo << ": returns " << ret <<
        ", acc=" << keyEvent->isAccepted());

      return ret;
    }
  }

  if (type == QEvent::Shortcut) {
    if (QShortcutEvent const *shortcutEvent =
          dynamic_cast<QShortcutEvent const *>(event)) {
      TRACE("notifyInput", eventNo << ": "
        "Shortcut to " << objectDesc(receiver) <<
        ": ambig=" << shortcutEvent->isAmbiguous() <<
        " id=" << shortcutEvent->shortcutId() <<
        " keys=" << shortcutEvent->key().toString() <<
        " acc=" << shortcutEvent->isAccepted() <<
        " focus=" << objectDesc(QApplication::focusWidget()));

      bool ret = this->QApplication::notify(receiver, event);

      TRACE("notifyInput", eventNo << ": returns " << ret <<
        ", acc=" << shortcutEvent->isAccepted());

      return ret;
    }
  }

  // This is normally too noisy.
  if (false && type == QEvent::Resize) {
    if (QResizeEvent const *resizeEvent =
          dynamic_cast<QResizeEvent const *>(event)) {
      TRACE("notifyInput", eventNo << ": "
        "ResizeEvent to " << objectDesc(receiver) <<
        ": spontaneous=" << resizeEvent->spontaneous() <<
        " oldSize=" << toString(resizeEvent->oldSize()) <<
        " size=" << toString(resizeEvent->size()));
    }
  }

  return this->QApplication::notify(receiver, event);
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
  return printObjectCountsIf(when, tracingSys("objectCount"));
}


int main(int argc, char **argv)
{
  TRACE_ARGS();

  // Not implemented in smbase for mingw.
  //printSegfaultAddrs();

  int ret;
  {
    try {
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
        string error = replay.runTest();
        if (error.empty()) {
          cout << "test passed" << endl;
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
        Restorer< void (*)(char const*, int, char const *) >
          restorer(g_devWarningHandler, &editorDevWarningHandler);
        ret = app.exec();
      }
    }
    catch (xBase &x) {
      cerr << x.why() << endl;
      return 4;
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


// EOF
