// editor-window.cc
// code for editor-window.h

#include "editor-window.h"             // this module

// editor
#include "c_hilite.h"                  // C_Highlighter
#include "command-runner.h"            // CommandRunner
#include "diff-hilite.h"               // DiffHighlighter
#include "editor-global.h"             // EditorGlobalState
#include "editor-widget.h"             // EditorWidget
#include "filename-input.h"            // FilenameInputDialog
#include "git-version.h"               // editor_git_version
#include "hashcomment_hilite.h"        // HashComment_Highlighter
#include "keybindings.doc.gen.h"       // doc_keybindings
#include "keys-dialog.h"               // KeysDialog
#include "launch-command-dialog.h"     // LaunchCommandDialog
#include "makefile_hilite.h"           // Makefile_Highlighter
#include "pixmaps.h"                   // pixmaps
#include "qhboxframe.h"                // QHBoxFrame
#include "sar-panel.h"                 // SearchAndReplacePanel
#include "status.h"                    // StatusDisplay
#include "td-editor.h"                 // TextDocumentEditor
#include "textinput.h"                 // TextInputDialog
#include "vfs-query-sync.h"            // VFS_QuerySync
#include "vfs-msg.h"                   // VFS_ReadFileRequest

// smqtutil
#include "qtguiutil.h"                 // CursorSetRestore
#include "qtutil.h"                    // toQString

// smbase
#include "exc.h"                       // XOpen, GENERIC_CATCH_BEGIN/END
#include "mysig.h"                     // printSegfaultAddrs
#include "nonport.h"                   // fileOrDirectoryExists
#include "objcount.h"                  // CHECK_OBJECT_COUNT
#include "sm-file-util.h"              // SMFileUtil
#include "strutil.h"                   // dirname
#include "sm-test.h"                   // PVAL
#include "trace.h"                     // TRACE_ARGS

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
#include <QInputDialog>


static char const appName[] = "Editor";

int EditorWindow::s_objectCount = 0;

CHECK_OBJECT_COUNT(EditorWindow);


EditorWindow::EditorWindow(EditorGlobalState *theState, NamedTextDocument *initFile,
                           QWidget *parent)
  : QWidget(parent),
    m_globalState(theState),
    m_menuBar(NULL),
    m_editorWidget(NULL),
    m_sarPanel(NULL),
    m_vertScroll(NULL),
    m_horizScroll(NULL),
    m_statusArea(NULL),
    m_toggleReadOnlyAction(NULL),
    m_toggleVisibleWhitespaceAction(NULL),
    m_toggleVisibleSoftMarginAction(NULL),
    m_toggleHighlightTrailingWSAction(NULL)
{
  xassert(theState);
  xassert(initFile);

  // will build a layout tree to manage sizes of child widgets
  QVBoxLayout *mainArea = new QVBoxLayout();
  mainArea->setObjectName("mainArea");
  mainArea->setSpacing(0);
  mainArea->setContentsMargins(0, 0, 0, 0);

  this->m_menuBar = new QMenuBar();
  this->m_menuBar->setObjectName("m_menuBar");
  mainArea->addWidget(this->m_menuBar);

  QGridLayout *editArea = new QGridLayout();
  editArea->setObjectName("editArea");
  mainArea->addLayout(editArea, 1 /*stretch*/);

  m_sarPanel = new SearchAndReplacePanel();
  mainArea->addWidget(m_sarPanel);
  m_sarPanel->setObjectName("m_sarPanel");
  m_sarPanel->hide();      // Initially hidden.
  QObject::connect(
    m_sarPanel, &SearchAndReplacePanel::signal_searchPanelChanged,
    m_globalState, &EditorGlobalState::slot_broadcastSearchPanelChanged);

  this->m_statusArea = new StatusDisplay();
  this->m_statusArea->setObjectName("m_statusArea");
  mainArea->addWidget(this->m_statusArea);

  // Put a black one-pixel frame around the editor widget.
  QHBoxFrame *editorFrame = new QHBoxFrame();
  editorFrame->setObjectName("editorFrame");
  editorFrame->setFrameStyle(QFrame::Box);
  editArea->addWidget(editorFrame, 0 /*row*/, 0 /*col*/);

  this->m_editorWidget = new EditorWidget(initFile,
    &(theState->m_documentList), this->m_statusArea);
  this->m_editorWidget->setObjectName("m_editorWidget");
  editorFrame->addWidget(this->m_editorWidget);
  this->m_editorWidget->setFocus();
  QObject::connect(this->m_editorWidget, &EditorWidget::viewChanged,
                   this, &EditorWindow::editorViewChanged);
  QObject::connect(this->m_editorWidget, &EditorWidget::closeSARPanel,
                   this, &EditorWindow::on_closeSARPanel);

  // See EditorWidget::fileOpenAtCursor for why this is a
  // QueuedConnection.
  QObject::connect(
    this->m_editorWidget, &EditorWidget::openFilenameInputDialogSignal,
    this, &EditorWindow::on_openFilenameInputDialogSignal,
    Qt::QueuedConnection);

  // See explanation in EditorGlobalState::focusChangedHandler().
  this->setFocusProxy(this->m_editorWidget);

  // Needed to ensure Tab gets passed down to the editor widget.
  this->m_editorWidget->installEventFilter(this);

  // Connect these, which had to wait until both were constructed.
  m_sarPanel->setEditorWidget(this->m_editorWidget);

  this->m_vertScroll = new QScrollBar(Qt::Vertical);
  this->m_vertScroll->setObjectName("m_vertScroll");
  editArea->addWidget(this->m_vertScroll, 0 /*row*/, 1 /*col*/);
  QObject::connect(this->m_vertScroll, &QScrollBar::valueChanged,
                   this->m_editorWidget, &EditorWidget::scrollToLine);

  // disabling horiz scroll for now..
  //m_horizScroll = new QScrollBar(QScrollBar::Horizontal, editArea, "horizontal scrollbar");
  //QObject::connect(m_horizScroll, &QScrollBar::valueChanged, editor, &EditorWidget::scrollToCol);

  this->buildMenu();

  this->setWindowIcon(pixmaps->icon);

  this->setLayout(mainArea);
  this->setGeometry(400,100,      // initial location
                    800,800);     // initial size

  // Set scrollbar ranges, status bar text, and window title.
  this->updateForChangedFile();

  // I want this object destroyed when it is closed.
  this->setAttribute(Qt::WA_DeleteOnClose);

  this->m_globalState->m_windows.append(this);
  this->m_globalState->m_documentList.addObserver(this);

  EditorWindow::s_objectCount++;
}


