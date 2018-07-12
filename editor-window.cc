// editor-window.cc
// code for editor-window.h

#include "editor-window.h"             // this module

// this dir
#include "c_hilite.h"                  // C_Highlighter
#include "command-runner.h"            // CommandRunner
#include "editor-widget.h"             // EditorWidget
#include "filename-input.h"            // FilenameInputDialog
#include "git-version.h"               // editor_git_version
#include "keybindings.doc.gen.h"       // doc_keybindings
#include "keys-dialog.h"               // KeysDialog
#include "main.h"                      // GlobalState
#include "pixmaps.h"                   // pixmaps
#include "qhboxframe.h"                // QHBoxFrame
#include "sar-panel.h"                 // SearchAndReplacePanel
#include "status.h"                    // StatusDisplay
#include "td-editor.h"                 // TextDocumentEditor
#include "textinput.h"                 // TextInputDialog

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
#include "test.h"                      // PVAL
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


EditorWindow::EditorWindow(GlobalState *theState, FileTextDocument *initFile,
                           QWidget *parent)
  : QWidget(parent),
    m_globalState(theState),
    m_menuBar(NULL),
    m_editorWidget(NULL),
    m_sarPanel(NULL),
    m_vertScroll(NULL),
    m_horizScroll(NULL),
    m_statusArea(NULL),
    m_windowMenu(NULL),
    m_toggleVisibleWhitespaceAction(NULL),
    m_toggleVisibleSoftMarginAction(NULL),
    m_toggleHighlightTrailingWSAction(NULL),
    m_fileChoiceActions()
{
  xassert(theState);
  xassert(initFile);

  // will build a layout tree to manage sizes of child widgets
  QVBoxLayout *mainArea = new QVBoxLayout();
  mainArea->setObjectName("mainArea");
  mainArea->setSpacing(0);
  mainArea->setContentsMargins(0, 0, 0, 0);

  this->m_menuBar = new QMenuBar();
  this->m_menuBar->setObjectName("main menu bar");
  mainArea->addWidget(this->m_menuBar);

  QGridLayout *editArea = new QGridLayout();
  editArea->setObjectName("editArea");
  mainArea->addLayout(editArea, 1 /*stretch*/);

  m_sarPanel = new SearchAndReplacePanel();
  mainArea->addWidget(m_sarPanel);
  m_sarPanel->setObjectName("m_sarPanel");
  m_sarPanel->hide();      // Initially hidden.

  this->m_statusArea = new StatusDisplay();
  this->m_statusArea->setObjectName("m_statusArea");
  mainArea->addWidget(this->m_statusArea);

  // Put a black one-pixel frame around the editor widget.
  QHBoxFrame *editorFrame = new QHBoxFrame();
  editorFrame->setFrameStyle(QFrame::Box);
  editArea->addWidget(editorFrame, 0 /*row*/, 0 /*col*/);

  this->m_editorWidget = new EditorWidget(initFile,
    &(theState->m_documentList), this->m_statusArea);
  this->m_editorWidget->setObjectName("editor widget");
  editorFrame->addWidget(this->m_editorWidget);
  this->m_editorWidget->setFocus();
  connect(this->m_editorWidget, SIGNAL(viewChanged()),
          this, SLOT(editorViewChanged()));
  connect(this->m_editorWidget, &EditorWidget::closeSARPanel,
          this, &EditorWindow::on_closeSARPanel);

  // See EditorWidget::openFileAtCursor for why this is a
  // QueuedConnection.
  connect(this->m_editorWidget, &EditorWidget::openFilenameInputDialogSignal,
          this, &EditorWindow::on_openFilenameInputDialogSignal,
          Qt::QueuedConnection);

  // See explanation in GlobalState::focusChangedHandler().
  this->setFocusProxy(this->m_editorWidget);

  // Needed to ensure Tab gets passed down to the editor widget.
  this->m_editorWidget->installEventFilter(this);

  // Connect these, which had to wait until both were constructed.
  m_sarPanel->setEditorWidget(this->m_editorWidget);

  this->m_vertScroll = new QScrollBar(Qt::Vertical);
  this->m_vertScroll->setObjectName("m_vertScroll");
  editArea->addWidget(this->m_vertScroll, 0 /*row*/, 1 /*col*/);
  connect(this->m_vertScroll, SIGNAL( valueChanged(int) ),
          this->m_editorWidget, SLOT( scrollToLine(int) ));

  // disabling horiz scroll for now..
  //m_horizScroll = new QScrollBar(QScrollBar::Horizontal, editArea, "horizontal scrollbar");
  //connect(m_horizScroll, SIGNAL( valueChanged(int) ), editor, SLOT( scrollToCol(int) ));

  this->buildMenu();
  this->rebuildWindowMenu();

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

  this->m_globalState->m_documentList.removeObserver(this);

  // This object might have already been removed, for example because
  // the GlobalState destructor is running, and is in the process of
  // removing elements from the list and destroying them.
  this->m_globalState->m_windows.removeIfPresent(this);

  // The QObject destructor will destroy both 'm_sarPanel' and
  // 'm_editorWidget', but the documentation of ~QObject does not
  // specify an order.  Disconnect them here so that either order works.
  m_sarPanel->setEditorWidget(NULL);
}


