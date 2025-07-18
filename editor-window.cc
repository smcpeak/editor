// editor-window.cc
// code for editor-window.h

#include "editor-window.h"             // this module

// editor
#include "apply-command-dialog.h"      // ApplyCommandDialog
#include "c_hilite.h"                  // C_Highlighter
#include "command-runner.h"            // CommandRunner
#include "diff-hilite.h"               // DiffHighlighter
#include "editor-global.h"             // EditorGlobal
#include "editor-widget-frame.h"       // EditorWidgetFrame
#include "editor-widget.h"             // EditorWidget
#include "filename-input.h"            // FilenameInputDialog
#include "fonts-dialog.h"              // FontsDialog
#include "git-version.h"               // editor_git_version
#include "hashcomment_hilite.h"        // HashComment_Highlighter
#include "keybindings.doc.gen.h"       // doc_keybindings
#include "keys-dialog.h"               // KeysDialog
#include "lsp-manager.h"               // LSPManager
#include "macro-creator-dialog.h"      // MacroCreatorDialog
#include "macro-run-dialog.h"          // MacroRunDialog
#include "makefile_hilite.h"           // Makefile_Highlighter
#include "ocaml_hilite.h"              // OCaml_Highlighter
#include "pixmaps.h"                   // g_editorPixmaps
#include "python_hilite.h"             // Python_Highlighter
#include "sar-panel.h"                 // SearchAndReplacePanel
#include "status.h"                    // StatusDisplay
#include "td-editor.h"                 // TextDocumentEditor
#include "textinput.h"                 // TextInputDialog
#include "vfs-query-sync.h"            // VFS_QuerySync
#include "vfs-msg.h"                   // VFS_ReadFileRequest

// smqtutil
#include "smqtutil/qhboxframe.h"       // QHBoxFrame
#include "smqtutil/qtguiutil.h"        // CursorSetRestore
#include "smqtutil/qtutil.h"           // toQString

// smbase
#include "smbase/exc.h"                // XOpen, GENERIC_CATCH_BEGIN/END
#include "smbase/gdvalue.h"            // gdv::GDValue
#include "smbase/mysig.h"              // printSegfaultAddrs
#include "smbase/nonport.h"            // fileOrDirectoryExists
#include "smbase/objcount.h"           // CHECK_OBJECT_COUNT
#include "smbase/overflow.h"           // safeToInt
#include "smbase/sm-file-util.h"       // SMFileUtil
#include "smbase/sm-test.h"            // PVAL
#include "smbase/string-util.h"        // endsWith, vectorOfUCharToString
#include "smbase/stringb.h"            // stringbc
#include "smbase/strutil.h"            // dirname
#include "smbase/syserr.h"             // smbase::XSysError
#include "smbase/trace.h"              // TRACE_ARGS
#include "smbase/xoverflow.h"          // smbase::XNumericConversion

// libc++
#include <string_view>                 // std::string_view
#include <utility>                     // std::move

// libc
#include <string.h>                    // strrchr
#include <stdlib.h>                    // atoi

// Qt
#include <qmenubar.h>                  // QMenuBar
#include <qscrollbar.h>                // QScrollBar
#include <qlabel.h>                    // QLabel
#include <qfiledialog.h>               // QFileDialog
#include <qmessagebox.h>               // QMessageBox
#include <qlayout.h>                   // QVBoxLayout
#include <qsizegrip.h>                 // QSizeGrip
#include <qstatusbar.h>                // QStatusBar
#include <qlineedit.h>                 // QLineEdit

#include <QCloseEvent>
#include <QDesktopWidget>
#include <QFontDialog>
#include <QInputDialog>

using namespace gdv;
using namespace smbase;


int EditorWindow::s_objectCount = 0;

CHECK_OBJECT_COUNT(EditorWindow);


EditorWindow::EditorWindow(EditorGlobal *editorGlobal,
                           NamedTextDocument *initFile,
                           QWidget *parent)
  : QWidget(parent),
    m_editorGlobal(editorGlobal),
    m_menuBar(NULL),
    m_editorWidgetFrame(NULL),
    m_sarPanel(NULL),
    m_statusArea(NULL),
    m_toggleReadOnlyAction(NULL),
    m_toggleVisibleWhitespaceAction(NULL),
    m_toggleVisibleSoftMarginAction(NULL),
    m_toggleHighlightTrailingWSAction(NULL)
{
  xassert(m_editorGlobal);
  xassert(initFile);

  // Use a layout tree to manage sizes of child widgets.
  //
  // See doc/editor-window-layout.ded.png.
  QVBoxLayout *mainArea = new QVBoxLayout();
  mainArea->setObjectName("mainArea");
  mainArea->setSpacing(0);
  mainArea->setContentsMargins(0, 0, 0, 0);

  this->m_menuBar = new QMenuBar();
  this->m_menuBar->setObjectName("m_menuBar");
  mainArea->addWidget(this->m_menuBar);

  m_editorWidgetFrame = new EditorWidgetFrame(this, initFile);
  m_editorWidgetFrame->setObjectName("frame1");
  mainArea->addWidget(m_editorWidgetFrame, 1 /*stretch*/);

  m_sarPanel = new SearchAndReplacePanel();
  mainArea->addWidget(m_sarPanel);
  m_sarPanel->setObjectName("m_sarPanel");
  m_sarPanel->hide();      // Initially hidden.
  QObject::connect(
    m_sarPanel, &SearchAndReplacePanel::signal_searchPanelChanged,
    m_editorGlobal, &EditorGlobal::slot_broadcastSearchPanelChanged);

  this->m_statusArea = new StatusDisplay();
  this->m_statusArea->setObjectName("m_statusArea");
  mainArea->addWidget(this->m_statusArea);

  // See explanation in EditorGlobal::focusChangedHandler().
  setFocusProxy(m_editorWidgetFrame);

  // Start with focus on the editor frame.
  m_editorWidgetFrame->setFocus();

  // Connect these, which had to wait until both were constructed.
  m_sarPanel->setEditorWidget(editorWidget());

  this->buildMenu();

  this->setWindowIcon(g_editorPixmaps->icon);

  this->setLayout(mainArea);
  this->setGeometry(400,100,      // initial location
                    800,800);     // initial size

  // Set scrollbar ranges, status bar text, and window title.
  this->updateForChangedFile();

  // I want this object destroyed when it is closed.
  this->setAttribute(Qt::WA_DeleteOnClose);

  this->m_editorGlobal->m_windows.append(this);
  this->m_editorGlobal->m_documentList.addObserver(this);

  QObject::connect(
    m_editorGlobal, &EditorGlobal::signal_editorFontChanged,
    this, &EditorWindow::slot_editorFontChanged);

  EditorWindow::s_objectCount++;
}


EditorWindow::~EditorWindow()
{
  EditorWindow::s_objectCount--;

  m_editorGlobal->m_documentList.removeObserver(this);

  // This object might have already been removed, for example because
  // the EditorGlobal destructor is running, and is in the process of
  // removing elements from the list and destroying them.  Hence the
  // "IfPresent" part of this call.
  m_editorGlobal->m_windows.removeIfPresent(this);

  // The QObject destructor will destroy both 'm_sarPanel' and
  // 'editorWidget()', but the documentation of ~QObject does not
  // specify an order.  Disconnect them here so that either order works.
  m_sarPanel->setEditorWidget(NULL);

  // See doc/signals-and-dtors.txt.
  QObject::disconnect(m_sarPanel,     NULL, m_editorGlobal, NULL);
  QObject::disconnect(m_editorGlobal, NULL, this, NULL);
}


EditorSettings const &EditorWindow::editorSettings() const
{
  return editorGlobal()->getSettings();
}


LSPManager &EditorWindow::lspManager()
{
  return editorGlobal()->m_lspManager;
}


EditorWidget *EditorWindow::editorWidget() const
{
  return m_editorWidgetFrame->editorWidget();
}


VFS_Connections *EditorWindow::vfsConnections() const
{
  return m_editorGlobal->vfsConnections();
}


