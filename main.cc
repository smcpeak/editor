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
#include "gotoline.h"        // GotoLine

#include <string.h>          // strrchr
#include <stdlib.h>          // atoi

#include <qmenubar.h>        // QMenuBar
#include <qscrollbar.h>      // QScrollBar
#include <qlabel.h>          // QLabel
#include <qfiledialog.h>     // QFileDialog
#include <qmessagebox.h>     // QMessageBox
#include <qvbox.h>           // QVBox
#include <qlayout.h>         // QVBoxLayout
#include <qhbox.h>           // QHBox
#include <qgrid.h>           // QGrid
#include <qsizegrip.h>       // QSizeGrip
#include <qstatusbar.h>      // QStatusBar


char const appName[] = "An Editor";   // TODO: find better name


EditorWindow::EditorWindow(GlobalState *theState, BufferState *initBuffer,
                           QWidget *parent, char const *name)
  : QWidget(parent, name),
    state(theState),
    menuBar(NULL),
    editor(NULL),
    vertScroll(NULL),
    horizScroll(NULL),
    statusArea(NULL),
    //position(NULL),
    //mode(NULL),
    //filename(NULL),
    windowMenu(NULL),
    bufferChoiceIds(),
    recentMenuBuffer(NULL),
    isearch(NULL)
{
  // will build a layout tree to manage sizes of child widgets
  QVBoxLayout *mainAreaMgr = new QVBoxLayout(this);
  QVBox *mainArea = new QVBox(this, "main area");
  mainAreaMgr->addWidget(mainArea);    // get 'mainArea' to fill the dialog

  QGrid *editArea = new QGrid(2 /*cols*/, QGrid::Horizontal, mainArea, "edit area");

  statusArea = new StatusDisplay(mainArea);

  editor = new Editor(NULL /*temporary*/, statusArea,
                      editArea, "editor widget");
  editor->setFocus();
  connect(editor, SIGNAL(viewChanged()), this, SLOT(editorViewChanged()));

  vertScroll = new QScrollBar(QScrollBar::Vertical, editArea, "vertical scrollbar");
  connect(vertScroll, SIGNAL( valueChanged(int) ), editor, SLOT( scrollToLine(int) ));

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
    menuBar = new QMenuBar(mainArea, "main menu bar");
    menuBar->setFrameStyle(QFrame::Panel | QFrame::Raised);
    menuBar->setLineWidth(1);

    QPopupMenu *file = new QPopupMenu(this);
    menuBar->insertItem("&File", file);
    file->insertItem("&New", this, SLOT(fileNewFile()));
    file->insertItem("&Open ...", this, SLOT(fileOpen()), Key_F3);
    file->insertItem("&Save", this, SLOT(fileSave()), Key_F2);
    file->insertItem("Save &as ...", this, SLOT(fileSaveAs()));
    file->insertItem("&Close", this, SLOT(fileClose()));
    file->insertSeparator();
    file->insertItem("E&xit", theState, SLOT(quit()));

    QPopupMenu *edit = new QPopupMenu(this);
    menuBar->insertItem("&Edit", edit);
    edit->insertItem("&Undo", editor, SLOT(editUndo()), ALT+Key_Backspace);
    edit->insertItem("&Redo", editor, SLOT(editRedo()), ALT+SHIFT+Key_Backspace);
    edit->insertSeparator();
    edit->insertItem("Cu&t", editor, SLOT(editCut()), CTRL+Key_X);
    edit->insertItem("&Copy", editor, SLOT(editCopy()), CTRL+Key_C);
    edit->insertItem("&Paste", editor, SLOT(editPaste()), CTRL+Key_V);
    edit->insertItem("&Delete", editor, SLOT(editDelete()));
    edit->insertSeparator();
    edit->insertItem("Inc. &Search", this, SLOT(editISearch()), CTRL+Key_S);
    edit->insertItem("&Goto Line ...", this, SLOT(editGotoLine()), ALT+Key_G);

    QPopupMenu *window = new QPopupMenu(this);
    menuBar->insertItem("&Window", window);
    window->insertItem("&New Window", this, SLOT(windowNewWindow()));
    window->insertItem("Occupy Left", this, SLOT(windowOccupyLeft()), CTRL+ALT+Key_Left);
    window->insertItem("Occupy Right", this, SLOT(windowOccupyRight()), CTRL+ALT+Key_Right);
    window->insertItem("Cycle buffer", this, SLOT(windowCycleBuffer()), Key_F6);  // for now
    window->insertSeparator();
    windowMenu = window;
    connect(windowMenu, SIGNAL(activated(int)), 
            this, SLOT(windowBufferChoiceActivated(int)));

    QPopupMenu *help = new QPopupMenu(this);
    menuBar->insertItem("&Help", help);
    help->insertItem("&About ...", this, SLOT(helpAbout()));
    help->insertItem("About &Qt ...", this, SLOT(helpAboutQt()));
  }

  setIcon(pixmaps->icon);

  setGeometry(200,200,      // initial location
              400,400);     // initial size

  setBuffer(initBuffer);

  // i-search; use filename as status display
  isearch = new IncSearch(statusArea);
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
  BufferState *b = state->createNewFile();
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