EditorWindow::~EditorWindow()
{
  EditorWindow::s_objectCount--;

  m_globalState->m_documentList.removeObserver(this);

  // This object might have already been removed, for example because
  // the EditorGlobalState destructor is running, and is in the process of
  // removing elements from the list and destroying them.  Hence the
  // "IfPresent" part of this call.
  m_globalState->m_windows.removeIfPresent(this);

  // The QObject destructor will destroy both 'm_sarPanel' and
  // 'm_editorWidget', but the documentation of ~QObject does not
  // specify an order.  Disconnect them here so that either order works.
  m_sarPanel->setEditorWidget(NULL);

  // See doc/signals-and-dtors.txt.
  QObject::disconnect(m_sarPanel,     NULL, m_globalState,  NULL);
  QObject::disconnect(m_editorWidget, NULL, this,           NULL);
  QObject::disconnect(m_vertScroll,   NULL, m_editorWidget, NULL);
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

    {
      QMenu *submenu = menu->addMenu("Open with traditional dialog");
      QMenu *menu = submenu;
      MENU_ITEM    ("Native ...", fileOpenNativeDialog);
      MENU_ITEM    ("Qt ...", fileOpenQtDialog);
    }

    MENU_ITEM_KEY("&Save", fileSave, Qt::Key_F2);
    MENU_ITEM    ("Save &as ...", fileSaveAs);
    MENU_ITEM    ("&Close", fileClose);

    menu->addSeparator();

    CHECKABLE_ACTION(m_toggleReadOnlyAction,
                  "Read only", fileToggleReadOnly, false /*initChecked*/);
    MENU_ITEM    ("&Reload", fileReload);
    MENU_ITEM    ("Reload a&ll", fileReloadAll);

    menu->addSeparator();

    MENU_ITEM_KEY("&Launch (run) command ...", fileLaunchCommand,
                  Qt::ALT + Qt::Key_R);
    MENU_ITEM_KEY("Run \"run-&make-from-editor\"", fileRunMake, Qt::Key_F9);
    MENU_ITEM    ("Kill running process ...", fileKillProcess);

    menu->addSeparator();

    MENU_ITEM    ("E&xit", fileExit);
  }

  {
    QMenu *menu = this->m_menuBar->addMenu("&Edit");
    menu->setObjectName("editMenu");

    // Used shortcut letters: ACDJGKLNPRSTU

    MENU_ITEM_KEY("&Undo", editUndo, Qt::ALT + Qt::Key_Backspace);
    MENU_ITEM_KEY("&Redo", editRedo, Qt::ALT + Qt::SHIFT + Qt::Key_Backspace);

    menu->addSeparator();

    // Some of these items have another shortcut (e.g., Shift+Delete
    // for "Cut"), but even with QAction::setShortcuts they cannot be
    // shown in the menu item, so I do not bind them here.
    MENU_ITEM_KEY("Cu&t", editCut, Qt::CTRL + Qt::Key_X);
    MENU_ITEM_KEY("&Copy", editCopy, Qt::CTRL + Qt::Key_C);
    MENU_ITEM_KEY("&Paste", editPaste, Qt::CTRL + Qt::Key_V);
    MENU_ITEM    ("&Delete\tDelete", editDelete);
    MENU_ITEM_KEY("&Kill (cut) current line", editKillLine,
                  Qt::CTRL + Qt::Key_K);

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
    QMenu *menu = this->m_menuBar->addMenu("&View");
    menu->setObjectName("viewMenu");

    CHECKABLE_ACTION(m_toggleVisibleWhitespaceAction,
      "Visible &whitespace",
      viewToggleVisibleWhitespace,
      this->m_editorWidget->m_visibleWhitespace);

    MENU_ITEM    ("Set whitespace opacity...", viewSetWhitespaceOpacity);

    CHECKABLE_ACTION(m_toggleVisibleSoftMarginAction,
      "Visible soft &margin",
      viewToggleVisibleSoftMargin,
      this->m_editorWidget->m_visibleSoftMargin);

    MENU_ITEM    ("Set soft margin column...", viewSetSoftMarginColumn);

    CHECKABLE_ACTION(m_toggleHighlightTrailingWSAction,
      "Highlight &trailing whitespace",
      viewToggleHighlightTrailingWS,
      this->m_editorWidget->highlightTrailingWhitespace());

    MENU_ITEM    ("Set &Highlighting...", viewSetHighlighting);
  }

  #undef CHECKABLE_ACTION

  {
    QMenu *menu = this->m_menuBar->addMenu("&Window");
    menu->setObjectName("windowMenu");

    MENU_ITEM_KEY("Choose an &Open Document ...",
                  windowOpenFilesList, Qt::CTRL + Qt::Key_O);

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
  return m_editorWidget->getDocument();
}