// Create a menu action.
//
// This function exists partly to work around an Eclipse CDT bug, as it
// has trouble with overload resolution when using QMenu::addAction and
// a pointer-to-member argument.
//
// It also exists to set the object name of the QAction in order to help
// my event record/replay test framework.
//
// I might move this to smqtutil at some point.
template <class RECEIVER>
QAction *addMenuAction(
  QMenu *menu, char const *title,
  RECEIVER *rcv, void (RECEIVER::*ptm)(),
  char const *functionName,
  QKeySequence const &shortcut = QKeySequence(0))
{
  QAction *action = menu->addAction(title, rcv, ptm, shortcut);
  action->setObjectName(functionName);
  return action;
}


#define MENU_ITEM(title, function)                          \
  addMenuAction(menu, title, this, &EditorWindow::function, \
                #function) /* user ; */

#define MENU_ITEM_KEY(title, function, key)                 \
  addMenuAction(menu, title, this, &EditorWindow::function, \
                #function, key) /* user ; */


void EditorWindow::buildMenu()
{
  #define CHECKABLE_ACTION(field, title, function, initChecked)  \
    this->field = MENU_ITEM(title, function);                    \
    this->field->setCheckable(true);                             \
    this->field->setChecked(initChecked) /* user ; */

  {
    QMenu *menu = this->m_menuBar->addMenu("&File");
    menu->setObjectName("fileMenu");

    MENU_ITEM    ("&New", fileNewFile);
    MENU_ITEM_KEY("&Open ...", fileOpen, Qt::Key_F3);
    MENU_ITEM_KEY("Open f&ile at cursor", fileOpenAtCursor,
                  Qt::CTRL + Qt::Key_I);

    MENU_ITEM_KEY("&Save", fileSave, Qt::Key_F2);
    MENU_ITEM    ("Save &as ...", fileSaveAs);
    MENU_ITEM    ("&Close", fileClose);

    menu->addSeparator();

    CHECKABLE_ACTION(m_toggleReadOnlyAction,
                  "Read only", fileToggleReadOnly, false /*initChecked*/);
    MENU_ITEM_KEY("&Reload", fileReload, Qt::Key_F5);
    MENU_ITEM    ("Check for on-disk changes", fileCheckForChanges);

    menu->addSeparator();

    MENU_ITEM_KEY("&Launch (run) command ...", fileLaunchCommand,
                  Qt::ALT + Qt::Key_R);
    MENU_ITEM_KEY("Run \"run-make-from-editor\"", fileRunMake, Qt::Key_F9);
    MENU_ITEM    ("Kill running process ...", fileKillProcess);

    menu->addSeparator();

    MENU_ITEM    ("&Manage connections ...", fileManageConnections);

    menu->addSeparator();

    MENU_ITEM    ("Reload settings",
                  fileLoadSettings);
    MENU_ITEM    ("Save settings",
                  fileSaveSettings);

    menu->addSeparator();

    MENU_ITEM    ("E&xit", fileExit);
  }

  {
    QMenu *menu = this->m_menuBar->addMenu("&Edit");
    menu->setObjectName("editMenu");

    // Used shortcut letters: ACDJFGKLNPRSTU

    MENU_ITEM_KEY("&Undo", editUndo, Qt::ALT + Qt::Key_Backspace);
    MENU_ITEM_KEY("&Redo", editRedo, Qt::ALT + Qt::SHIFT + Qt::Key_Backspace);

    menu->addSeparator();

    // Some of these items have another shortcut (e.g., Shift+Delete
    // for "Cut"), but even with QAction::setShortcuts they cannot be
    // shown in the menu item, so I do not bind them here.
    MENU_ITEM_KEY("Cu&t", editCut, Qt::CTRL + Qt::Key_X);
    MENU_ITEM_KEY("&Copy", editCopy, Qt::CTRL + Qt::Key_C);
    MENU_ITEM_KEY("&Paste", editPaste, Qt::CTRL + Qt::Key_V);

    // Here, I'm faking something that looks like a shortcut since the
    // menu Delete function is slightly different from the keyboard one,
    // as only the latter will do something if nothing is selected.
    MENU_ITEM    ("&Delete\tDelete", editDelete);

    MENU_ITEM_KEY("&Kill (cut) current line", editKillLine,
                  Qt::CTRL + Qt::Key_K);

    MENU_ITEM_KEY("Select entire &file", editSelectEntireFile,
                  Qt::CTRL + Qt::ALT + Qt::Key_F);

    menu->addSeparator();

    MENU_ITEM_KEY("&Search ...", editSearch, Qt::CTRL + Qt::Key_S);
    MENU_ITEM_KEY("Rep&lace", editReplace, Qt::CTRL + Qt::Key_R);
    MENU_ITEM_KEY("Replace and next", editReplaceAndNext,
                  Qt::CTRL + Qt::SHIFT + Qt::Key_R);
    MENU_ITEM_KEY("&Next search hit\tCtrl+Period", editNextSearchHit,
                  Qt::CTRL + Qt::Key_Period);
    MENU_ITEM_KEY("Previous search hit\tCtrl+Comma", editPreviousSearchHit,
                  Qt::CTRL + Qt::Key_Comma);

    menu->addSeparator();

    MENU_ITEM_KEY("&Goto line ...", editGotoLine, Qt::ALT + Qt::Key_G);
    MENU_ITEM_KEY("Grep source for symbol at cursor ...", editGrepSource,
                  Qt::CTRL + Qt::ALT + Qt::Key_G);

    menu->addSeparator();

    // These two do not have key bindings as proper shortcuts.
    // See doc/tab-key-issues.txt.
    MENU_ITEM    ("Rigidly indent selected lines\tTab",
                  editRigidIndent);
    MENU_ITEM    ("Rigidly un-indent selected lines\tShift+Tab",
                  editRigidUnindent);

    MENU_ITEM_KEY("&Justify paragraph to soft margin",
                  editJustifyParagraph, Qt::CTRL + Qt::Key_J);
    MENU_ITEM_KEY("&Apply command to selection...",
                  editApplyCommand, Qt::ALT + Qt::Key_A);
    MENU_ITEM_KEY("Insert current date/time",
                  editInsertDateTime, Qt::ALT + Qt::Key_D);
  }

  {
    // Used mnemonics: fhmtvw

    QMenu *menu = this->m_menuBar->addMenu("&View");
    menu->setObjectName("viewMenu");

    CHECKABLE_ACTION(m_toggleVisibleWhitespaceAction,
      "Visible &whitespace",
      viewToggleVisibleWhitespace,
      editorWidget()->m_visibleWhitespace);

    MENU_ITEM    ("Set whitespace opacity...", viewSetWhitespaceOpacity);

    CHECKABLE_ACTION(m_toggleVisibleSoftMarginAction,
      "Visible soft &margin",
      viewToggleVisibleSoftMargin,
      editorWidget()->m_visibleSoftMargin);

    MENU_ITEM    ("Set soft margin column...", viewSetSoftMarginColumn);

    CHECKABLE_ACTION(m_toggleHighlightTrailingWSAction,
      "Highlight &trailing whitespace",
      viewToggleHighlightTrailingWS,
      editorWidget()->highlightTrailingWhitespace());

    MENU_ITEM    ("Set &Highlighting...", viewSetHighlighting);

    {
      QMenu *submenu = menu->addMenu("&Fonts");
      submenu->setObjectName("fontMenu");
      QMenu *menu = submenu;

      // Used mnemonics: a

      MENU_ITEM("Set &application font...", viewSetApplicationFont);
      MENU_ITEM("Set &editor font...", viewSetEditorFont);
      MENU_ITEM("Font &help...", viewFontHelp);
    }
  }

  #undef CHECKABLE_ACTION

  {
    QMenu *menu = this->m_menuBar->addMenu("&Macro");
    menu->setObjectName("macroMenu");

    // Used mnemonics: cmr

    MENU_ITEM    ("&Create macro",
                  macroCreateMacro);
    MENU_ITEM_KEY("&Run...",
                  macroRunDialog, Qt::Key_F1);
    MENU_ITEM_KEY("Run &most recently run macro",
                  macroRunMostRecent, Qt::CTRL + Qt::Key_F1);
  }

  {
    QMenu *menu = this->m_menuBar->addMenu("&LSP");
    menu->setObjectName("lspMenu");

    // Used mnemonics: c

    MENU_ITEM    ("Start LSP server",
                  lspStartServer);
    MENU_ITEM    ("Stop LSP server",
                  lspStopServer);
    MENU_ITEM    ("&Check LSP server status",
                  lspCheckStatus);

    menu->addSeparator();

    MENU_ITEM    ("Open this file",
                  lspOpenFile);
    MENU_ITEM    ("Update this file",
                  lspUpdateFile);
    MENU_ITEM    ("Close this file",
                  lspCloseFile);
    MENU_ITEM    ("Review diagnostics for this file",
                  lspReviewDiagnostics);
  }

  {
    QMenu *menu = this->m_menuBar->addMenu("&Window");
    menu->setObjectName("windowMenu");

    // Used mnemonics: cnop

    MENU_ITEM_KEY("Choose an &Open Document ...",
                  windowOpenFilesList, Qt::CTRL + Qt::Key_O);
    MENU_ITEM_KEY("Switch to &Previous Document",
                  windowPreviousFile, Qt::Key_F6);

    menu->addSeparator();

    MENU_ITEM    ("&New Window", windowNewWindow);
    MENU_ITEM_KEY("&Close Window",
      windowCloseWindow, Qt::CTRL + Qt::Key_F4);
    MENU_ITEM_KEY("Move/size to Left Screen Half",
      windowOccupyLeft, Qt::CTRL + Qt::ALT + Qt::Key_Left);
    MENU_ITEM_KEY("Move/size to Right Screen Half",
      windowOccupyRight, Qt::CTRL + Qt::ALT + Qt::Key_Right);
  }

  {
    QMenu *menu = this->m_menuBar->addMenu("&Help");
    menu->setObjectName("helpMenu");

    MENU_ITEM    ("&Keybindings...", helpKeybindings);
    MENU_ITEM    ("&About Scott's Editor...", helpAbout);
    MENU_ITEM    ("About &Qt ...", helpAboutQt);

    menu->addSeparator();

    {
      QMenu *submenu = menu->addMenu("&Debug");
      submenu->setObjectName("debugMenu");
      QMenu *menu = submenu;

      MENU_ITEM    ("Dump &window object tree", helpDebugDumpWindowObjectTree);
      MENU_ITEM    ("Dump &application object tree", helpDebugDumpApplicationObjectTree);

      // The appearance of the widget is affected by whether it has the
      // focus.  However, even when choosing this from the menu, the
      // focus returns to the editor before it draws, if it had it
      // previously, so that turns out not to be a problem.
      MENU_ITEM    ("Save &screenshot of editor widget to file",
                    helpDebugEditorScreenshot);
    }
  }
}


#undef MENU_ITEM
#undef MENU_ITEM_KEY


// not inline because main.h doesn't see editor.h
NamedTextDocument *EditorWindow::currentDocument()
{
  return editorWidget()->getDocument();
}


void EditorWindow::fileNewFile() NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  NamedTextDocument *b = m_editorGlobal->createNewFile(
    editorWidget()->getDocumentDirectory());
  setDocumentFile(b);

  GENERIC_CATCH_END
}


void EditorWindow::makeCurrentDocumentTopmost()
{
  m_editorGlobal->m_documentList.moveDocument(this->currentDocument(), 0);
}


void EditorWindow::setDocumentFile(NamedTextDocument *file)
{
  // Before switching documents, put the old one at the top.  The idea
  // is that this document is the most recently used since it was just
  // shown to the user, even if it hasn't been explicitly switched to
  // recently.
  makeCurrentDocumentTopmost();

  editorWidget()->setDocumentFile(file);
  this->updateForChangedFile();
}

void EditorWindow::updateForChangedFile()
{
  editorWidget()->recomputeLastVisible();
  editorViewChanged();
}


static bool stringAmong(string const &str, char const * const *table,
                        int tableSize)
{
  for (int i=0; i < tableSize; i++) {
    if (str == table[i]) {
      return true;
    }
  }
  return false;
}


void EditorWindow::useDefaultHighlighter(NamedTextDocument *file)
{
  if (file->m_highlighter) {
    delete file->m_highlighter;
    file->m_highlighter = NULL;
  }

  // This handles both "foo.diff" and "git diff".
  if (endsWith(file->resourceName(), "diff")) {
    file->m_highlighter = new DiffHighlighter();

    // Diff output has lots of lines that are not empty and have
    // whitespace on them.  I do not want that highlighted.
    file->m_highlightTrailingWhitespace = false;

    return;
  }

  if (!file->hasFilename()) {
    return;
  }

  // get file extension
  string filename = file->filename();
  char const *dot = strrchr(filename.c_str(), '.');
  if (dot) {
    string ext = string(dot+1);

    static char const * const cppExts[] = {
      "ast",
      "c",
      "cc",
      "cpp",
      "gr",
      "i",
      "ii",
      "h",
      "hh",
      "hpp",
      "java",      // C/C++ highlighting is better than none.
      "lex",
      "y",
    };
    if (stringAmong(ext, cppExts, TABLESIZE(cppExts))) {
      file->m_highlighter = new C_Highlighter(file->getCore());
      return;
    }

    if (streq(ext, "mk")) {
      file->m_highlighter = new Makefile_Highlighter(file->getCore());
      return;
    }

    static char const * const hashCommentExts[] = {
      "ev",
      "pl",
      "sh",
    };
    if (stringAmong(ext, hashCommentExts, TABLESIZE(hashCommentExts))) {
      file->m_highlighter = new HashComment_Highlighter(file->getCore());
      return;
    }

    static char const * const ocamlExts[] = {
      "ml",
      "mli",
    };
    if (stringAmong(ext, ocamlExts, TABLESIZE(ocamlExts))) {
      file->m_highlighter = new OCaml_Highlighter(file->getCore());
      return;
    }

    static char const * const pythonExts[] = {
      "py",
      "pyi",
    };
    if (stringAmong(ext, pythonExts, TABLESIZE(pythonExts))) {
      file->m_highlighter = new Python_Highlighter(file->getCore());
      return;
    }
  }

  if (endsWith(filename, "Makefile")) {
    file->m_highlighter = new Makefile_Highlighter(file->getCore());
    return;
  }
}


string EditorWindow::fileChooseDialog(HostName /*INOUT*/ &hostName,
  string const &origDir, bool saveAs)
{
  string dir(origDir);
  TRACE("fileChooseDialog",
    "saveAs=" << saveAs << " dir: " << dir);
  if (dir == ".") {
    // If I pass "." to one of the static members of QFileDialog, it
    // automatically goes to the current directory.  But when using
    // QFileDialog directly, I have to pass the real directory name.
    dir = SMFileUtil().currentDirectory();
    TRACE("fileOpen", "current dir: " << dir);
  }

  FilenameInputDialog dialog(
    &(m_editorGlobal->m_filenameInputDialogHistory),
    vfsConnections(),
    this);
  dialog.setSaveAs(saveAs);

  QString choice = toQString(dir);

  HostAndResourceName harn(hostName, dir);
  if (dialog.runDialog(&(m_editorGlobal->m_documentList), harn)) {
    hostName = harn.hostName();
    return harn.resourceName();
  }
  else {
    return "";
  }
}


void EditorWindow::fileOpen() NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  HostAndResourceName dirHarn = editorWidget()->getDocumentDirectoryHarn();
  this->on_openFilenameInputDialogSignal(HostFileAndLineOpt(dirHarn, 0));

  GENERIC_CATCH_END
}


