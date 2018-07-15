// main.cc
// code for main.cc, and application main() function

#include "main.h"                      // EditorWindow

// this dir
#include "editor-widget.h"             // EditorWidget
#include "process-watcher.h"           // ProcessWatcher
#include "textinput.h"                 // TextInputDialog

// smqtutil
#include "qtutil.h"                    // toQString

// smbase
#include "sm-file-util.h"              // SMFileUtil
#include "strtokp.h"                   // StrtokParse
#include "test.h"                      // PVAL
#include "trace.h"                     // TRACE

#include <QMenuBar>
#include <QMessageBox>
#include <QStyleFactory>


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


// ---------------- GlobalState ----------------
GlobalState::GlobalState(int argc, char **argv)
  : QApplication(argc, argv),
    m_pixmaps(),
    m_documentList(),
    m_windows(),
    m_processes(),
    m_openFilesDialog(NULL)
{
  m_documentList.addObserver(this);

  // Optionally print the list of styles Qt supports.
  if (tracingSys("style")) {
    QStringList keys = QStyleFactory::keys();
    cout << "style keys:\n";
    for (int i=0; i < keys.size(); i++) {
      cout << "  " << keys.at(i).toUtf8().constData() << endl;
    }
  }

  // Activate my own modification to the Qt style.  This works even
  // if the user overrides the default style, for example, by passing
  // "-style Windows" on the command line.
  this->setStyle(new EditorProxyStyle);

  // Open the first window, initially showing the default "untitled"
  // file that 'fileDocuments' made in its constructor.
  EditorWindow *ed = createNewWindow(m_documentList.getDocumentAt(0));

  // this caption is immediately replaced with another one, at the
  // moment, since I call fileNewFile() right away
  //ed.setCaption("An Editor");

  // open all files specified on the command line
  SMFileUtil sfu;
  for (int i=1; i < argc; i++) {
    string path = sfu.getAbsolutePath(argv[i]);
    path = sfu.normalizePathSeparators(path);
    ed->fileOpenFile(path);
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
  connect(this, SIGNAL(lastWindowClosed()),
          this, SLOT(quit()));

  connect(this, SIGNAL(focusChanged(QWidget*, QWidget*)),
          this, SLOT(focusChangedHandler(QWidget*, QWidget*)));

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


GlobalState::~GlobalState()
{
  m_documentList.removeObserver(this);
}


EditorWindow *GlobalState::createNewWindow(NamedTextDocument *initFile)
{
  EditorWindow *ed = new EditorWindow(this, initFile);
  ed->setObjectName("Editor Window");

  // NOTE: caller still has to say 'ed->show()'!

  return ed;
}


NamedTextDocument *GlobalState::createNewFile(string const &dir)
{
  return m_documentList.createUntitledDocument(dir);
}


bool GlobalState::hasFileWithName(string const &name) const
{
  return m_documentList.findDocumentByNameC(name) != NULL;
}


bool GlobalState::hasFileWithTitle(string const &title) const
{
  return m_documentList.findDocumentByTitleC(title) != NULL;
}


string GlobalState::uniqueTitleFor(string const &filename)
{
  return m_documentList.computeUniqueTitle(filename);
}


void GlobalState::trackNewDocumentFile(NamedTextDocument *f)
{
  m_documentList.addDocument(f);
}


bool GlobalState::hotkeyAvailable(int key) const
{
  return m_documentList.findDocumentByHotkeyC(key) == NULL;
}


void GlobalState::deleteDocumentFile(NamedTextDocument *file)
{
  m_documentList.removeDocument(file);
  delete file;
}


NamedTextDocument *GlobalState::runOpenFilesDialog()
{
  if (!m_openFilesDialog.get()) {
    m_openFilesDialog = new OpenFilesDialog(&m_documentList);
  }
  return m_openFilesDialog->runDialog();
}


// Return a document to be populated by running 'command' in 'dir'.
// The name must be unique, but we will reuse an existing document if
// its process has terminated.
NamedTextDocument *GlobalState::getNewCommandOutputDocument(
  QString origDir, QString command)
{
  // Come up with a unique named based on the command and directory.
  string dir = SMFileUtil().stripTrailingDirectorySeparator(toString(origDir));
  string base = stringb(dir << "$ " << toString(command));
  for (int n = 1; n < 100; n++) {
    string name = (n==1? base : stringb(base << " (" << n << ')'));

    NamedTextDocument *fileDoc = m_documentList.findDocumentByName(name);
    if (!fileDoc) {
      // Nothing with this name, let's use it to make a new one.
      TRACE("process", "making new document: " << name);
      NamedTextDocument *newDoc = new NamedTextDocument();
      newDoc->setNonFileName(name, dir);
      newDoc->m_title = name;
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
    TRACE("process", "reusing existing document: " << name);
    fileDoc->clearContentsAndHistory();
    return fileDoc;
  }

  // Maybe something went haywire creating commands?
  xfailure("Hit limit of 100 same-named command output documents!");
}


NamedTextDocument *GlobalState::launchCommand(QString dir, QString command)
{
  // Find or create a document to hold the result.
  NamedTextDocument *fileDoc =
    this->getNewCommandOutputDocument(dir, command);

  // Show the directory and command at the top of the document.  Among
  // other things, this is a helpful acknowledgment that something is
  // happening in case the process does not print anything right away
  // (or at all!).
  fileDoc->appendString(stringb("Dir: " << toString(dir) << '\n'));
  fileDoc->appendString(stringb("Cmd: " << toString(command) << "\n\n"));

  // Make the watcher that will populate that file.
  ProcessWatcher *watcher = new ProcessWatcher(fileDoc);
  m_processes.prepend(watcher);
  QObject::connect(watcher, &ProcessWatcher::signal_processTerminated,
                   this,           &GlobalState::on_processTerminated);

  // Launch the child process.  I rely on having a POSIX shell.
  watcher->m_commandRunner.setProgram("sh");
  watcher->m_commandRunner.setArguments(QStringList() << "-c" << command);
  watcher->m_commandRunner.setWorkingDirectory(dir);
  watcher->m_commandRunner.startAsynchronous();

  TRACE("process", "dir: " << toString(dir));
  TRACE("process", "cmd: " << toString(command));
  TRACE("process", "started: " << watcher);
  TRACE("process", "fileDoc: " << fileDoc);

  return fileDoc;
}


string GlobalState::killCommand(NamedTextDocument *doc)
{
  ProcessWatcher *watcher = this->findWatcherForDoc(doc);
  if (!watcher) {
    if (doc->documentProcessStatus() == DPS_RUNNING) {
      DEV_WARNING("running process with no watcher");
      return stringb(
        "BUG: I lost track of the process that is or was producing the "
        "document \"" << doc->name() << "\"!  This should not happen.");
    }
    else {
      return stringb("Process \"" << doc->name() <<
                     "\" died before I could kill it.");
    }
  }
  else {
    return toString(watcher->m_commandRunner.killProcessNoWait());
  }
}


ProcessWatcher *GlobalState::findWatcherForDoc(NamedTextDocument *fileDoc)
{
  FOREACH_OBJLIST_NC(ProcessWatcher, m_processes, iter) {
    ProcessWatcher *watcher = iter.data();
    if (watcher->m_namedDoc == fileDoc) {
      return watcher;
    }
  }
  return NULL;
}


void GlobalState::namedTextDocumentRemoved(
  NamedTextDocumentList *documentList,
  NamedTextDocument *fileDoc) NOEXCEPT
{
  ProcessWatcher *watcher = this->findWatcherForDoc(fileDoc);
  if (watcher) {
    // Closing an output document.  Break the connection to the
    // document so it can go away safely, and start killing the
    // process.
    TRACE("process", "killing: " << watcher);
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


void GlobalState::on_processTerminated(ProcessWatcher *watcher)
{
  TRACE("process", "terminated: " << watcher);

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


void GlobalState::focusChangedHandler(QWidget *from, QWidget *to)
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
      GlobalState app(argc, argv);
      ret = app.exec();
    }
    catch (xBase &x) {
      cerr << x.why() << endl;
      return 4;
    }

    maybePrintObjectCounts("before GlobalState destruction");
  }

  int remaining = maybePrintObjectCounts("after GlobalState destruction");
  if (remaining != 0) {
    // Force the counts to be printed so we know more about the problem.
    printObjectCountsIf("after GlobalState destruction", true);

    cout << "WARNING: Allocated objects at end is " << remaining
         << ", not zero!\n"
         << "There is a leak or use-after-free somewhere." << endl;
  }

  return ret;
}


// EOF
