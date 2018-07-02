// editor-window.cc
// code for editor-window.h

#include "editor-window.h"             // this module

// this dir
#include "c_hilite.h"                  // C_Highlighter
#include "editor-widget.h"             // EditorWidget
#include "incsearch.h"                 // IncSearch
#include "keybindings.doc.gen.h"       // doc_keybindings
#include "main.h"                      // GlobalState
#include "pixmaps.h"                   // pixmaps
#include "qhboxframe.h"                // QHBoxFrame
#include "status.h"                    // StatusDisplay
#include "td-editor.h"                 // TextDocumentEditor
#include "textinput.h"                 // TextInputDialog

// smqtutil
#include "qtutil.h"                    // toQString

// smbase
#include "exc.h"                       // XOpen
#include "mysig.h"                     // printSegfaultAddrs
#include "nonport.h"                   // fileOrDirectoryExists
#include "strutil.h"                   // dirname
#include "test.h"                      // PVAL
#include "trace.h"                     // TRACE_ARGS

#include <string.h>                    // strrchr
#include <stdlib.h>                    // atoi

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

#include <stdlib.h>                    // atoi


char const appName[] = "Editor";

int EditorWindow::objectCount = 0;


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
    &(theState->fileDocuments), this->statusArea);
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

  this->globalState->windows.append(this);
  this->globalState->fileDocuments.addObserver(this);

  EditorWindow::objectCount++;
}


EditorWindow::~EditorWindow()
{
  EditorWindow::objectCount--;

  this->globalState->fileDocuments.removeObserver(this);

  // This object might have already been removed, for example because
  // the GlobalState destructor is running, and is in the process of
  // removing elements from the list and destroying them.
  this->globalState->windows.removeIfPresent(this);

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
    file->addAction("&Close", this, SLOT(fileClose()));
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
  }

  {
    QMenu *menu = this->menuBar->addMenu("&View");
    this->toggleVisibleWhitespaceAction =
      menu->addAction("Visible &whitespace", this, SLOT(viewToggleVisibleWhitespace()));
    this->toggleVisibleWhitespaceAction->setCheckable(true);
    this->toggleVisibleWhitespaceAction->setChecked(this->editorWidget->m_visibleWhitespace);
    menu->addAction("Set whitespace opacity...", this, SLOT(viewSetWhitespaceOpacity()));
    this->toggleVisibleSoftMarginAction =
      menu->addAction("Visible soft &margin", this, SLOT(viewToggleVisibleSoftMargin()));
    this->toggleVisibleSoftMarginAction->setCheckable(true);
    this->toggleVisibleSoftMarginAction->setChecked(this->editorWidget->m_visibleSoftMargin);
    menu->addAction("Set soft margin column...", this, SLOT(viewSetSoftMarginColumn()));
  }

  {
    QMenu *window = this->menuBar->addMenu("&Window");
    window->addAction("&New Window", this, SLOT(windowNewWindow()));
    window->addAction("Occupy Left", this, SLOT(windowOccupyLeft()), Qt::CTRL + Qt::ALT + Qt::Key_Left);
    window->addAction("Occupy Right", this, SLOT(windowOccupyRight()), Qt::CTRL + Qt::ALT + Qt::Key_Right);
    window->addAction("Cycle through files", this, SLOT(windowCycleFile()), Qt::Key_F6);  // for now
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


void EditorWindow::setFileName(rostring name, rostring hotkey)
{
  statusArea->status->setText(toQString(name));

  string s = string(appName) & ": ";
  if (hotkey[0]) {
    s &= stringc << "[" << hotkey << "] ";
  }
  s &= name;

  this->setWindowTitle(toQString(s));
}


void EditorWindow::fileOpen()
{
  // Start in the directory containing the file currently shown.
  string dir = dirname(editorWidget->getDocumentFile()->filename);
  TRACE("fileOpen", "dir: " << dir);

  QString name = QFileDialog::getOpenFileName(this,
    "Open File",
    toQString(dir),    // dir
    QString(),         // filter
    NULL,              // selectedFilter
    QFileDialog::Options());    //  QFileDialog::DontUseNativeDialog);
  if (name.isEmpty()) {
    return;
  }
  this->fileOpenFile(name.toUtf8().constData());
}

void EditorWindow::fileOpenFile(char const *name)
{
  // If this file is already open, switch to it.
  FileTextDocument *file = globalState->fileDocuments.findFileByName(name);
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
      file->readFile(name);
      file->refreshModificationTime();
    }
    catch (XOpen &x) {
      this->complain(stringb(
        "Can't open file \"" << name << "\": " << x.why()));
      delete file;
      return;
    }
  }
  else {
    // Just have the file open with its name set but no content.
  }

  // get file extension
  char const *dot = strrchr(name, '.');
  if (dot) {
    string ext = string(dot+1);
    if (ext.equals("h") ||
        ext.equals("cc")) {
      // make and attach a C++ highlighter for C/C++ files
      file->highlighter = new C_Highlighter(file->getCore());
    }
  }

  // TODO: Move the untitled shenanigans into FileTextDocumentList.

  // is there an untitled, empty file hanging around?
  FileTextDocument *untitled =
    this->globalState->fileDocuments.findUntitledUnmodifiedFile();
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
  try {
    FileTextDocument *b = this->currentDocument();
    writeFile(b->getCore(), toCStr(b->filename));
    b->noUnsavedChanges();
    b->refreshModificationTime();
    editorViewChanged();
  }
  catch (XOpen &x) {
    QMessageBox::information(this, "Can't write file", toQString(x.why()));
  }
}