void EditorWindow::fileOpenAtCursor() NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  editorWidget()->fileOpenAtCursor();

  GENERIC_CATCH_END
}


void EditorWindow::fileOpenFile(HostAndResourceName const &harn)
{
  TRACE("fileOpen", "fileOpenFile: " << harn);

  if (harn.empty()) {
    // Dialog was canceled.
    return;
  }

  DocumentName docName;
  docName.setFilenameHarn(harn);

  // If this file is already open, switch to it.
  NamedTextDocument *file =
    m_editorGlobal->m_documentList.findDocumentByName(docName);
  if (file) {
    this->setDocumentFile(file);
    return;
  }

  // Load the file contents.
  std::unique_ptr<VFS_ReadFileReply> rfr(
    readFileSynchronously(vfsConnections(), this, harn));
  if (!rfr) {
    // Either the request was canceled or an error has already been
    // reported.
    return;
  }

  file = new NamedTextDocument();
  file->setDocumentName(docName);
  file->m_title = m_editorGlobal->uniqueTitleFor(docName);

  if (rfr->m_success) {
    file->replaceFileAndStats(rfr->m_contents,
                              rfr->m_fileModificationTime,
                              rfr->m_readOnly);
  }
  else {
    if (rfr->m_failureReasonCode == XSysError::R_FILE_NOT_FOUND) {
      // Just have the file open with its name set but no content.
    }
    else {
      this->complain(stringbc(
        rfr->m_failureReasonString <<
        " (code " << rfr->m_failureReasonCode << ")"));
      delete file;
      return;
    }
  }

  this->useDefaultHighlighter(file);

  // is there an untitled, empty file hanging around?
  RCSerf<NamedTextDocument> untitled =
    this->m_editorGlobal->m_documentList.findUntitledUnmodifiedDocument();

  // now that we've opened the file, set the editor widget to edit it
  m_editorGlobal->trackNewDocumentFile(file);
  setDocumentFile(file);

  // remove the untitled file now, if it exists
  if (untitled) {
    m_editorGlobal->deleteDocumentFile(untitled.release());
  }
}


