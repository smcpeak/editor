// editor-window.cc
// code for editor-window.h

#include "editor-window.h"             // this module

// this dir
#include "c_hilite.h"                  // C_Highlighter
#include "command-runner.h"            // CommandRunner
#include "editor-widget.h"             // EditorWidget
#include "incsearch.h"                 // IncSearch
#include "keybindings.doc.gen.h"       // doc_keybindings
#include "keys-dialog.h"               // KeysDialog
#include "main.h"                      // GlobalState
#include "open-files-dialog.h"         // OpenFilesDialog
#include "pixmaps.h"                   // pixmaps
#include "qhboxframe.h"                // QHBoxFrame
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


EditorWindow::EditorWindow(GlobalState *theState, FileTextDocument *initFile,
                           QWidget *parent)
  : QWidget(parent),
    globalState(theState),
    menuBar(NULL),
    editorWidget(NULL),
    vertScroll(NULL),
    horizScroll(NULL),
    statusArea(NULL),
    //position(NULL),
    //mode(NULL),
    //filename(NULL),
    windowMenu(NULL),
    toggleVisibleWhitespaceAction(NULL),
    toggleVisibleSoftMarginAction(NULL),
    toggleHighlightTrailingWSAction(NULL),
    fileChoiceActions(),
    isearch(NULL)
{
  xassert(theState);
  xassert(initFile);

  // will build a layout tree to manage sizes of child widgets
  QVBoxLayout *mainArea = new QVBoxLayout();
  mainArea->setObjectName("mainArea");
  mainArea->setSpacing(0);
  mainArea->setContentsMargins(0, 0, 0, 0);

  this->menuBar = new QMenuBar();
  this->menuBar->setObjectName("main menu bar");
  mainArea->addWidget(this->menuBar);

  QGridLayout *editArea = new QGridLayout();
  editArea->setObjectName("editArea");
  mainArea->addLayout(editArea, 1 /*stretch*/);

  this->statusArea = new StatusDisplay();
  this->statusArea->setObjectName("statusArea");
  mainArea->addWidget(this->statusArea);

  // Put a black one-pixel frame around the editor widget.
  QHBoxFrame *editorFrame = new QHBoxFrame();
  editorFrame->setFrameStyle(QFrame::Box);
  editArea->addWidget(editorFrame, 0 /*row*/, 0 /*col*/);

  this->editorWidget = new EditorWidget(initFile,
    &(theState->m_documentList), this->statusArea);
  this->editorWidget->setObjectName("editor widget");
  editorFrame->addWidget(this->editorWidget);
  this->editorWidget->setFocus();
  connect(this->editorWidget, SIGNAL(viewChanged()), this, SLOT(editorViewChanged()));

  // See explanation in GlobalState::focusChangedHandler().
  this->setFocusProxy(this->editorWidget);

  this->vertScroll = new QScrollBar(Qt::Vertical);
  this->vertScroll->setObjectName("vertScroll");
  editArea->addWidget(this->vertScroll, 0 /*row*/, 1 /*col*/);
  connect(this->vertScroll, SIGNAL( valueChanged(int) ),
          this->editorWidget, SLOT( scrollToLine(int) ));

  // disabling horiz scroll for now..
  //horizScroll = new QScrollBar(QScrollBar::Horizontal, editArea, "horizontal scrollbar");
  //connect(horizScroll, SIGNAL( valueChanged(int) ), editor, SLOT( scrollToCol(int) ));

  this->buildMenu();
  this->rebuildWindowMenu();

  this->setWindowIcon(pixmaps->icon);

  this->setLayout(mainArea);
  this->setGeometry(400,100,      // initial location
                    800,800);     // initial size

  // Set scrollbar ranges, status bar text, and window title.
  this->updateForChangedFile();

  // i-search; use filename area as the status display.
  this->isearch = new IncSearch(this->statusArea);

  // I want this object destroyed when it is closed.
  this->setAttribute(Qt::WA_DeleteOnClose);

  this->globalState->m_windows.append(this);
  this->globalState->m_documentList.addObserver(this);

  EditorWindow::s_objectCount++;
}