void EditorWindow::setFileName(char const *name, char const *hotkey)
{
  statusArea->status->setText(name);
  
  string s = string(appName) & ": ";
  if (hotkey && hotkey[0]) {
    s &= stringc << "[" << hotkey << "] ";
  }
  s &= sm_basename(name);
    
  setCaption(QString(s));
}


void EditorWindow::fileOpen()
{
  QString name = QFileDialog::getOpenFileName(QString::null, "Files (*)", this);
  if (name.isEmpty()) {
    return;
  }
  fileOpenFile(name);
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
    QMessageBox::information(this, "Can't open file", QString(x.why()));
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
  FOREACH_OBJLIST_NC(BufferState, state->buffers, iter) {
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
      untitled->hotkey = 0;

      break;
    }
  }

  // now that we've opened the file, set the editor widget to edit it
  state->trackNewBuffer(b);
  setBuffer(b);

  // remove the untitled buffer now, if it exists
  if (untitled) {
    state->deleteBuffer(untitled);
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
    theBuffer()->writeFile(theBuffer()->filename);
    theBuffer()->noUnsavedChanges();
    editorViewChanged();
  }
  catch (XOpen &x) {
    QMessageBox::information(this, "Can't write file", QString(x.why()));
  }
}


void EditorWindow::fileSaveAs()
{
  QString s = QFileDialog::getSaveFileName(QString(theBuffer()->filename),
                                           "Files (*)", this);
  if (s.isEmpty()) {
    return;
  }

  char const *name = s;
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

  state->deleteBuffer(b);
}


void EditorWindow::editISearch()
{
  isearch->attach(editor);
}                        