template <class REPLY_TYPE>
std::unique_ptr<REPLY_TYPE> EditorWindow::vfsQuerySynchronously(
  HostName const &hostName, std::unique_ptr<VFS_Message> request)
{
  VFS_QuerySync querySync(vfsConnections(), this);
  return querySync.issueTypedRequestSynchronously<REPLY_TYPE>(
    hostName, std::move(request));
}


bool EditorWindow::checkFileExistenceSynchronously(
  HostAndResourceName const &harn)
{
  std::unique_ptr<VFS_FileStatusReply> reply(
    getFileStatusSynchronously(vfsConnections(), this, harn));
  return reply &&
         reply->m_success &&
         reply->m_fileKind == SMFileUtil::FK_REGULAR;
}


void EditorWindow::fileSave() NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  NamedTextDocument *b = this->currentDocument();
  if (!b->hasFilename()) {
    TRACE("untitled", "file has no title; invoking Save As ...");
    fileSaveAs();
    return;
  }

  if (b->m_modifiedOnDisk) {
    QMessageBox box(this);
    box.setWindowTitle("File Changed");
    box.setText(toQString(stringb(
      "The file " << b->documentName() << " has changed on disk.  "
      "If you save, those changes will be overwritten by the text "
      "in the editor's memory.  Save anyway?")));
    box.addButton(QMessageBox::Save);
    box.addButton(QMessageBox::Cancel);
    int ret = box.exec();
    if (ret != QMessageBox::Save) {
      return;
    }
  }

  writeTheFile();

  GENERIC_CATCH_END
}


void EditorWindow::writeTheFile()
{
  RCSerf<NamedTextDocument> file = this->currentDocument();

  std::unique_ptr<VFS_WriteFileRequest> req(new VFS_WriteFileRequest);
  req->m_path = file->filename();
  req->m_contents = file->getWholeFile();
  std::unique_ptr<VFS_WriteFileReply> reply(
    vfsQuerySynchronously<VFS_WriteFileReply>(file->hostName(),
                                              std::move(req)));

  if (reply) {
    if (reply->m_success) {
      file->m_lastFileTimestamp = reply->m_fileModificationTime;
      file->m_modifiedOnDisk = false;
      file->noUnsavedChanges();

      // Remove the asterisk indicating unsaved changes.
      editorViewChanged();
    }
    else {
      // There is not a severity between "warning" and "critical",
      // and "critical" is a bit obnoxious.
      QMessageBox::warning(this, "Write Error", qstringb(
        "Failed to save file " << file->documentName() <<
        ": " << reply->m_failureReasonString));
    }
  }
}


bool EditorWindow::stillCurrentDocument(NamedTextDocument *doc)
{
  if (doc != currentDocument()) {
    // Note: It is possible that 'doc' has been deallocated here!
    QMessageBox::information(this, "Object Changed",
      "The current file changed while the dialog was open.  "
      "Aborting operation.");
    return false;
  }
  else {
    return true;
  }
}


void EditorWindow::fileSaveAs() NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  NamedTextDocument *fileDoc = currentDocument();

  // Host to start in.
  HostName hostName = fileDoc->hostName();

  // Directory to start in.  This may change if we prompt the user
  // more than once.
  string dir = fileDoc->directory();

  while (true) {
    string chosenFilename =
      this->fileChooseDialog(hostName, dir, true /*saveAs*/);
    if (chosenFilename.empty()) {
      return;
    }
    if (!stillCurrentDocument(fileDoc)) {
      return;
    }

    if (fileDoc->hasFilename() &&
        fileDoc->hostName() == hostName &&
        fileDoc->filename() == chosenFilename) {
      // User chose to save using the same file name.
      this->fileSave();
      return;
    }

    DocumentName docName;
    docName.setFilename(hostName, chosenFilename);

    if (this->m_editorGlobal->hasFileWithName(docName)) {
      this->complain(stringbc(
        "There is already an open file with name " <<
        docName << ".  Choose a different name to save as."));

      // Discard name portion, but keep directory.
      dir = SMFileUtil().splitPathDir(chosenFilename);

      // Now prompt again.
    }
    else {
      fileDoc->setDocumentName(docName);
      fileDoc->m_title = m_editorGlobal->uniqueTitleFor(docName);
      writeTheFile();
      this->useDefaultHighlighter(fileDoc);

      // Notify observers of the file name and highlighter change.  This
      // includes myself.
      m_editorGlobal->m_documentList.notifyAttributeChanged(fileDoc);

      return;
    }
  }

  // Never reached.

  GENERIC_CATCH_END
}


void EditorWindow::fileClose() NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  NamedTextDocument *b = currentDocument();
  if (b->unsavedChanges()) {
    stringBuilder msg;
    msg << "The document " << b->documentName() << " has unsaved changes.  "
        << "Discard these changes and close it anyway?";
    if (!this->okToDiscardChanges(msg)) {
      return;
    }
    if (!stillCurrentDocument(b)) {
      return;
    }
  }

  m_editorGlobal->deleteDocumentFile(b);

  GENERIC_CATCH_END
}


void EditorWindow::fileToggleReadOnly() NOEXCEPT
{
  GENERIC_CATCH_BEGIN
  editorWidget()->setReadOnly(!editorWidget()->isReadOnly());
  GENERIC_CATCH_END
}


bool EditorWindow::reloadCurrentDocumentIfChanged()
{
  NamedTextDocument *doc = this->currentDocument();

  if (doc->hasFilename() && !doc->unsavedChanges()) {
    // Query the file modification time.
    std::unique_ptr<VFS_FileStatusReply> reply(
      getFileStatusSynchronously(vfsConnections(), this, doc->harn()));
    if (!reply) {
      return false;    // Canceled.
    }
    if (!stillCurrentDocument(doc)) {
      return false;
    }

    if (reply->m_success) {
      if (reply->m_fileModificationTime != doc->m_lastFileTimestamp) {
        TRACE("modification",
          "File " << doc->documentName() << " has changed on disk "
          "and has no unsaved changes; reloading it.");

        return reloadCurrentDocument();
      }
    }
    else {
      // Ignore the failure to read during automatic reload.  At some
      // point the user will make an explicit request to read or write,
      // and the problem will be reported then.
      TRACE("modification",
        "Reload failed: " << reply->m_failureReasonString);
    }
  }

  return false;
}


bool EditorWindow::reloadCurrentDocument()
{
  TRACE("EditorWindow", "reloadCurrentDocument");

  bool ret = m_editorGlobal->reloadDocumentFile(this, currentDocument());

  if (ret) {
    // Redraw file contents, update status bar including search hit
    // counts, etc., and remove "[DISKMOD]" from title bar.
    editorWidget()->redraw();
  }

  return ret;
}


void EditorWindow::fileReload() NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  NamedTextDocument const *doc = currentDocument();
  if (doc->unsavedChanges()) {
    // Prompt the user.
    QMessageBox box(this);
    box.setObjectName("refreshSafetyCheck_box");
    box.setWindowTitle("File Changed");
    box.setText(toQString(stringb(
      "The document " << doc->documentName() << " has unsaved changes.  "
      "Discard those changes and refresh from disk anyway?")));
    box.addButton(QMessageBox::Yes);
    box.addButton(QMessageBox::Cancel);
    int ret = box.exec();
    if (ret == QMessageBox::Yes) {
      // Go ahead with the refresh.
    }
    else {
      // Cancel the refresh.
      return;
    }
  }

  reloadCurrentDocument();

  GENERIC_CATCH_END
}