void EditorWindow::fileNewFile()
{
  NamedTextDocument *b = m_globalState->createNewFile(
    m_editorWidget->getDocumentDirectory());
  setDocumentFile(b);
}


void EditorWindow::setDocumentFile(NamedTextDocument *file)
{
  // Before switching documents, put the old one at the top.  The idea
  // is that this document is the most recently used since it was just
  // shown to the user, even if it hasn't been explicitly switched to
  // recently.
  m_globalState->m_documentList.moveDocument(this->currentDocument(), 0);

  m_editorWidget->setDocumentFile(file);
  this->updateForChangedFile();
}

void EditorWindow::updateForChangedFile()
{
  m_editorWidget->recomputeLastVisible();
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
  if (suffixEquals(file->docName(), "diff")) {
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
      "h",
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
      "py",
      "sh",
    };
    if (stringAmong(ext, hashCommentExts, TABLESIZE(hashCommentExts))) {
      file->m_highlighter = new HashComment_Highlighter(file->getCore());
      return;
    }
  }

  if (suffixEquals(filename, "Makefile")) {
    file->m_highlighter = new Makefile_Highlighter(file->getCore());
    return;
  }
}


string EditorWindow::fileChooseDialog(string const &origDir,
  bool saveAs, FileChooseDialogKind dialogKind)
{
  string dir(origDir);
  TRACE("fileChooseDialog",
    "saveAs=" << saveAs << " kind=" << dialogKind << " dir: " << dir);
  if (dir == ".") {
    // If I pass "." to one of the static members of QFileDialog, it
    // automatically goes to the current directory.  But when using
    // QFileDialog directly, I have to pass the real directory name.
    dir = SMFileUtil().currentDirectory();
    TRACE("fileOpen", "current dir: " << dir);
  }

  if (dialogKind == FCDK_FILENAME_INPUT) {
    FilenameInputDialog dialog(
      &(m_globalState->m_filenameInputDialogHistory),
      &(m_globalState->m_vfsConnections),
      this);
    dialog.setSaveAs(saveAs);
    QString choice =
      dialog.runDialog(&(m_globalState->m_documentList), toQString(dir));
    return toString(choice);
  }

  QFileDialog dialog(this);

  // As far as I can tell, the only effect of this is to set the
  // dialog window title.
  dialog.setAcceptMode(saveAs?
    QFileDialog::AcceptSave : QFileDialog::AcceptOpen);

  dialog.setDirectory(toQString(dir));

  // I want to be able to "open" a non-existent file, just like I can
  // in emacs.  It turns out AnyFile is the default but I make it
  // explicit.
  dialog.setFileMode(QFileDialog::AnyFile);

  // The native dialog is causing some problems with desktop icons
  // refreshing and the dialog hanging.  The Qt version also causes
  // problems, but less severe?  I need to continue experimenting.
  //
  // The problems with hanging, etc., are indeed a little less severe,
  // but the non-native dialog has a problem: its "file name" text edit
  // line does not get selected after I press Enter after typing the
  // name of a directory, so I have to manually delete that before I can
  // continue navigating.  So, back to the native dialog for a while.
  //
  // 2018-07-14: I have built my own file chooser that is, IMO, much
  // better than either Windows or Qt's dialogs.  I'm just leaving this
  // here in case I want to continue experimenting.
  if (dialogKind == FCDK_QT) {
    dialog.setOptions(QFileDialog::DontUseNativeDialog);
  }

  if (!dialog.exec()) {
    TRACE("fileOpen", "canceled");
    return "";
  }

  QStringList filenames = dialog.selectedFiles();
  if (filenames.isEmpty()) {
    // I have been unable to trigger this behavior.
    TRACE("fileOpen", "dialog returned empty list of files");
    return "";
  }

  QString name = filenames.at(0);
  TRACE("fileOpen", "name: " << toString(name));
  if (name.isEmpty()) {
    // I have been unable to trigger this behavior.
    TRACE("fileOpen", "name is empty");
    return "";
  }

  if (filenames.size() > 1) {
    // I have been unable to trigger this behavior.
    TRACE("fileOpen", "dialog returned list of " << filenames.size() <<
                      " files, ignoring all but first");
  }

  return toString(name);
}


