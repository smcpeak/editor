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

#include <string.h>          // strrchr

#include <qapplication.h>    // QApplication
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

#include "icon.xpm"          // icon_xpm[]


EditorWindow::EditorWindow(QWidget *parent=0, char const *name=0)
  : QWidget(parent, name),
    menuBar(NULL),
    editor(NULL),
    vertScroll(NULL),
    horizScroll(NULL),
    position(NULL),
    filename(NULL),
    buffers()
{                                                             
  // will build a layout tree to manage sizes of child widgets
  QVBoxLayout *mainAreaMgr = new QVBoxLayout(this);
  QVBox *mainArea = new QVBox(this, "main area");
  mainAreaMgr->addWidget(mainArea);    // get 'mainArea' to fill the dialog

  QGrid *editArea = new QGrid(2 /*cols*/, QGrid::Horizontal, mainArea, "edit area");

  editor = new Editor(NULL /*temporary*/, editArea, "editor widget");
  editor->setFocus();
  connect(editor, SIGNAL(viewChanged()), this, SLOT(editorViewChanged()));

  vertScroll = new QScrollBar(QScrollBar::Vertical, editArea, "vertical scrollbar");
  connect(vertScroll, SIGNAL( valueChanged(int) ), editor, SLOT( scrollToLine(int) ));

  // disabling horiz scroll for now..
  //horizScroll = new QScrollBar(QScrollBar::Horizontal, editArea, "horizontal scrollbar");
  //connect(horizScroll, SIGNAL( valueChanged(int) ), editor, SLOT( scrollToCol(int) ));

  QHBox *statusArea = new QHBox(mainArea, "status area");
  statusArea->setFixedHeight(20);

  //QStatusBar *statusArea = new QStatusBar(mainArea, "status area");
  //statusArea->setFixedHeight(20);

  position = new QLabel(statusArea, "cursor position label");
  position->setFixedWidth(100);
  //position->setBackgroundColor(QColor(0x60, 0x00, 0x80));   // purple
  //position->setFrameStyle(QFrame::Panel | QFrame::Sunken);
  //position->setLineWidth(1);

  filename = new QLabel(statusArea, "filename label");
  //filename->setFrameStyle(QFrame::Panel | QFrame::Sunken);
  //filename->setLineWidth(1);

  // corner resize widget
  QSizeGrip *corner = new QSizeGrip(statusArea, "corner grip");
  corner->setFixedSize(20,20);

  // menu
  {
    menuBar = new QMenuBar(mainArea, "main menu bar");
    menuBar->setFrameStyle(QFrame::Panel | QFrame::Raised);
    menuBar->setLineWidth(1);

    QPopupMenu *file = new QPopupMenu(this);
    menuBar->insertItem("&File", file);
    file->insertItem("&New", this, SLOT(fileNew()));
    file->insertItem("&Open ...", this, SLOT(fileOpen()), Key_F3);
    file->insertItem("&Save", this, SLOT(fileSave()), Key_F2);
    file->insertItem("Save &as ...", this, SLOT(fileSaveAs()));
    file->insertSeparator();
    file->insertItem("E&xit", this, SLOT(close()));

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

    QPopupMenu *window = new QPopupMenu(this);
    menuBar->insertItem("&Window", window);
    window->insertItem("Occupy Left", this, SLOT(windowOccupyLeft()), CTRL+ALT+Key_Left);
    window->insertItem("Occupy Right", this, SLOT(windowOccupyRight()), CTRL+ALT+Key_Right);
    window->insertItem("Cycle buffer", this, SLOT(windowCycleBuffer()), Key_F6);  // for now

    QPopupMenu *help = new QPopupMenu(this);
    menuBar->insertItem("&Help", help);
    help->insertItem("&About ...", this, SLOT(helpAbout()));
  }

  setIcon(QPixmap((char const**)icon_xpm));

  setGeometry(200,200,      // initial location
              400,400);     // initial size

  // initialize things the way File|New does; this does the initial
  // call to editor->setBuffer() to remove the NULL value
  fileNew();

  // temporary: insert some dummy text into the buffer
  //theBuffer.readFile("buffer.cc");
  
  // i-search; use filename as status display
  isearch = new IncSearch(filename);
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


void EditorWindow::fileNew()
{
  BufferState *b = new BufferState();
  b->filename = "untitled.txt";    // TODO: find a unique variant of this name
  buffers.append(b);

  setBuffer(b);
}

void EditorWindow::setBuffer(BufferState *b)
{
  editor->setBuffer(b);

  //editor->resetView();
  editor->updateView();
  editorViewChanged();

  setFileName(theBuffer()->filename);
}


void EditorWindow::setFileName(char const *name)
{
  theBuffer()->filename = name;
  filename->setText(name);
  setCaption(QString(stringc << "An Editor: " << sm_basename(name)));
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

  // now that we've opened the file, set the editor widget to edit it
  buffers.append(b);
  setBuffer(b);
}


void EditorWindow::fileSave()
{
  if (theBuffer()->filename.equals("untitled.txt")) {
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
  
  setFileName(s);
  writeTheFile();
}


void EditorWindow::editISearch()
{
  isearch->attach(editor);
}


void EditorWindow::windowOccupyLeft() 
{
  setGeometry(83, 55, 565, 867);
}


void EditorWindow::windowOccupyRight()
{
  setGeometry(493, 55, 781, 867);
}


void EditorWindow::windowCycleBuffer()
{
  int cur = buffers.indexOf(theBuffer());
  xassert(cur >= 0);
  cur = (cur + 1) % buffers.count();     // cycle
  setBuffer(buffers.nth(cur));
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
  position->setText(QString(stringc << " " << (editor->cursorLine()+1) << ":"
                                    << (editor->cursorCol()+1)
                                    << (editor->buffer->unsavedChanges()? " *" : "")
                                    ));
}


int main(int argc, char **argv)
{
  TRACE_ARGS();
  QApplication a(argc, argv);

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

    QPalette pal(a.palette());
    QColorGroup cg(pal.active());
    for (int i=0; i < QColorGroup::NColorRoles; i++) {
      QColor c = cg.color((QColorGroup::ColorRole)i);
      printf("%-20s: 0x%X,0x%X,0x%X\n", names[i], c.red(), c.green(), c.blue());
    }
  }

  // use my variant of the Windows style, if we're using Windows at all
  if (0==strcmp("QWindowsStyle", a.style().className())) {
    a.setStyle(new MyWindowsStyle);
  }

  EditorWindow ed(NULL /*parent*/, "main editor window");
  
  // this caption is immediately replaced with another one, at the moment,
  // since I call fileNew() right away
  ed.setCaption("An Editor");
  
  if (argc >= 2) {
    ed.fileOpenFile(argv[1]);
  }

  a.setMainWidget(&ed);
  ed.show();
  return a.exec();
}