// Issue a request to get the latest on-disk timestamp in order to see
// if it has been modified there.  This command primarily exists in
// order to facilitate testing of the file reload mechanism.
void EditorWindow::fileCheckForChanges() NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  editorWidget()->requestFileStatus();

  GENERIC_CATCH_END
}


void EditorWindow::fileLaunchCommand() NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  QString command;
  bool prefixStderrLines;
  if (!promptForCommandLine(command, prefixStderrLines, ECLF_RUN)) {
    return;
  }

  innerLaunchCommand(
    currentDocument()->hostName(),
    toQString(editorWidget()->getDocumentDirectory()),
    prefixStderrLines,
    command);

  GENERIC_CATCH_END
}


void EditorWindow::innerLaunchCommand(
  HostName const &hostName,
  QString dir,
  bool prefixStderrLines,
  QString command)
{
  bool stillRunning = false;
  NamedTextDocument *doc = m_editorGlobal->launchCommand(
    hostName,
    dir,
    prefixStderrLines,
    command,
    stillRunning /*OUT*/);

  this->setDocumentFile(doc);

  if (!stillRunning) {
    editorWidget()->initCursorForProcessOutput();

    // Choose a highlighter based on the command line.
    this->useDefaultHighlighter(doc);
  }
}


void EditorWindow::fileRunMake() NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  // Warn if there are unsaved files.  Sometimes I forget to save files
  // before building, resulting in errors that due to trying to compile
  // files that have old content.
  if (editorGlobal()->m_documentList.hasUnsavedFiles()) {
    auto response = QMessageBox::question(editorWidget(),
      "Unsaved Files",
      "There are unsaved files.  Build anyway?");
    if (response != QMessageBox::Yes) {
      return;
    }
  }

  string dir = editorWidget()->getDocumentDirectory();

  // My intent is the user creates a script with this name on their
  // $PATH.  Then the script can do whatever is desired here.
  innerLaunchCommand(
    currentDocument()->hostName(),
    toQString(dir),
    false /*prefixStderrLines*/,
    "run-make-from-editor");

  GENERIC_CATCH_END
}


void EditorWindow::fileKillProcess() NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  NamedTextDocument *doc = this->currentDocument();
  DocumentProcessStatus dps = doc->documentProcessStatus();

  switch (dps) {
    default:
      DEV_WARNING("bad dps");
      // fallthrough

    case DPS_NONE:
      messageBox(this, "Not a Process Document", qstringb(
        "The document " << doc->documentName() << " was not produced by "
        "running a process, so there is nothing to kill."));
      break;

    case DPS_RUNNING:
      if (questionBoxYesCancel(this, "Kill Process?", qstringb(
            "Kill the process " << doc->documentName() << "?"))) {
        if (this->stillCurrentDocument(doc)) {
          string problem = m_editorGlobal->killCommand(doc);
          if (!problem.empty()) {
            messageBox(this, "Problem Killing Process",
              toQString(problem));
          }
        }
      }
      break;

    case DPS_FINISHED:
      messageBox(this, "Process Finished", qstringb(
        "The process " << doc->documentName() << " has already terminated."));
      break;
  }

  GENERIC_CATCH_END
}


void EditorWindow::fileManageConnections() NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  m_editorGlobal->showConnectionsDialog();

  GENERIC_CATCH_END
}


bool EditorWindow::canQuitApplication()
{
  stringBuilder msg;
  int ct = getUnsavedChanges(msg);

  if (ct > 0) {
    msg << "\nDiscard these changes and quit anyway?";
    return this->okToDiscardChanges(msg);
  }

  return true;
}


int EditorWindow::getUnsavedChanges(stringBuilder &msg)
{
  int ct = 0;

  msg << "The following documents have unsaved changes:\n\n";
  for (int i=0; i < this->m_editorGlobal->m_documentList.numDocuments(); i++) {
    NamedTextDocument *file = this->m_editorGlobal->m_documentList.getDocumentAt(i);
    if (file->unsavedChanges()) {
      ct++;
      msg << " * " << file->resourceName() << '\n';
    }
  }

  return ct;
}


bool EditorWindow::okToDiscardChanges(string const &descriptionOfChanges)
{
  QMessageBox box(this);
  box.setObjectName("okToDiscardChanges_box");
  box.setWindowTitle("Unsaved Changes");
  box.setText(toQString(descriptionOfChanges));
  box.addButton(QMessageBox::Discard);
  box.addButton(QMessageBox::Cancel);
  int ret = box.exec();
  return (ret == QMessageBox::Discard);
}


void EditorWindow::searchPanelChanged(SearchAndReplacePanel *panel)
{
  // Pass this on to the SAR panel.  It will check if 'panel==this'
  // before otherwise reacting (just to keep all the logic in one
  // place).
  m_sarPanel->searchPanelChanged(panel);
}


bool EditorWindow::promptForCommandLine(
  QString /*OUT*/ &command,
  bool /*OUT*/ &prefixStderrLines,
  EditorCommandLineFunction whichFunction)
{
  ApplyCommandDialog dlg(editorWidget(), whichFunction);
  if (!dlg.exec()) {
    return false;
  }

  // Get dialog results.
  command = dlg.getSpecifiedCommand();
  bool useSubst = dlg.isSubstitutionEnabled();
  prefixStderrLines = dlg.isPrefixStderrEnabled();

  // Add the command to the history before substituting.
  editorGlobal()->settings_addHistoryCommand(
    editorWidget(), whichFunction,
    toString(command), useSubst, prefixStderrLines);

  if (useSubst) {
    command = toQString(
      currentDocument()->applyCommandSubstitutions(toString(command)));
  }

  return true;
}


void EditorWindow::namedTextDocumentAdded(
  NamedTextDocumentList *, NamedTextDocument *) NOEXCEPT
{}

void EditorWindow::namedTextDocumentRemoved(
  NamedTextDocumentList *, NamedTextDocument *) NOEXCEPT
{
  // It is possible that the file our widget is editing is being
  // removed, in which case the widget will switch to another file and
  // we will have to update the filename display.  But it would be
  // wrong to assume here that the widget has already updated to the
  // new file since the order in which observers are notified is
  // undefined.
  //
  // Instead, those updates happen in response to the 'viewChanged'
  // signal, which the widget will emit when it learns of the change.
}

void EditorWindow::namedTextDocumentAttributeChanged(
  NamedTextDocumentList *, NamedTextDocument *) NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  // The title of the file we are looking at could have changed.
  this->editorViewChanged();

  // The highlighter might have changed too.
  editorWidget()->update();

  GENERIC_CATCH_END
}

void EditorWindow::namedTextDocumentListOrderChanged(
  NamedTextDocumentList *) NOEXCEPT
{}


void EditorWindow::fileLoadSettings() NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  if (editorGlobal()->loadSettingsFile(this)) {
    std::string fname = editorGlobal()->getSettingsFileName();
    inform(stringbc("Loaded settings from " << doubleQuote(fname) << "."));
  }

  GENERIC_CATCH_END
}


void EditorWindow::fileSaveSettings() NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  if (editorGlobal()->saveSettingsFile(this)) {
    std::string fname = editorGlobal()->getSettingsFileName();
    inform(stringbc("Saved settings to " << doubleQuote(fname) << "."));
  }

  GENERIC_CATCH_END
}


void EditorWindow::fileExit() NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  if (this->canQuitApplication()) {
    EditorGlobal::quit();
  }

  GENERIC_CATCH_END
}


void EditorWindow::editUndo() NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  editorWidget()->editUndo();

  GENERIC_CATCH_END
}

void EditorWindow::editRedo() NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  editorWidget()->editRedo();

  GENERIC_CATCH_END
}

void EditorWindow::editCut() NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  editorWidget()->commandEditCut();

  GENERIC_CATCH_END
}

void EditorWindow::editCopy() NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  editorWidget()->commandEditCopy();

  GENERIC_CATCH_END
}

