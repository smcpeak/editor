// main.cc
// code for main.cc, and application main() function

#include "main.h"                      // EditorWindow

#include "buffer.h"                    // Buffer
#include "c_hilite.h"                  // C_Highlighter
#include "editor.h"                    // Editor
#include "exc.h"                       // XOpen
#include "gotoline.gen.h"              // Ui_GotoLine
#include "incsearch.h"                 // IncSearch
#include "keybindings.doc.gen.h"       // doc_keybindings
#include "mysig.h"                     // printSegfaultAddrs
#include "qhboxframe.h"                // QHBoxFrame
#include "qtutil.h"                    // toQString
#include "status.h"                    // StatusDisplay
#include "strutil.h"                   // sm_basename
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
#include <QStyleFactory>

#include <stdlib.h>                    // atoi


char const appName[] = "Scott's Editor";

int EditorWindow::objectCount = 0;


EditorWindow::EditorWindow(GlobalState *theState, BufferState *initBuffer,
                           QWidget *parent)
  : QWidget(parent),
    globalState(theState),
    menuBar(NULL),
    editor(NULL),
    vertScroll(NULL),
    horizScroll(NULL),
    statusArea(NULL),
    //position(NULL),
    //mode(NULL),
    //filename(NULL),
    windowMenu(NULL),
    toggleVisibleWhitespaceAction(NULL),
    bufferChoiceActions(),
    isearch(NULL)
{
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

  this->editor = new Editor(NULL /*temporary*/, this->statusArea);
  this->editor->setObjectName("editor widget");
  editorFrame->addWidget(this->editor);
  this->editor->setFocus();
  connect(this->editor, SIGNAL(viewChanged()), this, SLOT(editorViewChanged()));

  this->vertScroll = new QScrollBar(Qt::Vertical);
  this->vertScroll->setObjectName("vertScroll");
  editArea->addWidget(this->vertScroll, 0 /*row*/, 1 /*col*/);
  connect(this->vertScroll, SIGNAL( valueChanged(int) ),
          this->editor, SLOT( scrollToLine(int) ));

  // disabling horiz scroll for now..
  //horizScroll = new QScrollBar(QScrollBar::Horizontal, editArea, "horizontal scrollbar");
  //connect(horizScroll, SIGNAL( valueChanged(int) ), editor, SLOT( scrollToCol(int) ));

  this->buildMenu();

  this->setWindowIcon(pixmaps->icon);

  this->setLayout(mainArea);
  this->setGeometry(400,100,      // initial location
                    800,800);     // initial size

  // Set the BufferState, which was originally set as NULL above.
  this->setBuffer(initBuffer);

  // i-search; use filename area as the status display.
  this->isearch = new IncSearch(this->statusArea);

  // I want this object destroyed when it is closed.
  this->setAttribute(Qt::WA_DeleteOnClose);

  this->globalState->windows.append(this);

  EditorWindow::objectCount++;
}