void EditorWindow::fileOpen()
{
  string dir = m_editorWidget->getDocumentDirectory();
  this->on_openFilenameInputDialogSignal(toQString(dir), 0);
}


void EditorWindow::fileOpenAtCursor()
{
  m_editorWidget->fileOpenAtCursor();
}


void EditorWindow::fileOpenNativeDialog()
{
  this->fileOpenFile(
    this->fileChooseDialog(m_editorWidget->getDocumentDirectory(),
                           false /*saveAs*/, FCDK_NATIVE));
}

void EditorWindow::fileOpenQtDialog()
{
  this->fileOpenFile(
    this->fileChooseDialog(m_editorWidget->getDocumentDirectory(),
                           false /*saveAs*/, FCDK_QT));
}

void EditorWindow::fileOpenFile(string const &name)
{
  if (name.empty()) {
    // Dialog was canceled.
    return;
  }

  TRACE("fileOpen", "fileOpenFile: " << name);

  // If this file is already open, switch to it.
  NamedTextDocument *file = m_globalState->m_documentList.findDocumentByName(name);
  if (file) {
    this->setDocumentFile(file);
    return;
  }

  // Load the file contents.
  std::unique_ptr<VFS_ReadFileReply> rfr(readFileSynchronously(name));
  if (!rfr) {
    // Either the request was canceled or an error has already been
    // reported.
    return;
  }

  file = new NamedTextDocument();
  file->setFilename(name);
  file->m_title = this->m_globalState->uniqueTitleFor(file->filename());

  if (rfr->m_success) {
    file->replaceFileAndStats(rfr->m_contents,
                              rfr->m_fileModificationTime,
                              rfr->m_readOnly);
  }
  else {
    if (rfr->m_failureReasonCode == xSysError::R_FILE_NOT_FOUND) {
      // Just have the file open with its name set but no content.
    }
    else {
      this->complain(stringb(
        rfr->m_failureReasonString <<
        " (code " << rfr->m_failureReasonCode << ")"));
      delete file;
      return;
    }
  }

  this->useDefaultHighlighter(file);

  // is there an untitled, empty file hanging around?
  RCSerf<NamedTextDocument> untitled =
    this->m_globalState->m_documentList.findUntitledUnmodifiedDocument();

  // now that we've opened the file, set the editor widget to edit it
  m_globalState->trackNewDocumentFile(file);
  setDocumentFile(file);

  // remove the untitled file now, if it exists
  if (untitled) {
    m_globalState->deleteDocumentFile(untitled.release());
  }
}


template <class REPLY_TYPE>
std::unique_ptr<REPLY_TYPE> EditorWindow::vfsQuerySynchronously(
  std::unique_ptr<VFS_Message> request)
{
  // Initially empty pointer, used for error returns.
  std::unique_ptr<REPLY_TYPE> typedReply;

  // Issue the request.
  VFS_QuerySync querySync(&(m_globalState->m_vfsConnections), this);
  std::unique_ptr<VFS_Message> genericReply;
  string connLostMessage;
  if (!querySync.issueRequestSynchronously(
         std::move(request), genericReply, connLostMessage)) {
    // Attempt to load the file was canceled.
    return typedReply;
  }

  if (!connLostMessage.empty()) {
    this->complain(stringb(
      "VFS connection lost: " << connLostMessage));
    return typedReply;
  }

  if (REPLY_TYPE *r = dynamic_cast<REPLY_TYPE*>(genericReply.get())) {
    // Move the pointer from 'genericReply' to 'typedReply'.
    genericReply.release();
    typedReply.reset(r);
    return typedReply;
  }

  else {
    this->complain(stringb(
      "Server responded with incorrect message type: " <<
      toString(genericReply->messageType())));
    return typedReply;
  }
}


std::unique_ptr<VFS_ReadFileReply> EditorWindow::readFileSynchronously(
  string const &fname)
{
  std::unique_ptr<VFS_ReadFileRequest> req(new VFS_ReadFileRequest);
  req->m_path = fname;
  return vfsQuerySynchronously<VFS_ReadFileReply>(std::move(req));
}