EditorWindow::~EditorWindow()
{
  EditorWindow::s_objectCount--;

  this->globalState->m_documentList.removeObserver(this);

  // This object might have already been removed, for example because
  // the GlobalState destructor is running, and is in the process of
  // removing elements from the list and destroying them.
  this->globalState->m_windows.removeIfPresent(this);

  delete this->isearch;
}


void EditorWindow::buildMenu()
{
  {
    QMenu *file = this->menuBar->addMenu("&File");
    file->addAction("&New", this, SLOT(fileNewFile()));
    file->addAction("&Open ...", this, SLOT(fileOpen()), Qt::Key_F3);
    file->addAction("&Save", this, SLOT(fileSave()), Qt::Key_F2);
    file->addAction("Save &as ...", this, SLOT(fileSaveAs()));
    file->addAction("&Close", this, SLOT(fileClose()), Qt::CTRL + Qt::Key_F4);
    file->addSeparator();
    file->addAction("&Reload", this, SLOT(fileReload()));
    file->addAction("Reload a&ll", this, SLOT(fileReloadAll()));
    file->addSeparator();
    file->addAction("E&xit", this, SLOT(fileExit()));
  }

  {
    QMenu *edit = this->menuBar->addMenu("&Edit");
    edit->addAction("&Undo", editorWidget, SLOT(editUndo()), Qt::ALT + Qt::Key_Backspace);
    edit->addAction("&Redo", editorWidget, SLOT(editRedo()), Qt::ALT + Qt::SHIFT + Qt::Key_Backspace);
    edit->addSeparator();
    edit->addAction("Cu&t", editorWidget, SLOT(editCut()), Qt::CTRL + Qt::Key_X);
    edit->addAction("&Copy", editorWidget, SLOT(editCopy()), Qt::CTRL + Qt::Key_C);
    edit->addAction("&Paste", editorWidget, SLOT(editPaste()), Qt::CTRL + Qt::Key_V);
    edit->addAction("&Delete", editorWidget, SLOT(editDelete()));
    edit->addSeparator();
    edit->addAction("Inc. &Search", this, SLOT(editISearch()), Qt::CTRL + Qt::Key_S);
    edit->addAction("&Goto Line ...", this, SLOT(editGotoLine()), Qt::ALT + Qt::Key_G);
    edit->addAction("&Apply Command ...", this,
                    SLOT(editApplyCommand()), Qt::ALT + Qt::Key_A);
  }

  #define CHECKABLE_ACTION(field, title, slot, initChecked)  \
    this->field = menu->addAction(title, this, slot);        \
    this->field->setCheckable(true);                         \
    this->field->setChecked(initChecked) /* user ; */

  {
    QMenu *menu = this->menuBar->addMenu("&View");

    CHECKABLE_ACTION(toggleVisibleWhitespaceAction,
      "Visible &whitespace",
      SLOT(viewToggleVisibleWhitespace()),
      this->editorWidget->m_visibleWhitespace);

    menu->addAction("Set whitespace opacity...", this, SLOT(viewSetWhitespaceOpacity()));

    CHECKABLE_ACTION(toggleVisibleSoftMarginAction,
      "Visible soft &margin",
      SLOT(viewToggleVisibleSoftMargin()),
      this->editorWidget->m_visibleSoftMargin);

    menu->addAction("Set soft margin column...", this, SLOT(viewSetSoftMarginColumn()));

    CHECKABLE_ACTION(toggleHighlightTrailingWSAction,
      "Highlight &trailing whitespace",
      SLOT(viewToggleHighlightTrailingWS()),
      this->editorWidget->m_highlightTrailingWhitespace);
  }

  #undef CHECKABLE_ACTION

  {
    QMenu *window = this->menuBar->addMenu("&Window");

    window->addAction("Open &Files List ...", this,
      SLOT(windowOpenFilesList()), Qt::Key_F8);
    window->addAction("Cycle Through Files", this,
      SLOT(windowCycleFile()), Qt::Key_F6);

    window->addSeparator();

    window->addAction("&New Window", this, SLOT(windowNewWindow()));
    window->addAction("&Close Window", this, SLOT(windowCloseWindow()));
    window->addAction("Move/size to Left Screen Half", this,
      SLOT(windowOccupyLeft()), Qt::CTRL + Qt::ALT + Qt::Key_Left);
    window->addAction("Move/size to Right Screen Half", this,
      SLOT(windowOccupyRight()), Qt::CTRL + Qt::ALT + Qt::Key_Right);

    window->addSeparator();

    this->windowMenu = window;
    connect(this->windowMenu, SIGNAL(triggered(QAction*)),
            this, SLOT(windowFileChoiceActivated(QAction*)));
  }

  {
    QMenu *help = this->menuBar->addMenu("&Help");
    help->addAction("&Keybindings...", this, SLOT(helpKeybindings()));
    help->addAction("&About Scott's Editor...", this, SLOT(helpAbout()));
    help->addAction("About &Qt ...", this, SLOT(helpAboutQt()));
  }
}