void EditorWindow::buildMenu()
{
  {
    QMenu *file = this->m_menuBar->addMenu("&File");
    file->addAction("&New", this, SLOT(fileNewFile()));
    file->addAction("&Open ...", this, SLOT(fileOpen()), Qt::Key_F3);
    file->addAction("&Save", this, SLOT(fileSave()), Qt::Key_F2);
    file->addAction("Save &as ...", this, SLOT(fileSaveAs()));
    file->addAction("&Close", this, SLOT(fileClose()), Qt::Key_F4);
    file->addSeparator();
    file->addAction("&Reload", this, SLOT(fileReload()));
    file->addAction("Reload a&ll", this, SLOT(fileReloadAll()));
    file->addSeparator();
    file->addAction("E&xit", this, SLOT(fileExit()));
  }

  {
    QMenu *edit = this->m_menuBar->addMenu("&Edit");
    edit->addAction("&Undo", m_editorWidget, SLOT(editUndo()), Qt::ALT + Qt::Key_Backspace);
    edit->addAction("&Redo", m_editorWidget, SLOT(editRedo()), Qt::ALT + Qt::SHIFT + Qt::Key_Backspace);
    edit->addSeparator();
    edit->addAction("Cu&t", m_editorWidget, SLOT(editCut()), Qt::CTRL + Qt::Key_X);
    edit->addAction("&Copy", m_editorWidget, SLOT(editCopy()), Qt::CTRL + Qt::Key_C);
    edit->addAction("&Paste", m_editorWidget, SLOT(editPaste()), Qt::CTRL + Qt::Key_V);
    edit->addAction("&Delete", m_editorWidget, SLOT(editDelete()));
    edit->addSeparator();
    edit->addAction("&Search ...", this, SLOT(editISearch()), Qt::CTRL + Qt::Key_S);
    edit->addAction("&Goto Line ...", this, SLOT(editGotoLine()), Qt::ALT + Qt::Key_G);
    edit->addAction("&Apply Command ...", this,
                    SLOT(editApplyCommand()), Qt::ALT + Qt::Key_A);
  }

  #define CHECKABLE_ACTION(field, title, slot, initChecked)  \
    this->field = menu->addAction(title, this, slot);        \
    this->field->setCheckable(true);                         \
    this->field->setChecked(initChecked) /* user ; */

  {
    QMenu *menu = this->m_menuBar->addMenu("&View");

    CHECKABLE_ACTION(m_toggleVisibleWhitespaceAction,
      "Visible &whitespace",
      SLOT(viewToggleVisibleWhitespace()),
      this->m_editorWidget->m_visibleWhitespace);

    menu->addAction("Set whitespace opacity...", this, SLOT(viewSetWhitespaceOpacity()));

    CHECKABLE_ACTION(m_toggleVisibleSoftMarginAction,
      "Visible soft &margin",
      SLOT(viewToggleVisibleSoftMargin()),
      this->m_editorWidget->m_visibleSoftMargin);

    menu->addAction("Set soft margin column...", this, SLOT(viewSetSoftMarginColumn()));

    CHECKABLE_ACTION(m_toggleHighlightTrailingWSAction,
      "Highlight &trailing whitespace",
      SLOT(viewToggleHighlightTrailingWS()),
      this->m_editorWidget->m_highlightTrailingWhitespace);

    menu->addAction("Set &Highlighting...", this,
      &EditorWindow::viewSetHighlighting);
  }

  #undef CHECKABLE_ACTION

  {
    QMenu *window = this->m_menuBar->addMenu("&Window");

    window->addAction("Pick an &Open File ...", this,
      SLOT(windowOpenFilesList()), Qt::CTRL + Qt::Key_O);

    window->addSeparator();

    window->addAction("&New Window", this, SLOT(windowNewWindow()));
    window->addAction("&Close Window", this,
      SLOT(windowCloseWindow()), Qt::CTRL + Qt::Key_F4);
    window->addAction("Move/size to Left Screen Half", this,
      SLOT(windowOccupyLeft()), Qt::CTRL + Qt::ALT + Qt::Key_Left);
    window->addAction("Move/size to Right Screen Half", this,
      SLOT(windowOccupyRight()), Qt::CTRL + Qt::ALT + Qt::Key_Right);

    window->addSeparator();

    this->m_windowMenu = window;
    connect(this->m_windowMenu, SIGNAL(triggered(QAction*)),
            this, SLOT(windowFileChoiceActivated(QAction*)));
  }

  {
    QMenu *help = this->m_menuBar->addMenu("&Help");
    help->addAction("&Keybindings...", this, SLOT(helpKeybindings()));
    help->addAction("&About Scott's Editor...", this, SLOT(helpAbout()));
    help->addAction("About &Qt ...", this, SLOT(helpAboutQt()));
  }
}