void EditorWindow::editPaste() NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  editorWidget()->commandEditPaste();

  GENERIC_CATCH_END
}

void EditorWindow::editDelete() NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  editorWidget()->commandEditDelete();

  GENERIC_CATCH_END
}

void EditorWindow::editKillLine() NOEXCEPT
{
  GENERIC_CATCH_BEGIN
  editorWidget()->commandEditKillLine();
  GENERIC_CATCH_END
}


void EditorWindow::editSelectEntireFile() NOEXCEPT
{
  GENERIC_CATCH_BEGIN
  editorWidget()->commandEditSelectEntireFile();
  GENERIC_CATCH_END
}


void EditorWindow::editSearch() NOEXCEPT
{
  GENERIC_CATCH_BEGIN
  m_sarPanel->toggleSARFocus();
  GENERIC_CATCH_END
}


void EditorWindow::editReplace() NOEXCEPT
{
  GENERIC_CATCH_BEGIN
  m_sarPanel->editReplace(false /*advanceOnReplace*/);
  GENERIC_CATCH_END
}


void EditorWindow::editReplaceAndNext() NOEXCEPT
{
  GENERIC_CATCH_BEGIN
  m_sarPanel->editReplace(true /*advanceOnReplace*/);
  GENERIC_CATCH_END
}


void EditorWindow::editNextSearchHit() NOEXCEPT
{
  GENERIC_CATCH_BEGIN
  editorWidget()->nextSearchHit(false /*reverse*/);
  GENERIC_CATCH_END
}


void EditorWindow::editPreviousSearchHit() NOEXCEPT
{
  GENERIC_CATCH_BEGIN
  editorWidget()->nextSearchHit(true /*reverse*/);
  GENERIC_CATCH_END
}


void EditorWindow::editGotoLine() NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  // 2022-07-08: Previously, I used TextInputDialog to get history
  // services, but I then found that history for goto-line is a nuisance
  // in the UI (especially auto-completion), and almost never of any
  // use.  So, now this just uses an ordinary text input dialog.
  //
  // I do not use 'QInputDialog::getInt' because I don't want additional
  // clutter and defaults related to integers.

  bool ok;
  QString text = QInputDialog::getText(this,
    "Goto Line",
    "Line number:",
    QLineEdit::Normal,
    "",
    &ok);

  if (ok) {
    string s = toString(text);
    if (!s.empty()) {
      int n = atoi(s);
      if (n > 0) {
        editorWidget()->cursorTo(TextLCoord(n-1, 0));
        editorWidget()->scrollToCursor(-1 /*center*/);
      }
      else {
        this->complain(stringbc("Invalid line number: " << s));
      }
    }
  }

  GENERIC_CATCH_END
}


void EditorWindow::editGrepSource() NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  string searchText = editorWidget()->getSelectedOrIdentifier();
  if (searchText.empty()) {
    messageBox(this, "No Search Text Provided",
      "To use this feature, either select some text to search for, or "
      "put the text cursor on an identifier and that will act as the "
      "search text.  You also need a program called \"grepsrc\" in "
      "the PATH, as that is what the search string is passed to.");
  }
  else {
    string dir = editorWidget()->getDocumentDirectory();
    innerLaunchCommand(
      currentDocument()->hostName(),
      toQString(dir),
      true /*prefixStderrLines*/,
      qstringb("grepsrc " << shellDoubleQuote(searchText)));
  }

  GENERIC_CATCH_END
}


void EditorWindow::editRigidIndent() NOEXCEPT
{
  GENERIC_CATCH_BEGIN
  editorWidget()->commandEditRigidIndent();
  GENERIC_CATCH_END
}


void EditorWindow::editRigidUnindent() NOEXCEPT
{
  GENERIC_CATCH_BEGIN
  editorWidget()->commandEditRigidUnindent();
  GENERIC_CATCH_END
}


void EditorWindow::editJustifyParagraph() NOEXCEPT
{
  GENERIC_CATCH_BEGIN
  editorWidget()->editJustifyParagraph();
  GENERIC_CATCH_END
}


void EditorWindow::editApplyCommand() NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  // Object to manage the child process.
  CommandRunner runner;

  // The default timeout of 2s is too small.  On my Windows desktop, it
  // is common for a command I have not run in a while to take several
  // seconds to complete due to (e.g.) the Python interpreter having to
  // be loaded.  For now, just increase the timeout.
  //
  // TODO: Use the asynchronous interface instead, and show a proper
  // progress dialog.
  runner.m_synchronousTimeLimitMS = 10000;

  // The command the user wants to run.
  QString commandString;

  // The active editor when the command was started.
  TextDocumentEditor *tde = NULL;

  // Inside this block are variables and code used before the
  // child process is launched.  The block ends when the child
  // process terminates.  At that point, all variables in here
  // are suspect because we pumped the event queue while waiting,
  // so the user could have done pretty much anything.
  //
  // Only the variables declared above can be used after the
  // child exits, and even then only with care.
  {
    bool dummy;
    if (!promptForCommandLine(commandString, dummy, ECLF_APPLY)) {
      return;      // Canceled.
    }

    tde = editorWidget()->getDocumentEditor();
    string input = tde->getSelectedText();

    // Set the working directory and command of 'runner'.
    string dir = editorWidget()->getDocumentDirectory();
    HostName hostName = editorWidget()->getDocument()->hostName();
    m_editorGlobal->configureCommandRunner(runner,
      hostName, toQString(dir), commandString);

    // TODO: This mishandles NUL bytes.
    runner.setInputData(QByteArray(input.c_str()));

    // It would be bad to hold an undo group open while we pump
    // the event queue.
    xassert(!tde->inUndoGroup());

    // Both the window and the widget have to have their cursor
    // changed, the latter (I think) because it already has a
    // non-standard cursor set.
    CursorSetRestore csr(this, Qt::WaitCursor);
    CursorSetRestore csr2(editorWidget(), Qt::WaitCursor);

    // This blocks until the program terminates or times out.  However,
    // it will pump the GUI event queue while waiting.
    //
    // TODO: Block input events?
    //
    // TODO: Make timeout adjustable.
    runner.startAndWait();
  }

  if (runner.getFailed()) {
    QMessageBox mb;
    mb.setWindowTitle("Command Failed");
    mb.setText(qstringb(
      "The command \"" << toString(commandString) <<
      "\" failed: " << toString(runner.getErrorMessage()) <<
      (runner.getErrorData().isEmpty()?
        "\n\nThere was no error output." :
        "\n\nSee details for its error output.")));
    mb.setDetailedText(QString::fromUtf8(runner.getErrorData()));
    mb.exec();
    return;
  }

  // We just pumped the event queue.  The editor we had before
  // could have gone away.
  if (tde != editorWidget()->getDocumentEditor()) {
    QMessageBox::warning(this, "Editor Changed", qstringb(
      "While running command \"" << commandString <<
      "\", the active editor changed.  I will discard "
      "the output of that command."));
    return;
  }

  // Replace the selected text with the command's output.
  //
  // 2024-01-16: I previously passed ITF_SELECT_AFTERWARD in order to
  // leave the new text selected so it could easily be further
  // manipulated.  However, there is then no clear indication of when
  // the command has finished, since the UI only changes to the extent
  // that the new text is different.  Therefore, I've reverted that
  // change until I can design a way to better indicate completion.
  editorWidget()->insertText(runner.getOutputData().constData(),
                             runner.getOutputData().size());

  // For error output or non-zero exit code, we show a warning, but
  // still insert the text.  Note that we do this *after* inserting
  // the text because showing a dialog is another way to pump the
  // event queue.
  if (!runner.getErrorData().isEmpty()) {
    QMessageBox mb;
    mb.setWindowTitle("Command Error Output");
    mb.setText(qstringb(
      "The command \"" << commandString <<
      "\" exited with code " << runner.getExitCode() <<
      " and produced some error output."));
    mb.setDetailedText(QString::fromUtf8(runner.getErrorData()));
    mb.exec();
  }
  else if (runner.getExitCode() != 0) {
    QMessageBox mb;
    mb.setWindowTitle("Command Exit Code");
    mb.setText(qstringb(
      "The command \"" << commandString <<
      "\" exited with code " << runner.getExitCode() <<
      ", although it produced no error output."));
    mb.exec();
  }

  GENERIC_CATCH_END
}


