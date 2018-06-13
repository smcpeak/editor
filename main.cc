// main.cc
// code for main.cc, and application main() function

#include "main.h"            // EditorWindow
#include "buffer.h"          // Buffer
#include "editor.h"          // Editor
#include "exc.h"             // XOpen
#include "trace.h"           // TRACE_ARGS
#include "c_hilite.h"        // C_Highlighter
#include "incsearch.h"       // IncSearch
#include "strutil.h"         // sm_basename
#include "mysig.h"           // printSegfaultAddrs
#include "status.h"          // StatusDisplay
#include "gotoline.gen.h"    // Ui_GotoLine
#include "qtutil.h"          // toQString

#include <string.h>          // strrchr
#include <stdlib.h>          // atoi

#include <qmenubar.h>        // QMenuBar
#include <qscrollbar.h>      // QScrollBar
#include <qlabel.h>          // QLabel
#include <qfiledialog.h>     // QFileDialog
#include <qmessagebox.h>     // QMessageBox
#include <qlayout.h>         // QVBoxLayout
#include <qsizegrip.h>       // QSizeGrip
#include <qstatusbar.h>      // QStatusBar
#include <qlineedit.h>       // QLineEdit

#include <QDesktopWidget>


char const appName[] = "An Editor";   // TODO: find better name


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
    bufferChoiceActions(),
    recentMenuBuffer(NULL),
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

  this->editor = new Editor(NULL /*temporary*/, this->statusArea);
  this->editor->setObjectName("editor widget");
  editArea->addWidget(this->editor, 0 /*row*/, 0 /*col*/);
  this->editor->setFocus();
  connect(this->editor, SIGNAL(viewChanged()), this, SLOT(editorViewChanged()));

  this->vertScroll = new QScrollBar(Qt::Vertical);
  this->vertScroll->setObjectName("vertScroll");
  editArea->addWidget(this->vertScroll, 0 /*row*/, 1 /*col*/);
  connect(this->vertScroll, SIGNAL( valueChanged(int) ), this->editor, SLOT( scrollToLine(int) ));

  // disabling horiz scroll for now..
  //horizScroll = new QScrollBar(QScrollBar::Horizontal, editArea, "horizontal scrollbar");
  //connect(horizScroll, SIGNAL( valueChanged(int) ), editor, SLOT( scrollToCol(int) ));

  #if 0
  QHBox *statusArea = new QHBox(mainArea, "status area");
  statusArea->setFixedHeight(20);

  //QStatusBar *statusArea = new QStatusBar(mainArea, "status area");
  //statusArea->setFixedHeight(20);

  position = new QLabel(statusArea, "cursor position label");
  position->setFixedWidth(45);
  //position->setBackgroundColor(QColor(0x60, 0x00, 0x80));   // purple
  //position->setFrameStyle(QFrame::Panel | QFrame::Sunken);
  //position->setLineWidth(1);

  mode = new QLabel(statusArea);
  //mode->setPixmap(state->pixSearch);
  mode->setFixedWidth(65);

  filename = new QLabel(statusArea, "filename label");
  //filename->setFrameStyle(QFrame::Panel | QFrame::Sunken);
  //filename->setLineWidth(1);

  // corner resize widget
  QSizeGrip *corner = new QSizeGrip(statusArea, "corner grip");
  corner->setFixedSize(20,20);
  #endif // 0

  // menu
  {
    // TODO: replacement?  menuBar->setFrameStyle(QFrame::Panel | QFrame::Raised);
    // TODO: replacement?  menuBar->setLineWidth(1);

    QMenu *file = this->menuBar->addMenu("&File");
    file->addAction("&New", this, SLOT(fileNewFile()));
    file->addAction("&Open ...", this, SLOT(fileOpen()), Qt::Key_F3);
    file->addAction("&Save", this, SLOT(fileSave()), Qt::Key_F2);
    file->addAction("Save &as ...", this, SLOT(fileSaveAs()));
    file->addAction("&Close", this, SLOT(fileClose()));
    file->addSeparator();
    file->addAction("E&xit", theState, SLOT(quit()));

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

    QMenu *window = this->menuBar->addMenu("&Window");
    window->addAction("&New Window", this, SLOT(windowNewWindow()));
    window->addAction("Occupy Left", this, SLOT(windowOccupyLeft()), Qt::CTRL + Qt::ALT + Qt::Key_Left);
    window->addAction("Occupy Right", this, SLOT(windowOccupyRight()), Qt::CTRL + Qt::ALT + Qt::Key_Right);
    window->addAction("Cycle buffer", this, SLOT(windowCycleBuffer()), Qt::Key_F6);  // for now
    window->addSeparator();
    this->windowMenu = window;
    connect(this->windowMenu, SIGNAL(triggered(QAction*)),
            this, SLOT(windowBufferChoiceActivated(QAction*)));

    QMenu *help = this->menuBar->addMenu("&Help");
    help->addAction("&About ...", this, SLOT(helpAbout()));
    help->addAction("About &Qt ...", this, SLOT(helpAboutQt()));
  }

  this->setWindowIcon(pixmaps->icon);

  this->setLayout(mainArea);
  this->setGeometry(200,200,      // initial location
                    400,400);     // initial size

  // Set the BufferState, which was originally set as NULL above.
  this->setBuffer(initBuffer);

  // i-search; use filename area as the status display.
  this->isearch = new IncSearch(this->statusArea);
}