// not inline because main.h doesn't see editor.h
FileTextDocument *EditorWindow::currentDocument()
{
  return m_editorWidget->getDocumentFile();
}


void EditorWindow::fileNewFile()
{
  FileTextDocument *b = m_globalState->createNewFile();
  setDocumentFile(b);
}


void EditorWindow::setDocumentFile(FileTextDocument *file)
{
  m_editorWidget->setDocumentFile(file);
  this->updateForChangedFile();
}

void EditorWindow::updateForChangedFile()
{
  m_editorWidget->recomputeLastVisible();
  editorViewChanged();
}


void EditorWindow::useDefaultHighlighter(FileTextDocument *file)
{
  if (file->highlighter) {
    delete file->highlighter;
    file->highlighter = NULL;
  }

  // get file extension
  char const *dot = strrchr(file->filename.c_str(), '.');
  if (dot) {
    string ext = string(dot+1);
    if (ext.equals("h") ||
        ext.equals("cc") ||
        ext.equals("cpp")) {
      // make and attach a C++ highlighter for C/C++ files
      file->highlighter = new C_Highlighter(file->getCore());
    }
  }
}


string EditorWindow::fileChooseDialog(string const &initialName, bool saveAs)
{
  // Start in the directory containing the file currently shown.
  string dir = dirname(initialName);
  TRACE("fileOpen", "saveAs=" << saveAs << " dir: " << dir);
  if (dir == ".") {
    // If I pass "." to one of the static members of QFileDialog, it
    // automatically goes to the current directory.  But when using
    // QFileDialog directly, I have to pass the real directory name.
    dir = SMFileUtil().currentDirectory();
    TRACE("fileOpen", "current dir: " << dir);
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
  //dialog.setOptions(QFileDialog::DontUseNativeDialog);

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
  string name =
    this->fileChooseDialog(m_editorWidget->getDocumentFile()->filename,
                           false /*saveAs*/);
  if (name.isempty()) {
    return;
  }

  this->fileOpenFile(name);
}

void EditorWindow::fileOpenFile(string const &name)
{
  TRACE("fileOpen", "fileOpenFile: " << name);

  // If this file is already open, switch to it.
  FileTextDocument *file = m_globalState->m_documentList.findFileByName(name);
  if (file) {
    this->setDocumentFile(file);
    return;
  }

  file = new FileTextDocument();
  file->filename = name;
  file->isUntitled = false;
  file->title = this->m_globalState->uniqueTitleFor(file->filename);

  if (fileOrDirectoryExists(file->filename.c_str())) {
    try {
      file->readFile();
    }
    catch (xBase &x) {
      this->complain(stringb(
        "Can't read file \"" << name << "\": " << x.why()));
      delete file;
      return;
    }
  }
  else {
    // Just have the file open with its name set but no content.
  }

  this->useDefaultHighlighter(file);

  // is there an untitled, empty file hanging around?
  RCSerf<FileTextDocument> untitled =
    this->m_globalState->m_documentList.findUntitledUnmodifiedFile();
  if (untitled) {
    // I'm going to remove it, but can't yet b/c I
    // need to wait until the new file is added;
    // but right now I can remove its hotkey so that
    // the new file can use it instead.
    untitled->clearHotkey();
  }

  // now that we've opened the file, set the editor widget to edit it
  m_globalState->trackNewDocumentFile(file);
  setDocumentFile(file);

  // remove the untitled file now, if it exists
  if (untitled) {
    m_globalState->deleteDocumentFile(untitled.release());
  }
}


void EditorWindow::fileSave()
{
  FileTextDocument *b = this->currentDocument();
  if (b->isUntitled) {
    TRACE("untitled", "file has no title; invoking Save As ...");
    fileSaveAs();
    return;
  }

  if (b->hasStaleModificationTime()) {
    QMessageBox box(this);
    box.setWindowTitle("File Changed");
    box.setText(toQString(stringb(
      "The file \"" << b->filename << "\" has changed on disk.  "
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
  RCSerf<FileTextDocument> file = this->currentDocument();
  try {
    file->writeFile();
    editorViewChanged();
  }
  catch (xBase &x) {
    // There is not a severity between "warning" and "critical",
    // and "critical" is a bit obnoxious.
    QMessageBox::warning(this, "Write Error",
      qstringb("Failed to save file \"" << file->filename << "\": " << x.why()));
  }
}


bool EditorWindow::stillCurrentDocument(FileTextDocument *doc)
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
  FileTextDocument *fileDoc = currentDocument();
  string chosenFilename = fileDoc->filename;
  while (true) {
    chosenFilename =
      this->fileChooseDialog(chosenFilename, true /*saveAs*/);
    if (chosenFilename.isempty()) {
      return;
    }
    if (!stillCurrentDocument(fileDoc)) {
      return;
    }

    if (!fileDoc->isUntitled &&
        fileDoc->filename == chosenFilename) {
      this->fileSave();
      return;
    }

    if (this->m_globalState->hasFileWithName(chosenFilename)) {
      this->complain(stringb(
        "There is already an open file with name \"" <<
        chosenFilename <<
        "\".  Choose a different name to save as."));
    }
    else {
      break;
    }
  }

  fileDoc->filename = chosenFilename;
  fileDoc->isUntitled = false;
  fileDoc->title = this->m_globalState->uniqueTitleFor(chosenFilename);
  writeTheFile();
  this->useDefaultHighlighter(fileDoc);

  // Notify observers of the file name and highlighter change.  This
  // includes myself.
  this->m_globalState->m_documentList.notifyAttributeChanged(fileDoc);
}


void EditorWindow::fileClose()
{
  FileTextDocument *b = currentDocument();
  if (b->unsavedChanges()) {
    stringBuilder msg;
    msg << "The file \"" << b->filename << "\" has unsaved changes.  "
        << "Discard these changes and close this file anyway?";
    if (!this->okToDiscardChanges(msg)) {
      return;
    }
    if (!stillCurrentDocument(b)) {
      return;
    }
  }

  m_globalState->deleteDocumentFile(b);
}


void EditorWindow::fileReload()
{
  if (reloadFile(this->currentDocument())) {
    this->editorViewChanged();
  }
}


bool EditorWindow::reloadFile(FileTextDocument *b_)
{
  // TODO: This function's signature is inherently dangerous because I
  // have no way of re-confirming 'b' after the dialog box closes since
  // I don't know where it came from.  For now I will just arrange to
  // abort before memory corruption can happen, but I should change the
  // callers to use a more reliable pattern.
  RCSerf<FileTextDocument> b(b_);

  if (b->unsavedChanges()) {
    if (!this->okToDiscardChanges(stringb(
          "The file \"" << b->filename << "\" has unsaved changes.  "
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
      "Can't read file \"" << b->filename << "\": " << x.why() <<
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

  for (int i=0; i < this->m_globalState->m_documentList.numFiles(); i++) {
    FileTextDocument *file = this->m_globalState->m_documentList.getFileAt(i);
    if (!this->reloadFile(file)) {
      // Stop after first error.
      break;
    }
  }

  this->editorViewChanged();
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

  msg << "The following files have unsaved changes:\n\n";
  for (int i=0; i < this->m_globalState->m_documentList.numFiles(); i++) {
    FileTextDocument *file = this->m_globalState->m_documentList.getFileAt(i);
    if (file->unsavedChanges()) {
      ct++;
      msg << " * " << file->filename << '\n';
    }
  }

  return ct;
}


bool EditorWindow::okToDiscardChanges(string const &descriptionOfChanges)
{
  QMessageBox box(this);
  box.setWindowTitle("Unsaved Changes");
  box.setText(toQString(descriptionOfChanges));
  box.addButton(QMessageBox::Discard);
  box.addButton(QMessageBox::Cancel);
  int ret = box.exec();
  return (ret == QMessageBox::Discard);
}


bool EditorWindow::eventFilter(QObject *watched, QEvent *event) NOEXCEPT
{
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
}


void EditorWindow::fileTextDocumentAdded(
  FileTextDocumentList *, FileTextDocument *) NOEXCEPT
{
  GENERIC_CATCH_BEGIN
  this->rebuildWindowMenu();
  GENERIC_CATCH_END
}

void EditorWindow::fileTextDocumentRemoved(
  FileTextDocumentList *, FileTextDocument *) NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  this->rebuildWindowMenu();

  // It is possible that the file our widget is editing is being
  // removed, in which case the widget will switch to another file and
  // we will have to update the filename display.  But it would be
  // wrong to assume here that the widget has already updated to the
  // new file since the order in which observers are notified is
  // undefined.
  //
  // Instead, those updates happen in response to the 'viewChanged'
  // signal, which the widget will emit when it learns of the change.

  GENERIC_CATCH_END
}

void EditorWindow::fileTextDocumentAttributeChanged(
  FileTextDocumentList *, FileTextDocument *) NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  this->rebuildWindowMenu();

  // The title of the file we are looking at could have changed.
  this->editorViewChanged();

  // The highlighter might have changed too.
  this->m_editorWidget->update();

  GENERIC_CATCH_END
}

void EditorWindow::fileTextDocumentListOrderChanged(
  FileTextDocumentList *) NOEXCEPT
{
  GENERIC_CATCH_BEGIN
  this->rebuildWindowMenu();
  GENERIC_CATCH_END
}


void EditorWindow::fileExit()
{
  if (this->canQuitApplication()) {
    GlobalState::quit();
  }
}


void EditorWindow::editISearch()
{
  m_sarPanel->toggleSARFocus();
}


void EditorWindow::editGotoLine()
{
  // Use a single dialog instance shared across all windows so they
  // can share the input history.
  static TextInputDialog *dialog;
  if (!dialog) {
    dialog = new TextInputDialog();
    dialog->setWindowTitle("Goto Line");
    dialog->setLabelText("Line number:");
  }

  // Populate with the current line number.  Among the benefits of that
  // is the goto-line dialog can act as a crude form of bookmark, where
  // you hit Alt+G, Enter to quickly add the current line number to the
  // history so you can later grab it again.
  dialog->m_text = qstringb(m_editorWidget->cursorLine() + 1);

  if (dialog->exec()) {
    string s = dialog->m_text.toUtf8().constData();

    if (!s.isempty()) {
      int n = atoi(s);
      if (n > 0) {
        dialog->rememberInput(qstringb(n));

        m_editorWidget->cursorTo(TextCoord(n-1, 0));
        m_editorWidget->scrollToCursor(-1 /*center*/);
      }
      else {
        this->complain(stringb("Invalid line number: " << s));
      }
    }
  }
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
    static TextInputDialog *dialog;
    if (!dialog) {
      dialog = new TextInputDialog();
      dialog->setWindowTitle("Apply Command");
    }

    if (dialog->isVisible()) {
      // This should be impossible because it is application-modal,
      // but I will be defensive.
      QMessageBox::information(this, "Dialog Already Shown",
        "The apply-command dialog is already visible elsewhere.  "
        "There can only be one instance of that dialog open.");
      return;
    }

    string dir = m_editorWidget->getDocumentDirectory();
    dialog->setLabelText(qstringb("Command to run in " << dir << ":"));

    if (!dialog->exec() || dialog->m_text.isEmpty()) {
      return;
    }
    dialog->rememberInput(dialog->m_text);
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
  {
    UndoHistoryGrouper ugh(*tde);
    tde->insertText(runner.getOutputData().constData(), runner.getOutputData().size());
  }
  m_editorWidget->redraw();

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
  CHECKABLE_MENU_TOGGLE(m_toggleHighlightTrailingWSAction,
    this->m_editorWidget->m_highlightTrailingWhitespace);
}


#undef CHECKABLE_MENU_TOGGLE


void EditorWindow::viewSetHighlighting()
{
  FileTextDocument *doc = this->currentDocument();

  QInputDialog dialog(this);
  dialog.setWindowTitle("Set Highlighting");
  dialog.setLabelText("Highlighting to use for this file:");
  dialog.setComboBoxItems(QStringList() << "None" << "C/C++");

  // One annoying thing is you can't double-click an item to choose
  // it and simultaneously close the dialog.
  dialog.setOption(QInputDialog::UseListViewForComboBoxItems);

  if (doc->highlighter) {
    dialog.setTextValue("C/C++");
  }
  else {
    dialog.setTextValue("None");
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
  delete doc->highlighter;
  doc->highlighter = NULL;

  // TODO: Obviously this is not a good method of recognition.
  if (chosen == "C/C++") {
    doc->highlighter = new C_Highlighter(doc->getCore());
  }

  // Notify everyone of the change.
  this->m_globalState->m_documentList.notifyAttributeChanged(doc);
}


void EditorWindow::windowOpenFilesList()
{
  FileTextDocument *doc = m_globalState->runOpenFilesDialog();
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


void EditorWindow::editorViewChanged()
{
  RCSerf<TextDocumentEditor> tde = m_editorWidget->getDocumentEditor();

  // Set the scrollbars.  In both dimensions, the range includes the
  // current value so we can scroll arbitrarily far beyond the nominal
  // size of the file contents.  Also, it is essential to set the range
  // *before* setting the value, since otherwise the scrollbar's value
  // will be clamped to the old range.
  if (m_horizScroll) {
    m_horizScroll->setRange(0, max(tde->maxLineLength(),
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
  m_statusArea->position->setText(QString(
    stringc << " " << (m_editorWidget->cursorLine()+1) << ":"
            << (m_editorWidget->cursorCol()+1)
            << (tde->unsavedChanges()? " *" : "")
  ));

  // Status text: full path name.
  FileTextDocument *file = currentDocument();
  m_statusArea->status->setText(toQString(file->filename));

  // Window title.
  stringBuilder sb;
  sb << file->title;
  if (tde->unsavedChanges()) {
    sb << " *";
  }
  string hotkey = file->hotkeyDesc();
  if (hotkey[0]) {
    sb << " [" << hotkey << "]";
  }
  sb << " - " << appName;
  this->setWindowTitle(toQString(sb));
}


void EditorWindow::on_closeSARPanel()
{
  if (m_sarPanel->isVisible()) {
    m_sarPanel->hide();
    this->m_editorWidget->setFocus();
  }
}


void EditorWindow::on_openFilenameInputDialogSignal(QString const &filename)
{
  // Check for fast-open conditions.
  {
    SMFileUtil sfu;
    string fn(toString(filename));
    if (sfu.absoluteFileExists(fn) &&
        currentDocument()->filename != fn) {
      // The file exists, and it is not the current document.  Just
      // go straight to opening it without prompting.
      this->fileOpenFile(fn);
      return;
    }
  }

  // Prompt to confirm.
  FilenameInputDialog dialog;
  QString confirmedFilename =
    dialog.runDialog(&(m_globalState->m_documentList), filename);

  if (!confirmedFilename.isEmpty()) {
    this->fileOpenFile(toString(confirmedFilename));
  }
}


void EditorWindow::rebuildWindowMenu()
{
  // remove all of the old menu items
  while (this->m_fileChoiceActions.isNotEmpty()) {
    QAction *action = this->m_fileChoiceActions.pop();
    this->m_windowMenu->removeAction(action);

    // Removing an action effectively means we take ownership of it.
    delete action;
  }

  // add new items for all of the open files;
  // hotkeys have already been assigned by now
  for (int i=0; i < this->m_globalState->m_documentList.numFiles(); i++) {
    FileTextDocument *b = this->m_globalState->m_documentList.getFileAt(i);

    QKeySequence keySequence;
    if (b->hasHotkey()) {
      keySequence = Qt::ALT + (Qt::Key_0 + b->getHotkeyDigit());
    }

    QAction *action = this->m_windowMenu->addAction(
      toQString(b->title),        // menu item text
      this,                       // receiver
      SLOT(windowFileChoice()), // slot name
      keySequence);               // accelerator

    // Associate the action with the FileTextDocument object.
    action->setData(QVariant(b->windowMenuId));

    this->m_fileChoiceActions.push(action);
  }
}


// Respond to the choice of an entry from the Window menu.
void EditorWindow::windowFileChoiceActivated(QAction *action)
{
  TRACE("menu", "window file choice activated");

  // Search through the list of files to find the one
  // that this action refers to.
  int windowMenuId = action->data().toInt();
  FileTextDocument *file =
    this->m_globalState->m_documentList.findFileByWindowMenuId(windowMenuId);
  if (file) {
    TRACE("menu", "window file choice is: " << file->filename);
    this->setDocumentFile(file);
  }
  else {
    // the id doesn't match any that I'm aware of; this happens
    // for window menu items that do *not* switch to some file
    TRACE("menu", "window file choice did not match any file");
  }
}

// This is just a placeholder.  Every QMenu::addAction() that
// accepts a shortcut also insists on having a receiver, so here
// we are.  But 'windowFileChoiceActivated' does all the work.
void EditorWindow::windowFileChoice()
{
  TRACE("menu", "window file choice");
  return;
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