void EditorWindow::editInsertDateTime() NOEXCEPT
{
  GENERIC_CATCH_BEGIN
  editorWidget()->editInsertDateTime();
  GENERIC_CATCH_END
}


#define CHECKABLE_MENU_TOGGLE(actionField, sourceBool)  \
  (sourceBool) = !(sourceBool);                         \
  this->actionField->setChecked(sourceBool);            \
  editorWidget()->update() /* user ; */


void EditorWindow::viewToggleVisibleWhitespace() NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  CHECKABLE_MENU_TOGGLE(m_toggleVisibleWhitespaceAction,
    editorWidget()->m_visibleWhitespace);

  GENERIC_CATCH_END
}


void EditorWindow::viewSetWhitespaceOpacity() NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  bool ok;
  int n = QInputDialog::getInt(this,
    "Visible Whitespace",
    "Opacity in [1,255]:",
    editorWidget()->m_whitespaceOpacity,
    1 /*min*/, 255 /*max*/, 1 /*step*/, &ok);
  if (ok) {
    editorWidget()->m_whitespaceOpacity = n;
    editorWidget()->update();
  }

  GENERIC_CATCH_END
}


void EditorWindow::viewToggleVisibleSoftMargin() NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  CHECKABLE_MENU_TOGGLE(m_toggleVisibleSoftMarginAction,
    editorWidget()->m_visibleSoftMargin);

  GENERIC_CATCH_END
}


void EditorWindow::viewSetSoftMarginColumn() NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  bool ok;
  int n = QInputDialog::getInt(this,
    "Soft Margin Column",
    "Column number (positive):",
    editorWidget()->m_softMarginColumn+1,
    1 /*min*/, INT_MAX /*max*/, 1 /*step*/, &ok);
  if (ok) {
    editorWidget()->m_softMarginColumn = n-1;
    editorWidget()->update();
  }

  GENERIC_CATCH_END
}


void EditorWindow::viewToggleHighlightTrailingWS() NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  editorWidget()->toggleHighlightTrailingWhitespace();

  // Includes firing 'editorViewChanged'.
  editorWidget()->redraw();

  GENERIC_CATCH_END
}


#undef CHECKABLE_MENU_TOGGLE


void EditorWindow::viewSetHighlighting() NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  NamedTextDocument *doc = this->currentDocument();

  QInputDialog dialog(this);
  dialog.setWindowTitle("Set Highlighting");
  dialog.setLabelText("Highlighting to use for this file:");
  dialog.setComboBoxItems(QStringList() <<
    "None" <<
    "C/C++" <<
    "Diff" <<
    "HashComment" <<
    "Makefile" <<
    "OCaml" <<
    "Python");

  // One annoying thing is you can't double-click an item to choose
  // it and simultaneously close the dialog.
  dialog.setOption(QInputDialog::UseListViewForComboBoxItems);

  if (doc->m_highlighter) {
    dialog.setTextValue(toQString(doc->m_highlighter->highlighterName()));
  }

  if (!dialog.exec()) {
    return;
  }

  if (!stillCurrentDocument(doc)) {
    return;
  }

  // The QInputDialog documentation is incomplete.  It says that
  // 'textValue' is only used in TextInput mode without clarifying that
  // comboBox mode is a form of TextInput mode.  I determined that by
  // reading the source code.
  QString chosen = dialog.textValue();

  // We are going to replace the highlighter (even if we replace it with
  // the same style), so remove the old one.
  delete doc->m_highlighter;
  doc->m_highlighter = NULL;

  // TODO: Obviously this is not a good method of recognizing the chosen
  // element, nor a scalable registry of available highlighters.
  if (chosen == "C/C++") {
    doc->m_highlighter = new C_Highlighter(doc->getCore());
  }
  else if (chosen == "Diff") {
    doc->m_highlighter = new DiffHighlighter();
  }
  else if (chosen == "HashComment") {
    doc->m_highlighter = new HashComment_Highlighter(doc->getCore());
  }
  else if (chosen == "Makefile") {
    doc->m_highlighter = new Makefile_Highlighter(doc->getCore());
  }
  else if (chosen == "OCaml") {
    doc->m_highlighter = new OCaml_Highlighter(doc->getCore());
  }
  else if (chosen == "Python") {
    doc->m_highlighter = new Python_Highlighter(doc->getCore());
  }
  else {
    // We use no highlighter.
  }

  // Notify everyone of the change.
  this->m_editorGlobal->m_documentList.notifyAttributeChanged(doc);

  GENERIC_CATCH_END
}


void EditorWindow::viewSetApplicationFont() NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  QFont oldFont = qApp->font();

  bool ok = false;
  QFontDialog::FontDialogOptions options;
  QFont newFont = QFontDialog::getFont(
    &ok /*OUT*/,
    oldFont,
    this,
    "Editor Application Font",
    options);

  if (ok) {
    qApp->setFont(newFont);

    // This was an attempt to get the scroll bar thumb to increase its
    // height to match the new width, but it did not work.
    #if 0
    int newHeight = QFontMetrics(newFont).height();
    TRACE("EditorWindow", "new global strut height: " << newHeight);
    qApp->setGlobalStrut(QSize(newHeight, newHeight));;
    #endif
  }

  GENERIC_CATCH_END
}


void EditorWindow::viewSetEditorFont() NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  FontsDialog dialog(this, editorGlobal());
  dialog.exec();

  GENERIC_CATCH_END
}


void EditorWindow::macroCreateMacro() NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  MacroCreatorDialog dlg(editorGlobal());
  if (dlg.exec()) {
    TRACE("macro", "macro name: " << doubleQuote(dlg.getMacroName()));
    TRACE("macro", "commands:\n" << serializeECV(dlg.getChosenCommands()));

    editorGlobal()->settings_addMacro(editorWidget(),
      dlg.getMacroName(), dlg.getChosenCommands());
  }

  GENERIC_CATCH_END
}


void EditorWindow::macroRunDialog() NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  MacroRunDialog dlg(editorGlobal());
  if (dlg.exec()) {
    std::string name = dlg.getMacroName();
    TRACE("macro", "chosen macro to run: " << doubleQuote(name));

    editorWidget()->runMacro(name);
    editorGlobal()->settings_setMostRecentlyRunMacro(editorWidget(), name);
  }

  GENERIC_CATCH_END
}


void EditorWindow::macroRunMostRecent() NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  std::string name =
    editorGlobal()->settings_getMostRecentlyRunMacro(editorWidget());
  if (!name.empty()) {
    editorWidget()->runMacro(name);
  }
  else {
    inform("There is no recently run macro.");
  }

  GENERIC_CATCH_END
}


void EditorWindow::lspStartServer() NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  bool dummy;
  inform(lspManager().startServer(dummy));

  GENERIC_CATCH_END
}


void EditorWindow::lspStopServer() NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  inform(lspManager().stopServer());

  GENERIC_CATCH_END
}


void EditorWindow::lspCheckStatus() NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  inform(lspManager().checkStatus());

  GENERIC_CATCH_END
}


void EditorWindow::lspOpenFile() NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  NamedTextDocument const *ntd = currentDocument();
  DocumentName const &docName = ntd->documentName();

  if (!docName.isLocalFilename()) {
    inform("LSP only works with local files.");
    return;
  }

  std::string fname = docName.filename();
  std::string languageId = "cpp";        // TODO: Compute this properly.

  int version;
  try {
    version = safeToInt(ntd->getVersionNumber());
  }
  catch (XNumericConversion &x) {
    complain(stringb(
      "The version number cannot be represented as an LSP int: " <<
      x.what()));
    return;
  }

  std::string contents = vectorOfUCharToString(ntd->getWholeFile());

  if (lspManager().isFileOpen(fname)) {
    inform(stringb("Document " << doubleQuote(fname) <<
                   " is already open."));
  }
  else {
    lspManager().notify_textDocument_didOpen(
      fname, languageId, version, std::move(contents));
  }

  GENERIC_CATCH_END
}


void EditorWindow::lspUpdateFile() NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  inform("TODO");

  GENERIC_CATCH_END
}


void EditorWindow::lspCloseFile() NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  inform("TODO");

  GENERIC_CATCH_END
}