EditorWindow::~EditorWindow()
{
  delete isearch;
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
  QString name = QFileDialog::getOpenFileName(this, "Open File");
  if (name.isEmpty()) {
    return;
  }
  this->fileOpenFile(name.toUtf8().constData());
}

void EditorWindow::fileOpenFile(char const *name)
{
  BufferState *b = new BufferState();
  b->filename = name;
  b->title = sm_basename(name);   // shorter; todo: ensure unique

  try {
    b->readFile(name);
  }
  catch (XOpen &x) {
    QMessageBox::information(this, "Can't open file", toQString(x.why()));
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
      trace("untitled") << "found untitled buffer to remove\n";
      untitled = iter.data();

      // I'm going to remove it, but can't yet b/c I
      // need to wait until the new buffer is added;
      // but right now I can remove its hotkey so that
      // the new buffer can use it instead
      untitled->hotkeyDigit = 0;

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
  if (theBuffer()->title.equals("untitled.txt")) {
    trace("untitled") << "file has no title; invoking Save As ...\n";
    fileSaveAs();
    return;
  }

  writeTheFile();
}

void EditorWindow::writeTheFile()
{
  try {
    theBuffer()->writeFile(toCStr(theBuffer()->filename));
    theBuffer()->noUnsavedChanges();
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
}


void EditorWindow::fileClose()
{
  BufferState *b = theBuffer();
  if (b->unsavedChanges()) {
    int choice =
      QMessageBox::information(this, "Editor",
        "This file has unsaved changes.  Do you want to "
        "close it anyway?",
        "&Don't Close",
        "&Close",
        QString::null,    // no third button
        0,                // Enter -> Don't close
        1);               // Escape -> Close
    if (choice == 0) {
      return;            // Don't close
    }
  }

  globalState->deleteBuffer(b);
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


void EditorWindow::helpAbout()
{
  QMessageBox::about(this, "About...",
                     "This is a text editor, in case you didn't know.\n"
                     "It doesn't have a name yet.\n"
                     "\n"
                     "Step 1.  Write a good editor.\n"
                     "Step 2.  ???\n"
                     "Step 3.  Profit!");
}


void EditorWindow::helpAboutQt()
{
  QMessageBox::aboutQt(this, "An editor");
}


void EditorWindow::editorViewChanged()
{
  // set the scrollbars
  if (horizScroll) {
    horizScroll->setValue(editor->firstVisibleCol);
    horizScroll->setRange(0, max(editor->buffer->maxLineLength(),
                                 editor->firstVisibleCol));
    horizScroll->setSingleStep(1);
    horizScroll->setPageStep(editor->visCols());
  }

  if (vertScroll) {
    vertScroll->setValue(editor->firstVisibleLine);
    vertScroll->setRange(0, max(editor->buffer->numLines(),
                                editor->firstVisibleLine));
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
    if (b->hotkeyDigit) {
      keySequence = Qt::ALT + (Qt::Key_0 + b->hotkeyDigit);
    }

    QAction *action = this->windowMenu->addAction(
      toQString(b->title),        // menu item text
      this,                       // receiver
      SLOT(windowBufferChoice()), // slot name
      keySequence);               // accelerator

    this->bufferChoiceActions.push(action);
  }
}

  
// the user has chosen a window menu item; this is called
// just before windowBufferChoice(), for menu items that
// select buffers; this also gets called when the user uses
// the accelerator keybinding
void EditorWindow::windowBufferChoiceActivated(QAction *action)
{
  trace("menu") << "window buffer choice activated" << endl;

  // search through the list of buffers to find the one
  // that this action refers to; the buffers were marked
  // with their actions when the buffer menu was built
  FOREACH_OBJLIST_NC(BufferState, this->globalState->buffers, iter) {
    BufferState *b = iter.data();

    // TODO: This is not right because the title is not
    // necessarily unique.  Qt3 had numeric menu IDs that
    // I could use to distinguish the entries, but Qt5 only
    // has the QAction pointer, which is not safe to share
    // across EditorWindows.
    if (toQString(b->title) == action->text()) {
      this->recentMenuBuffer = b;
      trace("menu") << "window buffer choice is: " << b->filename << endl;
      return;
    }
  }

  // the id doesn't match any that I'm aware of; this happens
  // for window menu items that do *not* switch to some buffer
  trace("menu") << "window buffer choice did not match any buffer" << endl;
  this->recentMenuBuffer = NULL;
}

// respond to a choice of a buffer in the window item
void EditorWindow::windowBufferChoice()
{
  trace("menu") << "window buffer choice\n";

  if (this->recentMenuBuffer) {
    setBuffer(this->recentMenuBuffer);
  }
  else {
    // should not be reachable
    complain("windowBufferChoice() called but recentMenuBuffer "
             "is NULL!");
  }
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


// ---------------- GlobalState ----------------
GlobalState *GlobalState::state = NULL;

GlobalState::GlobalState(int argc, char **argv)
  : QApplication(argc, argv),
    pixmaps(),
    buffers(),
    windows()
{
  if (state == NULL) {
    state = this;
  }

#ifdef HAS_QWINDOWSSTYLE
  // use my variant of the Windows style, if we're using Windows at all
  if (0==strcmp("QWindowsStyle", style().className())) {
    setStyle(new MyWindowsStyle);
  }
#endif // HAS_QWINDOWSSTYLE

  EditorWindow *ed = createNewWindow(createNewFile());

  // this caption is immediately replaced with another one, at the
  // moment, since I call fileNewFile() right away
  //ed.setCaption("An Editor");

  // open all files specified on the command line
  for (int i=1; i < argc; i++) {
    ed->fileOpenFile(argv[i]);
  }

  // TODO: replacement?
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

EditorWindow *GlobalState::createNewWindow(BufferState *initBuffer)
{
  EditorWindow *ed = new EditorWindow(this, initBuffer);
  ed->setObjectName("main editor window");
  windows.append(ed);
  rebuildWindowMenus();

  // NOTE: caller still has to say 'ed->show()'!

  return ed;
}


GlobalState::~GlobalState()
{
  if (state == this) {
    state = NULL;
  }
}


BufferState *GlobalState::createNewFile()
{                        
  trace("untitled") << "creating untitled buffer\n";
  BufferState *b = new BufferState();
  b->filename = "untitled.txt";    // TODO: find a unique variant of this name
  b->title = b->filename;
  trackNewBuffer(b);
  return b;
}


void GlobalState::trackNewBuffer(BufferState *b)
{
  // assign hotkey
  b->hotkeyDigit = 0;
  for (int i=1; i<=10; i++) {
    if (hotkeyAvailable(i)) {
      b->hotkeyDigit = i;
      break;
    }
  }

  buffers.append(b);
  rebuildWindowMenus();
}

bool GlobalState::hotkeyAvailable(int key) const
{
  FOREACH_OBJLIST(BufferState, buffers, iter) {
    if (iter.data()->hotkeyDigit == key) {
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


int main(int argc, char **argv)
{
  TRACE_ARGS();

  // Not implemented in smbase for mingw.
  //printSegfaultAddrs();

  GlobalState app(argc, argv);

  return app.exec();
}