EditorWindow::~EditorWindow()
{
  EditorWindow::objectCount--;

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
    edit->addAction("&Undo", editor, SLOT(editUndo()), Qt::ALT + Qt::Key_Backspace);
    edit->addAction("&Redo", editor, SLOT(editRedo()), Qt::ALT + Qt::SHIFT + Qt::Key_Backspace);
    edit->addSeparator();
    edit->addAction("Cu&t", editor, SLOT(editCut()), Qt::CTRL + Qt::Key_X);
    edit->addAction("&Copy", editor, SLOT(editCopy()), Qt::CTRL + Qt::Key_C);
    edit->addAction("&Paste", editor, SLOT(editPaste()), Qt::CTRL + Qt::Key_V);
    edit->addAction("&Delete", editor, SLOT(editDelete()));
    edit->addSeparator();
    edit->addAction("Inc. &Search", this, SLOT(editISearch()), Qt::CTRL + Qt::Key_S);
    edit->addAction("&Goto Line ...", this, SLOT(editGotoLine()), Qt::ALT + Qt::Key_G);
  }

  {
    QMenu *menu = this->menuBar->addMenu("&View");
    this->toggleVisibleWhitespaceAction =
      menu->addAction("Visible &whitespace", this, SLOT(viewToggleVisibleWhitespace()));
    this->toggleVisibleWhitespaceAction->setCheckable(true);
    this->toggleVisibleWhitespaceAction->setChecked(this->editor->visibleWhitespace);
    menu->addAction("Set whitespace opacity...", this, SLOT(viewSetWhitespaceOpacity()));
  }

  {
    QMenu *window = this->menuBar->addMenu("&Window");
    window->addAction("&New Window", this, SLOT(windowNewWindow()));
    window->addAction("Occupy Left", this, SLOT(windowOccupyLeft()), Qt::CTRL + Qt::ALT + Qt::Key_Left);
    window->addAction("Occupy Right", this, SLOT(windowOccupyRight()), Qt::CTRL + Qt::ALT + Qt::Key_Right);
    window->addAction("Cycle buffer", this, SLOT(windowCycleBuffer()), Qt::Key_F6);  // for now
    window->addSeparator();
    this->windowMenu = window;
    connect(this->windowMenu, SIGNAL(triggered(QAction*)),
            this, SLOT(windowBufferChoiceActivated(QAction*)));
  }

  {
    QMenu *help = this->menuBar->addMenu("&Help");
    help->addAction("&Keybindings...", this, SLOT(helpKeybindings()));
    help->addAction("&About Scott's Editor...", this, SLOT(helpAbout()));
    help->addAction("About &Qt ...", this, SLOT(helpAboutQt()));
  }
}


// not inline because main.h doesn't see editor.h
BufferState *EditorWindow::theBuffer()
{
  return editor->buffer;
}


void EditorWindow::fileNewFile()
{
  BufferState *b = globalState->createNewFile();
  setBuffer(b);
}


void EditorWindow::setBuffer(BufferState *b)
{
  editor->setBuffer(b);

  //editor->resetView();
  editor->updateView();
  editorViewChanged();

  setFileName(theBuffer()->title, theBuffer()->hotkeyDesc());
}


void EditorWindow::setFileName(rostring name, rostring hotkey)
{
  statusArea->status->setText(toQString(name));

  string s = string(appName) & ": ";
  if (hotkey[0]) {
    s &= stringc << "[" << hotkey << "] ";
  }
  s &= sm_basename(name);

  this->setWindowTitle(toQString(s));
}


void EditorWindow::fileOpen()
{
  QString name = QFileDialog::getOpenFileName(this,
    "Open File",
    QString(),         // dir
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
  FOREACH_OBJLIST_NC(BufferState, globalState->buffers, iter) {
    BufferState *bs = iter.data();
    if (bs->filename.equals(name)) {
      this->setBuffer(bs);
      return;
    }
  }

  BufferState *b = new BufferState();
  b->filename = name;
  b->title = sm_basename(name);   // shorter; todo: ensure unique

  try {
    b->readFile(name);
    b->refreshModificationTime();
  }
  catch (XOpen &x) {
    this->complain(stringb(
      "Can't open file \"" << name << "\": " << x.why()));
    delete b;
    return;
  }

  // get file extension
  char const *dot = strrchr(name, '.');
  if (dot) {
    string ext = string(dot+1);
    if (ext.equals("h") ||
        ext.equals("cc")) {
      // make and attach a C++ highlighter for C/C++ files
      b->highlighter = new C_Highlighter(b->core());
    }
  }

  // is there an untitled, empty file hanging around?
  BufferState *untitled = NULL;
  FOREACH_OBJLIST_NC(BufferState, globalState->buffers, iter) {
    BufferState *bs = iter.data();
    if (bs->title.equals("untitled.txt") &&
        bs->numLines() == 1 &&
        bs->lineLength(0) == 0) {
      TRACE("untitled", "found untitled buffer to remove");
      untitled = iter.data();

      // I'm going to remove it, but can't yet b/c I
      // need to wait until the new buffer is added;
      // but right now I can remove its hotkey so that
      // the new buffer can use it instead.
      untitled->clearHotkey();

      break;
    }
  }

  // now that we've opened the file, set the editor widget to edit it
  globalState->trackNewBuffer(b);
  setBuffer(b);

  // remove the untitled buffer now, if it exists
  if (untitled) {
    globalState->deleteBuffer(untitled);
  }
}


void EditorWindow::fileSave()
{
  BufferState *b = this->theBuffer();
  if (b->title.equals("untitled.txt")) {
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
      "in the editor buffer.  Save anyway?")));
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
    BufferState *b = this->theBuffer();
    b->writeFile(toCStr(b->filename));
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
  QString s = QFileDialog::getSaveFileName(
    this, "Save file as", toQString(theBuffer()->filename));
  if (s.isEmpty()) {
    return;
  }

  QByteArray utf8(s.toUtf8());
  char const *name = utf8.constData();
  theBuffer()->filename = name;
  theBuffer()->title = sm_basename(name);
  setFileName(name, theBuffer()->hotkeyDesc());
  writeTheFile();

  this->globalState->rebuildWindowMenus();
}