// not inline because main.h doesn't see editor.h
FileTextDocument *EditorWindow::currentDocument()
{
  return editorWidget->getDocumentFile();
}


void EditorWindow::fileNewFile()
{
  FileTextDocument *b = globalState->createNewFile();
  setDocumentFile(b);
}


void EditorWindow::setDocumentFile(FileTextDocument *file)
{
  editorWidget->setDocumentFile(file);
  this->updateForChangedFile();
}

void EditorWindow::updateForChangedFile()
{
  editorWidget->recomputeLastVisible();
  editorViewChanged();
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
  dialog.setOptions(QFileDialog::DontUseNativeDialog);

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
    this->fileChooseDialog(editorWidget->getDocumentFile()->filename,
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
  FileTextDocument *file = globalState->m_documentList.findFileByName(name);
  if (file) {
    this->setDocumentFile(file);
    return;
  }

  file = new FileTextDocument();
  file->filename = name;
  file->isUntitled = false;
  file->title = this->globalState->uniqueTitleFor(file->filename);

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

  // get file extension
  char const *dot = strrchr(name.c_str(), '.');
  if (dot) {
    string ext = string(dot+1);
    if (ext.equals("h") ||
        ext.equals("cc") ||
        ext.equals("cpp")) {
      // make and attach a C++ highlighter for C/C++ files
      file->highlighter = new C_Highlighter(file->getCore());
    }
  }

  // is there an untitled, empty file hanging around?
  FileTextDocument *untitled =
    this->globalState->m_documentList.findUntitledUnmodifiedFile();
  if (untitled) {
    // I'm going to remove it, but can't yet b/c I
    // need to wait until the new file is added;
    // but right now I can remove its hotkey so that
    // the new file can use it instead.
    untitled->clearHotkey();
  }

  // now that we've opened the file, set the editor widget to edit it
  globalState->trackNewDocumentFile(file);
  setDocumentFile(file);

  // remove the untitled file now, if it exists
  if (untitled) {
    globalState->deleteDocumentFile(untitled);
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
  FileTextDocument *file = this->currentDocument();
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

    if (!fileDoc->isUntitled &&
        fileDoc->filename == chosenFilename) {
      this->fileSave();
      return;
    }

    if (this->globalState->hasFileWithName(chosenFilename)) {
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
  fileDoc->title = this->globalState->uniqueTitleFor(chosenFilename);
  writeTheFile();

  // Notify observers of the file name change.  This includes myself.
  this->globalState->m_documentList.notifyAttributeChanged(fileDoc);
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
  }

  globalState->deleteDocumentFile(b);
}


void EditorWindow::fileReload()
{
  if (reloadFile(this->currentDocument())) {
    this->editorViewChanged();
  }
}


bool EditorWindow::reloadFile(FileTextDocument *b)
{
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

  for (int i=0; i < this->globalState->m_documentList.numFiles(); i++) {
    FileTextDocument *file = this->globalState->m_documentList.getFileAt(i);
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
  for (int i=0; i < this->globalState->m_documentList.numFiles(); i++) {
    FileTextDocument *file = this->globalState->m_documentList.getFileAt(i);
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


void EditorWindow::fileTextDocumentAdded(
  FileTextDocumentList *, FileTextDocument *) noexcept
{
  GENERIC_CATCH_BEGIN
  this->rebuildWindowMenu();
  GENERIC_CATCH_END
}

void EditorWindow::fileTextDocumentRemoved(
  FileTextDocumentList *, FileTextDocument *) noexcept
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
  FileTextDocumentList *, FileTextDocument *) noexcept
{
  GENERIC_CATCH_BEGIN

  this->rebuildWindowMenu();

  // The title of the file we are looking at could have changed.
  this->editorViewChanged();

  GENERIC_CATCH_END
}

void EditorWindow::fileTextDocumentListOrderChanged(
  FileTextDocumentList *) noexcept
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
  isearch->attach(editorWidget);
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
  dialog->m_text = qstringb(editorWidget->cursorLine() + 1);

  if (dialog->exec()) {
    string s = dialog->m_text.toUtf8().constData();

    if (!s.isempty()) {
      int n = atoi(s);
      if (n > 0) {
        dialog->rememberInput(qstringb(n));

        editorWidget->cursorTo(TextCoord(n-1, 0));
        editorWidget->scrollToCursor(-1 /*center*/);
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

    FileTextDocument *file = editorWidget->getDocumentFile();
    string dir = dirname(file->filename);
    dialog->setLabelText(qstringb("Command to run in " << dir << ":"));

    if (!dialog->exec() || dialog->m_text.isEmpty()) {
      return;
    }
    dialog->rememberInput(dialog->m_text);
    commandString = toString(dialog->m_text);

    tde = editorWidget->getDocumentEditor();
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
    CursorSetRestore csr2(editorWidget, Qt::WaitCursor);

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
  if (tde != editorWidget->getDocumentEditor()) {
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
  editorWidget->redraw();

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
  this->editorWidget->update() /* user ; */


void EditorWindow::viewToggleVisibleWhitespace()
{
  CHECKABLE_MENU_TOGGLE(toggleVisibleWhitespaceAction,
    this->editorWidget->m_visibleWhitespace);
}


void EditorWindow::viewSetWhitespaceOpacity()
{
  bool ok;
  int n = QInputDialog::getInt(this,
    "Visible Whitespace",
    "Opacity in [1,255]:",
    this->editorWidget->m_whitespaceOpacity,
    1 /*min*/, 255 /*max*/, 1 /*step*/, &ok);
  if (ok) {
    this->editorWidget->m_whitespaceOpacity = n;
    this->editorWidget->update();
  }
}


void EditorWindow::viewToggleVisibleSoftMargin()
{
  CHECKABLE_MENU_TOGGLE(toggleVisibleSoftMarginAction,
    this->editorWidget->m_visibleSoftMargin);
}


void EditorWindow::viewSetSoftMarginColumn()
{
  bool ok;
  int n = QInputDialog::getInt(this,
    "Soft Margin Column",
    "Column number (positive):",
    this->editorWidget->m_softMarginColumn+1,
    1 /*min*/, INT_MAX /*max*/, 1 /*step*/, &ok);
  if (ok) {
    this->editorWidget->m_softMarginColumn = n-1;
    this->editorWidget->update();
  }
}


void EditorWindow::viewToggleHighlightTrailingWS()
{
  CHECKABLE_MENU_TOGGLE(toggleHighlightTrailingWSAction,
    this->editorWidget->m_highlightTrailingWhitespace);
}


#undef CHECKABLE_MENU_TOGGLE


void EditorWindow::windowOpenFilesList()
{
  OpenFilesDialog dialog(&( globalState->m_documentList ));
  FileTextDocument *doc = dialog.runDialog();
  if (doc) {
    this->setDocumentFile(doc);
  }
}


void EditorWindow::windowCycleFile()
{
  int cur = globalState->m_documentList.getFileIndex(currentDocument());
  xassert(cur >= 0);
  cur = (cur + 1) % globalState->m_documentList.numFiles();     // cycle
  setDocumentFile(globalState->m_documentList.getFileAt(cur));
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
    "This is a text editor that has a user interface I like.");
}


void EditorWindow::helpAboutQt()
{
  QMessageBox::aboutQt(this, "An editor");
}


void EditorWindow::editorViewChanged()
{
  TextDocumentEditor *tde = editorWidget->getDocumentEditor();

  // Set the scrollbars.  In both dimensions, the range includes the
  // current value so we can scroll arbitrarily far beyond the nominal
  // size of the file contents.  Also, it is essential to set the range
  // *before* setting the value, since otherwise the scrollbar's value
  // will be clamped to the old range.
  if (horizScroll) {
    horizScroll->setRange(0, max(tde->maxLineLength(),
                                 editorWidget->firstVisibleCol()));
    horizScroll->setValue(editorWidget->firstVisibleCol());
    horizScroll->setSingleStep(1);
    horizScroll->setPageStep(editorWidget->visCols());
  }

  if (vertScroll) {
    vertScroll->setRange(0, max(tde->numLines(),
                                editorWidget->firstVisibleLine()));
    vertScroll->setValue(editorWidget->firstVisibleLine());
    vertScroll->setSingleStep(1);
    vertScroll->setPageStep(editorWidget->visLines());
  }

  // I want the user to interact with line/col with a 1:1 origin,
  // even though the TextDocument interface uses 0:0.
  statusArea->position->setText(QString(
    stringc << " " << (editorWidget->cursorLine()+1) << ":"
            << (editorWidget->cursorCol()+1)
            << (tde->unsavedChanges()? " *" : "")
  ));

  // Status text: full path name.
  FileTextDocument *file = currentDocument();
  statusArea->status->setText(toQString(file->filename));

  // Window title.
  string s = string(appName) & ": ";
  string hotkey = file->hotkeyDesc();
  if (hotkey[0]) {
    s &= stringc << "[" << hotkey << "] ";
  }
  s &= file->title;
  this->setWindowTitle(toQString(s));
}


void EditorWindow::rebuildWindowMenu()
{
  // remove all of the old menu items
  while (this->fileChoiceActions.isNotEmpty()) {
    QAction *action = this->fileChoiceActions.pop();
    this->windowMenu->removeAction(action);

    // Removing an action effectively means we take ownership of it.
    delete action;
  }

  // add new items for all of the open files;
  // hotkeys have already been assigned by now
  for (int i=0; i < this->globalState->m_documentList.numFiles(); i++) {
    FileTextDocument *b = this->globalState->m_documentList.getFileAt(i);

    QKeySequence keySequence;
    if (b->hasHotkey()) {
      keySequence = Qt::ALT + (Qt::Key_0 + b->getHotkeyDigit());
    }

    QAction *action = this->windowMenu->addAction(
      toQString(b->title),        // menu item text
      this,                       // receiver
      SLOT(windowFileChoice()), // slot name
      keySequence);               // accelerator

    // Associate the action with the FileTextDocument object.
    action->setData(QVariant(b->windowMenuId));

    this->fileChoiceActions.push(action);
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
    this->globalState->m_documentList.findFileByWindowMenuId(windowMenuId);
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
  EditorWindow *ed = this->globalState->createNewWindow(this->currentDocument());
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
  if (this->globalState->m_windows.count() == 1) {
    if (!this->canQuitApplication()) {
      event->ignore();    // Prevent app from closing.
      return;
    }
  }

  event->accept();
}


// EOF