void EditorWindow::lspReviewDiagnostics() NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  inform("TODO");

  GENERIC_CATCH_END
}


void EditorWindow::viewFontHelp() NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  QMessageBox mb;
  mb.setWindowTitle("Editor Fonts");
  mb.setText(
"The application font affects the menus, status bar, and dialogs, \
although the main menu bar and status bar are only affected when a new \
window is opened.\n\
\n\
It is possible to set an initial application font size by setting the \
envvar EDITOR_APP_FONT_POINT_SIZE before starting the editor.  Setting \
that envvar also affects the width of the scroll bar, whereas changing \
the font via the menu does not affect the scroll bar.\n\
\n\
The editor font only affects the text inside the main editing area. \
The larger font can be chosen initially by setting envvar \
EDITOR_USE_LARGE_FONT."
  );
  mb.exec();

  GENERIC_CATCH_END
}



void EditorWindow::windowOpenFilesList() NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  // Put the current document on top before opening the dialog so one
  // can always hit Ctrl+O, Enter and the displayed document won't
  // change.
  makeCurrentDocumentTopmost();

  NamedTextDocument *doc = m_editorGlobal->runOpenFilesDialog(this);
  if (doc) {
    this->setDocumentFile(doc);
  }

  GENERIC_CATCH_END
}


void EditorWindow::windowPreviousFile() NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  NamedTextDocumentList &docList = m_editorGlobal->m_documentList;
  if (docList.numDocuments() > 1) {
    NamedTextDocument *current = this->currentDocument();
    NamedTextDocument *previous = docList.getDocumentAt(0);
    if (current == previous) {
      // The current document is already at the top, so use the one
      // underneath it.
      previous = docList.getDocumentAt(1);
    }

    this->setDocumentFile(previous);
  }
  else {
    // There is only one document, so just ignore the command.
  }

  GENERIC_CATCH_END
}


void EditorWindow::windowOccupyLeft() NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  if (QApplication::desktop()->width() == 1024) {  // 1024x768
    setGeometry(83, 49, 465, 660);
  }
  else {    // 1280x1024
    setGeometry(83, 59, 565, 867);
  }

  GENERIC_CATCH_END
}


void EditorWindow::windowOccupyRight() NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  if (QApplication::desktop()->width() == 1024) {  // 1024x768
    setGeometry(390, 49, 630, 660);
  }
  else {    // 1280x1024
    setGeometry(493, 59, 783, 867);
  }

  GENERIC_CATCH_END
}


void EditorWindow::helpKeybindings() NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  KeysDialog d(doc_keybindings, this);
  d.exec();

  GENERIC_CATCH_END
}


void EditorWindow::helpAbout() NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  QMessageBox::about(this, "About Scott's Editor",
    qstringb(
      "This is a text editor that has a user interface I like.\n"
      "\n"
      "Version: " << editor_git_version));

  GENERIC_CATCH_END
}


void EditorWindow::helpAboutQt() NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  QMessageBox::aboutQt(this, "An editor");

  GENERIC_CATCH_END
}


void EditorWindow::helpDebugDumpWindowObjectTree() NOEXCEPT
{
  GENERIC_CATCH_BEGIN
  this->dumpObjectTree();
  GENERIC_CATCH_END
}


void EditorWindow::helpDebugDumpApplicationObjectTree() NOEXCEPT
{
  GENERIC_CATCH_BEGIN
  qApp->dumpObjectTree();
  GENERIC_CATCH_END
}


void EditorWindow::helpDebugEditorScreenshot() NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  QImage image(editorWidget()->getScreenshot());

  QString fname(qstringb("screenshot-" << getCurrentUnixTime() << ".png"));
  if (!image.save(fname, "PNG")) {
    // This API does not provide a reason...
    cout << "Failed to write " << fname << endl;
  }
  else {
    cout << "Wrote screenshot to " << fname << endl;
  }

  GENERIC_CATCH_END
}


void EditorWindow::editorViewChanged() NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  TRACE("EditorWindow", "editorViewChanged");

  m_editorWidgetFrame->setScrollbarRangesAndValues();

  // I want the user to interact with line/col with a 1:1 origin,
  // even though the TextDocument interface uses 0:0.
  m_statusArea->m_cursor->setText(qstringb(
    (editorWidget()->cursorLine()+1) << ':' <<
    (editorWidget()->cursorCol()+1)));

  // Status text: full document name plus status indicators.
  NamedTextDocument *file = currentDocument();
  m_statusArea->setFilenameText(toQString(
    file->nameWithStatusIndicators()));

  // Window title.
  stringBuilder sb;
  sb << file->m_title
     << file->fileStatusString()
     << " - " << EditorGlobal::appName;
  this->setWindowTitle(toQString(sb));

  // Trailing whitespace menu checkbox.
  this->m_toggleHighlightTrailingWSAction->setChecked(
    editorWidget()->highlightTrailingWhitespace());

  // Read-only menu checkbox.
  m_toggleReadOnlyAction->setChecked(editorWidget()->isReadOnly());

  GENERIC_CATCH_END
}


void EditorWindow::slot_editorFontChanged() NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  TRACE("EditorWindow", "slot_editorFontChanged");

  editorWidget()->setFontsFromEditorGlobal();
  editorWidget()->redraw();

  GENERIC_CATCH_END
}


void EditorWindow::on_closeSARPanel() NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  if (m_sarPanel->isVisible()) {
    m_sarPanel->hide();
    editorWidget()->setFocus();
  }

  GENERIC_CATCH_END
}


void EditorWindow::on_openFilenameInputDialogSignal(
  HostFileAndLineOpt hfl) NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  TRACE("EditorWindow",
    "on_openFilenameInputDialogSignal: harn=" << hfl.m_harn <<
    " line=" << hfl.m_line);

  if (!hfl.hasFilename()) {
    // Ignore empty object.
    return;
  }

  // Check for fast-open conditions.
  {
    SMFileUtil sfu;
    if (!sfu.endsWithDirectorySeparator(hfl.m_harn.resourceName()) &&
        hfl.m_harn != currentDocument()->documentName() &&
        checkFileExistenceSynchronously(hfl.m_harn)) {
      // The file exists, and it is not the current document.  Just
      // go straight to opening it without prompting.
      this->fileOpenFile(hfl.m_harn);
      if (hfl.m_line != 0) {
        // Also go to line number, if provided.
        editorWidget()->cursorTo(TextLCoord(hfl.m_line-1, 0));
        editorWidget()->clearMark();
        editorWidget()->scrollToCursor(-1 /*gap*/);
      }
      return;
    }
  }

  // Prompt to confirm.
  FilenameInputDialog dialog(
    &(m_editorGlobal->m_filenameInputDialogHistory),
    vfsConnections(),
    this);

  HostAndResourceName confirmedHarn = hfl.m_harn;

  if (dialog.runDialog(&(m_editorGlobal->m_documentList),
                       confirmedHarn)) {
    this->fileOpenFile(confirmedHarn);
  }

  GENERIC_CATCH_END
}


void EditorWindow::complain(std::string_view msg)
{
  QMessageBox::information(this, EditorGlobal::appName, toQString(msg));
}


void EditorWindow::inform(std::string_view msg)
{
  // On my system, QMessageBox::information rings the bell, and I do
  // not want that here.  Removing the icon seems to disable that.

  QMessageBox box;
  box.setIcon(QMessageBox::NoIcon);
  box.setText(toQString(msg));
  box.exec();
}


void EditorWindow::windowNewWindow() NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  EditorWindow *ed = this->m_editorGlobal->createNewWindow(this->currentDocument());
  ed->show();

  GENERIC_CATCH_END
}


void EditorWindow::windowCloseWindow() NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  // This sends 'closeEvent' and actually closes the window only if
  // the event is accepted.
  this->close();

  GENERIC_CATCH_END
}


void EditorWindow::closeEvent(QCloseEvent *event)
{
  if (this->m_editorGlobal->m_windows.count() == 1) {
    if (!this->canQuitApplication()) {
      event->ignore();    // Prevent app from closing.
      return;
    }

    // Close the connections dialog if it is open, since otherwise that
    // will prevent the program from terminating.
    m_editorGlobal->hideModelessDialogs();
  }

  event->accept();
}


// EOF