void EditorWindow::fileSaveAs()
{
  FileTextDocument *fileDoc = currentDocument();
  string chosenFilename = fileDoc->filename;
  while (true) {
    QString s = QFileDialog::getSaveFileName(
      this, "Save file as", toQString(chosenFilename));
    if (s.isEmpty()) {
      return;
    }

    chosenFilename = s.toUtf8().constData();

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
  setFileName(chosenFilename, fileDoc->hotkeyDesc());
  writeTheFile();

  // Notify observers of the file name change.
  this->globalState->fileDocuments.notifyAttributeChanged(fileDoc);
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
    b->readFile(b->filename.c_str());
    b->refreshModificationTime();
    return true;
  }
  catch (XOpen &x) {
    this->complain(stringb(
      "Can't open file \"" << b->filename << "\": " << x.why() <<
      "\n\nThe file will remain open in the editor with its "
      "old contents."));
    return false;
  }
  catch (xBase &x) {
    this->complain(stringb(
      "Error while reading file \"" << b->filename << "\": " << x.why() <<
      "\n\nThe file will remain open in the editor with the "
      "partial contents (if any) that were read before the error, "
      "but this may not match what is on disk."));
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

  for (int i=0; i < this->globalState->fileDocuments.numFiles(); i++) {
    FileTextDocument *file = this->globalState->fileDocuments.getFileAt(i);
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
  for (int i=0; i < this->globalState->fileDocuments.numFiles(); i++) {
    FileTextDocument *file = this->globalState->fileDocuments.getFileAt(i);
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
  FileTextDocumentList *, FileTextDocument *)
{
  this->rebuildWindowMenu();
}

void EditorWindow::fileTextDocumentRemoved(
  FileTextDocumentList *, FileTextDocument *)
{
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
}

void EditorWindow::fileTextDocumentAttributeChanged(
  FileTextDocumentList *, FileTextDocument *)
{
  this->rebuildWindowMenu();
}

void EditorWindow::fileTextDocumentListOrderChanged(
  FileTextDocumentList *)
{
  this->rebuildWindowMenu();
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


void EditorWindow::viewToggleVisibleWhitespace()
{
  this->editorWidget->m_visibleWhitespace = !this->editorWidget->m_visibleWhitespace;
  this->toggleVisibleWhitespaceAction->setChecked(this->editorWidget->m_visibleWhitespace);
  this->editorWidget->update();
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
  this->editorWidget->m_visibleSoftMargin = !this->editorWidget->m_visibleSoftMargin;
  this->toggleVisibleSoftMarginAction->setChecked(this->editorWidget->m_visibleSoftMargin);
  this->editorWidget->update();
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


void EditorWindow::windowCycleFile()
{
  int cur = globalState->fileDocuments.getFileIndex(currentDocument());
  xassert(cur >= 0);
  cur = (cur + 1) % globalState->fileDocuments.numFiles();     // cycle
  setDocumentFile(globalState->fileDocuments.getFileAt(cur));
}


void EditorWindow::helpKeybindings()
{
  QMessageBox m(this);
  m.setWindowTitle("Keybindings");
  m.setText(doc_keybindings);
  m.exec();
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

  setFileName(currentDocument()->title, currentDocument()->hotkeyDesc());
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
  for (int i=0; i < this->globalState->fileDocuments.numFiles(); i++) {
    FileTextDocument *b = this->globalState->fileDocuments.getFileAt(i);

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
    this->globalState->fileDocuments.findFileByWindowMenuId(windowMenuId);
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


void EditorWindow::closeEvent(QCloseEvent *event)
{
  if (this->globalState->windows.count() == 1) {
    if (!this->canQuitApplication()) {
      event->ignore();    // Prevent app from closing.
      return;
    }
  }

  event->accept();
}


// EOF