void EditorWindow::fileSave()
{
  NamedTextDocument *b = this->currentDocument();
  if (!b->hasFilename()) {
    TRACE("untitled", "file has no title; invoking Save As ...");
    fileSaveAs();
    return;
  }

  if (b->hasStaleModificationTime()) {
    QMessageBox box(this);
    box.setWindowTitle("File Changed");
    box.setText(toQString(stringb(
      "The file \"" << b->docName() << "\" has changed on disk.  "
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
}

void EditorWindow::writeTheFile()
{
  RCSerf<NamedTextDocument> file = this->currentDocument();
  try {
    file->writeFile();
    editorViewChanged();
  }
  catch (xBase &x) {
    // There is not a severity between "warning" and "critical",
    // and "critical" is a bit obnoxious.
    QMessageBox::warning(this, "Write Error",
      qstringb("Failed to save file \"" << file->docName() << "\": " << x.why()));
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


void EditorWindow::fileSaveAs()
{
  NamedTextDocument *fileDoc = currentDocument();

  // Directory to start in.  This may change if we prompt the user
  // more than once.
  string dir = fileDoc->directory();

  while (true) {
    string chosenFilename =
      this->fileChooseDialog(dir, true /*saveAs*/, FCDK_FILENAME_INPUT);
    if (chosenFilename.isempty()) {
      return;
    }
    if (!stillCurrentDocument(fileDoc)) {
      return;
    }

    if (fileDoc->hasFilename() &&
        fileDoc->filename() == chosenFilename) {
      // User chose to save using the same file name.
      this->fileSave();
      return;
    }

    if (this->m_globalState->hasFileWithName(chosenFilename)) {
      this->complain(stringb(
        "There is already an open file with name \"" <<
        chosenFilename <<
        "\".  Choose a different name to save as."));

      // Discard name portion, but keep directory.
      dir = SMFileUtil().splitPathDir(chosenFilename);

      // Now prompt again.
    }
    else {
      fileDoc->setFilename(chosenFilename);
      fileDoc->m_title = this->m_globalState->uniqueTitleFor(chosenFilename);
      writeTheFile();
      this->useDefaultHighlighter(fileDoc);

      // Notify observers of the file name and highlighter change.  This
      // includes myself.
      this->m_globalState->m_documentList.notifyAttributeChanged(fileDoc);

      return;
    }
  }

  // Never reached.
}


void EditorWindow::fileClose()
{
  NamedTextDocument *b = currentDocument();
  if (b->unsavedChanges()) {
    stringBuilder msg;
    msg << "The document " << quoted(b->docName()) << " has unsaved changes.  "
        << "Discard these changes and close it anyway?";
    if (!this->okToDiscardChanges(msg)) {
      return;
    }
    if (!stillCurrentDocument(b)) {
      return;
    }
  }

  m_globalState->deleteDocumentFile(b);
}


void EditorWindow::fileToggleReadOnly() NOEXCEPT
{
  GENERIC_CATCH_BEGIN
  m_editorWidget->setReadOnly(!m_editorWidget->isReadOnly());
  GENERIC_CATCH_END
}


void EditorWindow::fileReload()
{
  if (reloadFile(this->currentDocument())) {
    this->editorViewChanged();
  }
}


bool EditorWindow::reloadFile(NamedTextDocument *b_)
{
  // TODO: This function's signature is inherently dangerous because I
  // have no way of re-confirming 'b' after the dialog box closes since
  // I don't know where it came from.  For now I will just arrange to
  // abort before memory corruption can happen, but I should change the
  // callers to use a more reliable pattern.
  RCSerf<NamedTextDocument> b(b_);

  if (b->unsavedChanges()) {
    if (!this->okToDiscardChanges(stringb(
          "The file \"" << b->docName() << "\" has unsaved changes.  "
          "Discard those changes and reload this file anyway?"))) {
      return false;
    }
  }

  try {
    b->readFile();
    return true;
  }
  catch (xBase &x) {
    this->complain(stringb(
      "Can't read file \"" << b->docName() << "\": " << x.why() <<
      "\n\nThe file will remain open in the editor with its "
      "old contents."));
    return false;
  }
}


void EditorWindow::fileReloadAll()
{
  stringBuilder msg;
  int ct = getUnsavedChanges(msg);

  if (ct > 0) {
    msg << "\nYou must deal with those individually.";
    this->complain(msg);
    return;
  }

  for (int i=0; i < this->m_globalState->m_documentList.numDocuments(); i++) {
    NamedTextDocument *file = this->m_globalState->m_documentList.getDocumentAt(i);
    if (!this->reloadFile(file)) {
      // Stop after first error.
      break;
    }
  }

  this->editorViewChanged();
}


void EditorWindow::fileLaunchCommand()
{
  static LaunchCommandDialog *dialog =
    new LaunchCommandDialog();

  string dir = m_editorWidget->getDocumentDirectory();
  if (!dialog->runPrompt_nonEmpty(
        qstringb("&Command to launch in " << dir << ":"), this)) {
    return;
  }

  NamedTextDocument *doc = m_globalState->launchCommand(
    toQString(dir),
    dialog->prefixStderrLines(),
    dialog->m_text);
  this->setDocumentFile(doc);

  // Choose a highlighter based on the command line.
  this->useDefaultHighlighter(doc);
}


void EditorWindow::fileRunMake()
{
  string dir = m_editorWidget->getDocumentDirectory();

  // My intent is the user creates a script with this name on their
  // $PATH.  Then the script can do whatever is desired here.
  NamedTextDocument *fileDoc = m_globalState->launchCommand(
    toQString(dir), false /*prefixStderrLines*/, "run-make-from-editor");
  this->setDocumentFile(fileDoc);
}


void EditorWindow::fileKillProcess()
{
  NamedTextDocument *doc = this->currentDocument();
  DocumentProcessStatus dps = doc->documentProcessStatus();

  switch (dps) {
    default:
      DEV_WARNING("bad dps");
      // fallthrough

    case DPS_NONE:
      messageBox(this, "Not a Process Document", qstringb(
        "The document \"" << doc->docName() << "\" was not produced by "
        "running a process, so there is nothing to kill."));
      break;

    case DPS_RUNNING:
      if (questionBoxYesCancel(this, "Kill Process?", qstringb(
            "Kill the process \"" << doc->docName() << "\"?"))) {
        if (this->stillCurrentDocument(doc)) {
          string problem = m_globalState->killCommand(doc);
          if (!problem.empty()) {
            messageBox(this, "Problem Killing Process",
              toQString(problem));
          }
        }
      }
      break;

    case DPS_FINISHED:
      messageBox(this, "Process Finished", qstringb(
        "The process \"" << doc->docName() << "\" has already terminated."));
      break;
  }
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
  for (int i=0; i < this->m_globalState->m_documentList.numDocuments(); i++) {
    NamedTextDocument *file = this->m_globalState->m_documentList.getDocumentAt(i);
    if (file->unsavedChanges()) {
      ct++;
      msg << " * " << file->docName() << '\n';
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


bool EditorWindow::eventFilter(QObject *watched, QEvent *event) NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  // Within the editor window, I do not use Tab for input focus changes,
  // but the existence of other focusable controls (when the Search and
  // Replace panel is open) causes Tab to be treated as such unless I
  // use an event filter.
  if (watched == this->m_editorWidget && event->type() == QEvent::KeyPress) {
    QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);
    if (keyEvent->key() == Qt::Key_Tab ||
        keyEvent->key() == Qt::Key_Backtab) {
      TRACE("input", "EditorWindow passing Tab press on to EditorWidget");
      this->m_editorWidget->rescuedKeyPressEvent(keyEvent);
      return true;       // no further processing
    }
  }

  return false;

  GENERIC_CATCH_END_RET(false)
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
  this->m_editorWidget->update();

  GENERIC_CATCH_END
}

void EditorWindow::namedTextDocumentListOrderChanged(
  NamedTextDocumentList *) NOEXCEPT
{}


void EditorWindow::fileExit()
{
  if (this->canQuitApplication()) {
    EditorGlobalState::quit();
  }
}


void EditorWindow::editUndo() NOEXCEPT
{
  m_editorWidget->editUndo();
}

void EditorWindow::editRedo() NOEXCEPT
{
  m_editorWidget->editRedo();
}

void EditorWindow::editCut() NOEXCEPT
{
  m_editorWidget->editCut();
}

void EditorWindow::editCopy() NOEXCEPT
{
  m_editorWidget->editCopy();
}

void EditorWindow::editPaste() NOEXCEPT
{
  m_editorWidget->editPaste();
}

void EditorWindow::editDelete() NOEXCEPT
{
  m_editorWidget->editDelete();
}

void EditorWindow::editKillLine() NOEXCEPT
{
  GENERIC_CATCH_BEGIN
  m_editorWidget->editKillLine();
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
  m_editorWidget->nextSearchHit(false /*reverse*/);
  GENERIC_CATCH_END
}


void EditorWindow::editPreviousSearchHit() NOEXCEPT
{
  GENERIC_CATCH_BEGIN
  m_editorWidget->nextSearchHit(true /*reverse*/);
  GENERIC_CATCH_END
}


void EditorWindow::editGotoLine()
{
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
    if (!s.isempty()) {
      int n = atoi(s);
      if (n > 0) {
        m_editorWidget->cursorTo(TextLCoord(n-1, 0));
        m_editorWidget->scrollToCursor(-1 /*center*/);
      }
      else {
        this->complain(stringb("Invalid line number: " << s));
      }
    }
  }
}


void EditorWindow::editGrepSource() NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  string searchText = m_editorWidget->getSelectedOrIdentifier();
  if (searchText.empty()) {
    messageBox(this, "No Search Text Provided",
      "To use this feature, either select some text to search for, or "
      "put the text cursor on an identifier and that will act as the "
      "search text.  You also need a program called \"grepsrc\" in "
      "the PATH, as that is what the search string is passed to.");
  }
  else {
    string dir = m_editorWidget->getDocumentDirectory();
    NamedTextDocument *fileDoc =
      m_globalState->launchCommand(toQString(dir),
        true /*prefixStderrLines*/,
        qstringb("grepsrc " << shellDoubleQuote(searchText)));
    this->setDocumentFile(fileDoc);
  }

  GENERIC_CATCH_END
}


void EditorWindow::editRigidIndent() NOEXCEPT
{
  GENERIC_CATCH_BEGIN
  m_editorWidget->editRigidIndent();
  GENERIC_CATCH_END
}


void EditorWindow::editRigidUnindent() NOEXCEPT
{
  GENERIC_CATCH_BEGIN
  m_editorWidget->editRigidUnindent();
  GENERIC_CATCH_END
}


void EditorWindow::editJustifyParagraph() NOEXCEPT
{
  GENERIC_CATCH_BEGIN
  m_editorWidget->editJustifyParagraph();
  GENERIC_CATCH_END
}


void EditorWindow::editApplyCommand()
{
  GENERIC_CATCH_BEGIN

  // Object to manage the child process.
  CommandRunner runner;

  // The command the user wants to run.
  string commandString;

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
    static TextInputDialog *dialog =
      new TextInputDialog("Apply Command");

    string dir = m_editorWidget->getDocumentDirectory();
    if (!dialog->runPrompt_nonEmpty(
          qstringb("Command to run in " << dir << ":"),
          this)) {
      return;
    }
    commandString = toString(dialog->m_text);

    tde = m_editorWidget->getDocumentEditor();
    string input = tde->getSelectedText();

    // For now, I will assume I have a POSIX shell.
    runner.setProgram("sh");
    runner.setArguments(QStringList() << "-c" << dialog->m_text);

    runner.setWorkingDirectory(toQString(dir));

    // TODO: This mishandles NUL bytes.
    runner.setInputData(QByteArray(input.c_str()));

    // It would be bad to hold an undo group open while we pump
    // the event queue.
    xassert(!tde->inUndoGroup());

    // Both the window and the widget have to have their cursor
    // changed, the latter (I think) because it already has a
    // non-standard cursor set.
    CursorSetRestore csr(this, Qt::WaitCursor);
    CursorSetRestore csr2(m_editorWidget, Qt::WaitCursor);

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
      "The command \"" << commandString <<
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
  if (tde != m_editorWidget->getDocumentEditor()) {
    QMessageBox::warning(this, "Editor Changed", qstringb(
      "While running command \"" << commandString <<
      "\", the active editor changed.  I will discard "
      "the output of that command."));
    return;
  }

  // Replace the selected text with the command's output.
  m_editorWidget->insertText(runner.getOutputData().constData(),
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
  m_editorWidget->editInsertDateTime();
  GENERIC_CATCH_END
}


#define CHECKABLE_MENU_TOGGLE(actionField, sourceBool)  \
  (sourceBool) = !(sourceBool);                         \
  this->actionField->setChecked(sourceBool);            \
  this->m_editorWidget->update() /* user ; */


void EditorWindow::viewToggleVisibleWhitespace()
{
  CHECKABLE_MENU_TOGGLE(m_toggleVisibleWhitespaceAction,
    this->m_editorWidget->m_visibleWhitespace);
}


void EditorWindow::viewSetWhitespaceOpacity()
{
  bool ok;
  int n = QInputDialog::getInt(this,
    "Visible Whitespace",
    "Opacity in [1,255]:",
    this->m_editorWidget->m_whitespaceOpacity,
    1 /*min*/, 255 /*max*/, 1 /*step*/, &ok);
  if (ok) {
    this->m_editorWidget->m_whitespaceOpacity = n;
    this->m_editorWidget->update();
  }
}


void EditorWindow::viewToggleVisibleSoftMargin()
{
  CHECKABLE_MENU_TOGGLE(m_toggleVisibleSoftMarginAction,
    this->m_editorWidget->m_visibleSoftMargin);
}


void EditorWindow::viewSetSoftMarginColumn()
{
  bool ok;
  int n = QInputDialog::getInt(this,
    "Soft Margin Column",
    "Column number (positive):",
    this->m_editorWidget->m_softMarginColumn+1,
    1 /*min*/, INT_MAX /*max*/, 1 /*step*/, &ok);
  if (ok) {
    this->m_editorWidget->m_softMarginColumn = n-1;
    this->m_editorWidget->update();
  }
}


void EditorWindow::viewToggleHighlightTrailingWS()
{
  this->m_editorWidget->toggleHighlightTrailingWhitespace();

  // Includes firing 'editorViewChanged'.
  this->m_editorWidget->redraw();
}


#undef CHECKABLE_MENU_TOGGLE


void EditorWindow::viewSetHighlighting()
{
  NamedTextDocument *doc = this->currentDocument();

  QInputDialog dialog(this);
  dialog.setWindowTitle("Set Highlighting");
  dialog.setLabelText("Highlighting to use for this file:");
  dialog.setComboBoxItems(QStringList() <<
    "None" <<
    "C/C++" <<
    "Diff" <<
    "HashComment" <<
    "Makefile");

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

  // Notify everyone of the change.
  this->m_globalState->m_documentList.notifyAttributeChanged(doc);
}


void EditorWindow::windowOpenFilesList()
{
  // Put the current document on top before opening the dialog so one
  // can always hit Ctrl+O, Enter and the displayed document won't
  // change.
  m_globalState->m_documentList.moveDocument(this->currentDocument(), 0);

  NamedTextDocument *doc = m_globalState->runOpenFilesDialog(this);
  if (doc) {
    this->setDocumentFile(doc);
  }
}


void EditorWindow::windowOccupyLeft()
{
  if (QApplication::desktop()->width() == 1024) {  // 1024x768
    setGeometry(83, 49, 465, 660);
  }
  else {    // 1280x1024
    setGeometry(83, 59, 565, 867);
  }
}


void EditorWindow::windowOccupyRight()
{
  if (QApplication::desktop()->width() == 1024) {  // 1024x768
    setGeometry(390, 49, 630, 660);
  }
  else {    // 1280x1024
    setGeometry(493, 59, 783, 867);
  }
}


void EditorWindow::helpKeybindings()
{
  KeysDialog d(doc_keybindings, this);
  d.exec();
}


void EditorWindow::helpAbout()
{
  QMessageBox::about(this, "About Scott's Editor",
    qstringb(
      "This is a text editor that has a user interface I like.\n"
      "\n"
      "Version: " << editor_git_version));
}


void EditorWindow::helpAboutQt()
{
  QMessageBox::aboutQt(this, "An editor");
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

  QImage image(m_editorWidget->getScreenshot());

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


void EditorWindow::editorViewChanged()
{
  RCSerf<TextDocumentEditor> tde = m_editorWidget->getDocumentEditor();

  // Set the scrollbars.  In both dimensions, the range includes the
  // current value so we can scroll arbitrarily far beyond the nominal
  // size of the file contents.  Also, it is essential to set the range
  // *before* setting the value, since otherwise the scrollbar's value
  // will be clamped to the old range.
  if (m_horizScroll) {
    m_horizScroll->setRange(0, max(tde->maxLineLengthColumns(),
                                   m_editorWidget->firstVisibleCol()));
    m_horizScroll->setValue(m_editorWidget->firstVisibleCol());
    m_horizScroll->setSingleStep(1);
    m_horizScroll->setPageStep(m_editorWidget->visCols());
  }

  if (m_vertScroll) {
    m_vertScroll->setRange(0, max(tde->numLines(),
                                  m_editorWidget->firstVisibleLine()));
    m_vertScroll->setValue(m_editorWidget->firstVisibleLine());
    m_vertScroll->setSingleStep(1);
    m_vertScroll->setPageStep(m_editorWidget->visLines());
  }

  // I want the user to interact with line/col with a 1:1 origin,
  // even though the TextDocument interface uses 0:0.
  m_statusArea->m_cursor->setText(qstringb(
    (m_editorWidget->cursorLine()+1) << ':' <<
    (m_editorWidget->cursorCol()+1)));

  // Status text: full document name plus status indicators.
  NamedTextDocument *file = currentDocument();
  m_statusArea->m_filename->setText(toQString(
    file->nameWithStatusIndicators()));

  // Window title.
  stringBuilder sb;
  sb << file->m_title;
  if (tde->unsavedChanges()) {
    sb << " *";
  }
  sb << " - " << appName;
  this->setWindowTitle(toQString(sb));

  // Trailing whitespace menu checkbox.
  this->m_toggleHighlightTrailingWSAction->setChecked(
    this->m_editorWidget->highlightTrailingWhitespace());

  // Read-only menu checkbox.
  m_toggleReadOnlyAction->setChecked(m_editorWidget->isReadOnly());
}


void EditorWindow::on_closeSARPanel()
{
  if (m_sarPanel->isVisible()) {
    m_sarPanel->hide();
    this->m_editorWidget->setFocus();
  }
}


void EditorWindow::on_openFilenameInputDialogSignal(
  QString const &filename, int line)
{
  // Check for fast-open conditions.
  {
    SMFileUtil sfu;
    string fn(toString(filename));
    if (sfu.absoluteFileExists(fn) &&
        currentDocument()->docName() != fn) {
      // The file exists, and it is not the current document.  Just
      // go straight to opening it without prompting.
      this->fileOpenFile(fn);
      if (line != 0) {
        // Also go to line number, if provided.
        m_editorWidget->cursorTo(TextLCoord(line-1, 0));
        m_editorWidget->clearMark();
        m_editorWidget->scrollToCursor(-1 /*gap*/);
      }
      return;
    }
  }

  // Prompt to confirm.
  FilenameInputDialog dialog(
    &(m_globalState->m_filenameInputDialogHistory),
    &(m_globalState->m_vfsConnections),
    this);
  QString confirmedFilename =
    dialog.runDialog(&(m_globalState->m_documentList), filename);

  if (!confirmedFilename.isEmpty()) {
    this->fileOpenFile(toString(confirmedFilename));
  }
}


void EditorWindow::complain(char const *msg)
{
  QMessageBox::information(this, appName, msg);
}


void EditorWindow::windowNewWindow()
{
  EditorWindow *ed = this->m_globalState->createNewWindow(this->currentDocument());
  ed->show();
}


void EditorWindow::windowCloseWindow()
{
  // This sends 'closeEvent' and actually closes the window only if
  // the event is accepted.
  this->close();
}


void EditorWindow::closeEvent(QCloseEvent *event)
{
  if (this->m_globalState->m_windows.count() == 1) {
    if (!this->canQuitApplication()) {
      event->ignore();    // Prevent app from closing.
      return;
    }
  }

  event->accept();
}


// EOF
