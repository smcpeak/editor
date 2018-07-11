// main.cc
// code for main.cc, and application main() function

#include "main.h"                      // EditorWindow

// this dir
#include "editor-widget.h"             // EditorWidget

// smqtutil
#include "qtutil.h"                    // toQString

// smbase
#include "sm-file-util.h"              // SMFileUtil
#include "strtokp.h"                   // StrtokParse
#include "test.h"                      // PVAL
#include "trace.h"                     // TRACE

#include <QMenuBar>
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
    m_windows()
{
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
  EditorWindow *ed = createNewWindow(m_documentList.getFileAt(0));

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
{}


EditorWindow *GlobalState::createNewWindow(FileTextDocument *initFile)
{
  EditorWindow *ed = new EditorWindow(this, initFile);
  ed->setObjectName("Editor Window");

  // NOTE: caller still has to say 'ed->show()'!

  return ed;
}


FileTextDocument *GlobalState::createNewFile()
{
  TRACE("untitled", "creating untitled file");
  FileTextDocument *b = new FileTextDocument();

  // Come up with a unique "untitled" name.
  b->filename = "untitled.txt";
  b->isUntitled = true;
  int n = 1;
  while (this->hasFileWithName(b->filename)) {
    n++;
    b->filename = stringb("untitled" << n << ".txt");
  }

  b->title = b->filename;
  trackNewDocumentFile(b);
  return b;
}


bool GlobalState::hasFileWithName(string const &fname) const
{
  return m_documentList.findFileByNameC(fname) != NULL;
}


bool GlobalState::hasFileWithTitle(string const &fname) const
{
  return m_documentList.findFileByTitleC(fname) != NULL;
}


string GlobalState::uniqueTitleFor(string const &filename)
{
  return m_documentList.computeUniqueTitle(filename);
}


void GlobalState::trackNewDocumentFile(FileTextDocument *f)
{
  m_documentList.addFile(f);
}


bool GlobalState::hotkeyAvailable(int key) const
{
  return m_documentList.findFileByHotkeyC(key) == NULL;
}


void GlobalState::deleteDocumentFile(FileTextDocument *file)
{
  m_documentList.removeFile(file);
  delete file;
}


FileTextDocument *GlobalState::runOpenFilesDialog()
{
  if (!m_openFilesDialog.get()) {
    m_openFilesDialog = new OpenFilesDialog(&m_documentList);
  }
  return m_openFilesDialog->runDialog();
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
  PRINT_COUNT(FileTextDocument::s_objectCount);
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