void EditorWindow::fileClose()
{
  BufferState *b = theBuffer();
  if (b->unsavedChanges()) {
    stringBuilder msg;
    msg << "The file \"" << b->filename << "\" has unsaved changes.  "
        << "Discard these changes and close this file anyway?";
    if (!this->okToDiscardChanges(msg)) {
      return;
    }
  }

  globalState->deleteBuffer(b);
}


void EditorWindow::fileReload()
{
  if (reloadBuffer(this->theBuffer())) {
    this->editorViewChanged();
  }
}


bool EditorWindow::reloadBuffer(BufferState *b)
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
      "\n\nThe buffer will remain open in the editor with its "
      "old contents."));
    return false;
  }
  catch (xBase &x) {
    this->complain(stringb(
      "Error while reading file \"" << b->filename << "\": " << x.why() <<
      "\n\nThe buffer will remain open in the editor with the "
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

  FOREACH_OBJLIST_NC(BufferState, globalState->buffers, iter) {
    BufferState *bs = iter.data();
    if (!this->reloadBuffer(bs)) {
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

  msg << "The following buffers have unsaved changes:\n\n";
  FOREACH_OBJLIST(BufferState, this->globalState->buffers, iter) {
    if (iter.data()->unsavedChanges()) {
      ct++;
      msg << " * " << iter.data()->filename << '\n';
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


void EditorWindow::fileExit()
{
  if (this->canQuitApplication()) {
    GlobalState::quit();
  }
}


void EditorWindow::editISearch()
{
  isearch->attach(editor);
}


void EditorWindow::editGotoLine()
{
  Ui_GotoLine gl;
  QDialog dialog;
  gl.setupUi(&dialog);

  // I find it a bit bothersome to do this manually.. perhaps there
  // is a way to query the tab order, and always set focus to the
  // first widget in that order?
  gl.lineNumberEdit->setFocus();

  //bool hasWT = gl.testWFlags(WStyle_ContextHelp);
  //cout << "hasWT: " << hasWT << endl;

  if (dialog.exec()) {
    // user pressed Ok (or Enter)
    string s = gl.lineNumberEdit->text().toUtf8().constData();
    //cout << "text is \"" << s << "\"\n";

    if (s.length()) {
      int n = atoi(s);

      //cout << "going to line " << n << endl;
      editor->cursorTo(n-1, 0);
      editor->scrollToCursor(-1 /*center*/);
    }
  }
}


void EditorWindow::viewToggleVisibleWhitespace()
{
  this->editor->visibleWhitespace = !this->editor->visibleWhitespace;
  this->toggleVisibleWhitespaceAction->setChecked(this->editor->visibleWhitespace);
  this->editor->update();
}


void EditorWindow::viewSetWhitespaceOpacity()
{
  bool ok;
  int n = QInputDialog::getInt(this,
    "Visible Whitespace",
    "Opacity in [1,255]:",
    this->editor->whitespaceOpacity,
    1 /*min*/, 255 /*max*/, 1 /*step*/, &ok);
  if (ok) {
    this->editor->whitespaceOpacity = n;
    this->editor->update();
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


void EditorWindow::windowCycleBuffer()
{
  int cur = globalState->buffers.indexOf(theBuffer());
  xassert(cur >= 0);
  cur = (cur + 1) % globalState->buffers.count();     // cycle
  setBuffer(globalState->buffers.nth(cur));
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
  // Set the scrollbars.  In both dimensions, the range includes the
  // current value so we can scroll arbitrarily far beyond the nominal
  // size of the file contents.  Also, it is essential to set the range
  // *before* setting the value, since otherwise the scrollbar's value
  // will be clamped to the old range.
  if (horizScroll) {
    horizScroll->setRange(0, max(editor->buffer->maxLineLength(),
                                 editor->firstVisibleCol));
    horizScroll->setValue(editor->firstVisibleCol);
    horizScroll->setSingleStep(1);
    horizScroll->setPageStep(editor->visCols());
  }

  if (vertScroll) {
    vertScroll->setRange(0, max(editor->buffer->numLines(),
                                editor->firstVisibleLine));
    vertScroll->setValue(editor->firstVisibleLine);
    vertScroll->setSingleStep(1);
    vertScroll->setPageStep(editor->visLines());
  }

  // I want the user to interact with line/col with a 1:1 origin,
  // even though the Buffer interface uses 0:0
  statusArea->position->setText(QString(
    stringc << " " << (editor->cursorLine()+1) << ":"
            << (editor->cursorCol()+1)
            << (editor->buffer->unsavedChanges()? " *" : "")
  ));
}


void EditorWindow::rebuildWindowMenu()
{
  // remove all of the old menu items
  while (this->bufferChoiceActions.isNotEmpty()) {
    QAction *action = this->bufferChoiceActions.pop();
    this->windowMenu->removeAction(action);

    // Removing an action effectively means we take ownership of it.
    delete action;
  }

  // add new items for all of the open buffers;
  // hotkeys have already been assigned by now
  FOREACH_OBJLIST_NC(BufferState, this->globalState->buffers, iter) {
    BufferState *b = iter.data();

    QKeySequence keySequence;
    if (b->hasHotkey()) {
      keySequence = Qt::ALT + (Qt::Key_0 + b->getHotkeyDigit());
    }

    QAction *action = this->windowMenu->addAction(
      toQString(b->title),        // menu item text
      this,                       // receiver
      SLOT(windowBufferChoice()), // slot name
      keySequence);               // accelerator

    // Associate the action with the BufferState object.
    action->setData(QVariant(b->windowMenuId));

    this->bufferChoiceActions.push(action);
  }
}


// Respond to the choice of an entry from the Window menu.
void EditorWindow::windowBufferChoiceActivated(QAction *action)
{
  TRACE("menu", "window buffer choice activated");

  // Search through the list of buffers to find the one
  // that this action refers to.
  FOREACH_OBJLIST_NC(BufferState, this->globalState->buffers, iter) {
    BufferState *b = iter.data();

    if (b->windowMenuId == action->data().toInt()) {
      TRACE("menu", "window buffer choice is: " << b->filename);
      this->setBuffer(b);
      return;
    }
  }

  // the id doesn't match any that I'm aware of; this happens
  // for window menu items that do *not* switch to some buffer
  TRACE("menu", "window buffer choice did not match any buffer");
}

// This is just a placeholder.  Every QMenu::addAction() that
// accepts a shortcut also insists on having a receiver, so here
// we are.  But 'windowBufferChoiceActivated' does all the work.
void EditorWindow::windowBufferChoice()
{
  TRACE("menu", "window buffer choice");
  return;
}


void EditorWindow::complain(char const *msg)
{
  QMessageBox::information(this, appName, msg);
}


void EditorWindow::windowNewWindow()
{
  EditorWindow *ed = this->globalState->createNewWindow(this->theBuffer());
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
GlobalState *GlobalState::global_globalState = NULL;

GlobalState::GlobalState(int argc, char **argv)
  : QApplication(argc, argv),
    pixmaps(),
    buffers(),
    windows()
{
  // There should only be one of these.
  xassert(global_globalState == NULL);
  global_globalState = this;

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

  EditorWindow *ed = createNewWindow(createNewFile());

  // this caption is immediately replaced with another one, at the
  // moment, since I call fileNewFile() right away
  //ed.setCaption("An Editor");

  // open all files specified on the command line
  for (int i=1; i < argc; i++) {
    ed->fileOpenFile(argv[i]);
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

  ed->show();
}


GlobalState::~GlobalState()
{
  if (global_globalState == this) {
    global_globalState = NULL;
  }
}


EditorWindow *GlobalState::createNewWindow(BufferState *initBuffer)
{
  EditorWindow *ed = new EditorWindow(this, initBuffer);
  ed->setObjectName("main editor window");
  rebuildWindowMenus();

  // NOTE: caller still has to say 'ed->show()'!

  return ed;
}


BufferState *GlobalState::createNewFile()
{
  TRACE("untitled", "creating untitled buffer");
  BufferState *b = new BufferState();
  b->filename = "untitled.txt";    // TODO: find a unique variant of this name
  b->title = b->filename;
  trackNewBuffer(b);
  return b;
}


void GlobalState::trackNewBuffer(BufferState *b)
{
  // assign hotkey
  b->clearHotkey();
  for (int i=1; i<=10; i++) {
    // Use 0 as the tenth digit in this sequence to match the order
    // of the digit keys on the keyboard.
    int digit = (i==10? 0 : i);

    if (hotkeyAvailable(digit)) {
      b->setHotkeyDigit(digit);
      break;
    }
  }

  buffers.append(b);
  rebuildWindowMenus();
}

bool GlobalState::hotkeyAvailable(int key) const
{
  FOREACH_OBJLIST(BufferState, buffers, iter) {
    if (iter.data()->hasHotkey() &&
        iter.data()->getHotkeyDigit() == key) {
      return false;    // not available
    }
  }
  return true;         // available
}

void GlobalState::rebuildWindowMenus()
{
  FOREACH_OBJLIST_NC(EditorWindow, windows, iter) {
    iter.data()->rebuildWindowMenu();
  }
}


void GlobalState::deleteBuffer(BufferState *b)
{
  // where in the list is 'b'?
  int index = buffers.indexOf(b);
  xassert(index >= 0);

  // remove it
  buffers.removeItem(b);

  // select new buffer to display, for every window
  // that previously was displaying 'b'
  if (index > 0) {
    index--;
  }

  if (buffers.isEmpty()) {
    createNewFile();    // ensure there's always one
  }

  // update any windows that are still referring to 'b'
  // so that instead they (somewhat arbitrarily) refer
  // to whatever is in 'buffers' index slot 'index'
  FOREACH_OBJLIST_NC(EditorWindow, windows, iter) {
    EditorWindow *ed = iter.data();

    if (ed->editor->buffer == b) {
      ed->setBuffer(buffers.nth(index));
    }

    // also, rebuild all the window menus
    ed->rebuildWindowMenu();
  }

  delete b;
}


static void printObjectCounts(char const *when)
{
  cout << "Counts " << when << ':' << endl;
  PVAL(Editor::objectCount);
  PVAL(EditorWindow::objectCount);
  PVAL(BufferState::objectCount);
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

    if (tracingSys("objectCount")) {
      printObjectCounts("before GlobalState destruction");
    }
  }

  if (tracingSys("objectCount")) {
    printObjectCounts("after GlobalState destruction");
  }

  return ret;
}