void EditorWindow::editGotoLine()
{
  GotoLine gl(this, NULL /*name*/, true /*modal*/,
              // these flags are the defaults for dialogs, as found in
              // qt/src/kernel/qwidget_x11.cpp line 285, except I
              // have remove WStyle_ContextHelp
              WStyle_Customize | WStyle_NormalBorder | WStyle_Title | WStyle_SysMenu);
              
  // I find it a bit bothersome to do this manually.. perhaps there
  // is a way to query the tab order, and always set focus to the
  // first widget in that order?
  gl.lineNumber->setFocus();
              
  //bool hasWT = gl.testWFlags(WStyle_ContextHelp);
  //cout << "hasWT: " << hasWT << endl;
  
  if (gl.exec()) {
    // user pressed Ok (or Enter)
    string s = string(gl.lineNumber->text());
    //cout << "text is \"" << s << "\"\n";
    
    if (s.length()) {
      int n = atoi(s);
      
      cout << "going to line " << n << endl;
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
  int cur = state->buffers.indexOf(theBuffer());
  xassert(cur >= 0);
  cur = (cur + 1) % state->buffers.count();     // cycle
  setBuffer(state->buffers.nth(cur));
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
    horizScroll->setSteps(1, editor->visCols());
  }

  if (vertScroll) {
    vertScroll->setValue(editor->firstVisibleLine);
    vertScroll->setRange(0, max(editor->buffer->numLines(),
                                editor->firstVisibleLine));
    vertScroll->setSteps(1, editor->visLines());
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
  while (bufferChoiceIds.isNotEmpty()) {
    int id = bufferChoiceIds.pop();
    windowMenu->removeItem(id);
  }

  // add new items for all of the open buffers; ids and
  // hotkeys have already been assigned by now
  FOREACH_OBJLIST_NC(BufferState, state->buffers, iter) {
    BufferState *b = iter.data();

    windowMenu->insertItem(
      QString(b->title),          // menu item text
      this,                       // receiver
      SLOT(windowBufferChoice()), // slot name
      b->hotkey,                  // accelerator
      b->windowMenuId);           // menu id

    bufferChoiceIds.push(b->windowMenuId);
  }
}

  
// the user has chosen a window menu item; this is called
// just before windowBufferChoice(), for menu items that
// select buffers; this also gets called when the user uses
// the accelerator keybinding
void EditorWindow::windowBufferChoiceActivated(int itemId)
{
  trace("menu") << "window buffer choice activated: " << itemId << endl;

  // search through the list of buffers to find the one
  // that this item id refers to; the buffers were marked
  // with their is when the buffer menu was built
  FOREACH_OBJLIST_NC(BufferState, state->buffers, iter) {
    BufferState *b = iter.data();
    if (b->windowMenuId == itemId) {
      recentMenuBuffer = b;
      return;
    }
  }

  // the id doesn't match any that I'm aware of; this happens
  // for window menu items that do *not* switch to some buffer
  recentMenuBuffer = NULL;
}

// respond to a choice of a buffer in the window item
void EditorWindow::windowBufferChoice()
{
  trace("menu") << "window buffer choice\n";

  if (recentMenuBuffer) {
    setBuffer(recentMenuBuffer);
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
  EditorWindow *ed = state->createNewWindow(theBuffer());
  ed->show();
}


// ---------------- GlobalState ----------------
GlobalState *GlobalState::state = NULL;

GlobalState::GlobalState(int argc, char **argv)
  : QApplication(argc, argv),
    pixmaps(),
    buffers(),
    nextMenuId(1),
    windows()
{
  if (state == NULL) {
    state = this;
  }

  // use my variant of the Windows style, if we're using Windows at all
  if (0==strcmp("QWindowsStyle", style().className())) {
    setStyle(new MyWindowsStyle);
  }

  EditorWindow *ed = createNewWindow(createNewFile());

  // this caption is immediately replaced with another one, at the
  // moment, since I call fileNewFile() right away
  //ed.setCaption("An Editor");

  // open all files specified on the command line
  for (int i=1; i < argc; i++) {
    ed->fileOpenFile(argv[i]);
  }

  // this gets the user's preferred initial geometry from
  // the -geometry command line, or xrdb database, etc.
  setMainWidget(ed);
  setMainWidget(NULL);    // but don't remember this

  // instead, to quit the application, close all of the
  // toplevel windows
  connect(this, SIGNAL(lastWindowClosed()),
          this, SLOT(quit()));

  ed->show();
}

EditorWindow *GlobalState::createNewWindow(BufferState *initBuffer)
{
  EditorWindow *ed =
    new EditorWindow(this, initBuffer,
                     NULL /*parent*/, "main editor window");
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
  // assign menu id
  b->windowMenuId = nextMenuId++;

  // assign hotkey
  for (int i=1; i<=10; i++) {
    int hotkey = i < 10? (i-1+Key_1) | ALT :
                 i ==10? Key_0 | ALT       :
                         0 /*no accelerator*/;
    if (hotkeyAvailable(hotkey)) {
      b->hotkey = hotkey;
      break;
    }
  }

  buffers.append(b);
  rebuildWindowMenus();
}

bool GlobalState::hotkeyAvailable(int key) const
{
  FOREACH_OBJLIST(BufferState, buffers, iter) {
    if (iter.data()->hotkey == key) {
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
  printSegfaultAddrs();

  GlobalState app(argc, argv);

  if (false) {
    static char const * const names[] = {
      "Foreground",
      "Button",
      "Light",
      "Midlight",
      "Dark",
      "Mid",
      "Text",
      "BrightText",
      "ButtonText",
      "Base",
      "Background",
      "Shadow",
      "Highlight",
      "HighlightedText",
    };

    QPalette pal(app.palette());
    QColorGroup cg(pal.active());
    for (int i=0; i < QColorGroup::NColorRoles; i++) {
      QColor c = cg.color((QColorGroup::ColorRole)i);
      printf("%-20s: 0x%X,0x%X,0x%X\n", names[i], c.red(), c.green(), c.blue());
    }
  }

  return app.exec();
}
