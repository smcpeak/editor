// main.cc
// code for main.cc, and application main() function

#include "main.h"            // EditorWindow
#include "buffer.h"          // Buffer
#include "editor.h"          // Editor
#include "exc.h"             // XOpen
#include "trace.h"           // TRACE_ARGS

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

#include "icon.xpm"          // icon_xpm[]


EditorWindow::EditorWindow(QWidget *parent=0, char const *name=0)
  : QWidget(parent, name),
    menuBar(NULL),
    editor(NULL),
    vertScroll(NULL),
    horizScroll(NULL),
    position(NULL),
    filename(NULL),
    theBuffer()
{
  // will build a layout tree to manage sizes of child widgets
  QVBoxLayout *mainAreaMgr = new QVBoxLayout(this);
  QVBox *mainArea = new QVBox(this, "main area");
  mainAreaMgr->addWidget(mainArea);    // get 'mainArea' to fill the dialog

  QGrid *editArea = new QGrid(2 /*cols*/, QGrid::Horizontal, mainArea, "edit area");

  editor = new Editor(&theBuffer, editArea, "editor widget");
  editor->setFocus();
  connect(editor, SIGNAL(viewChanged()), this, SLOT(editorViewChanged()));

  vertScroll = new QScrollBar(QScrollBar::Vertical, editArea, "vertical scrollbar");
  connect(vertScroll, SIGNAL( valueChanged(int) ), editor, SLOT( scrollToLine(int) ));

  horizScroll = new QScrollBar(QScrollBar::Horizontal, editArea, "horizontal scrollbar");
  connect(horizScroll, SIGNAL( valueChanged(int) ), editor, SLOT( scrollToCol(int) ));

  QHBox *statusArea = new QHBox(mainArea, "status area");
  statusArea->setFixedHeight(20);

  position = new QLabel(statusArea, "cursor position label");
  position->setFixedWidth(100);
  position->setFrameStyle(QFrame::Panel | QFrame::Sunken);
  position->setLineWidth(1);

  filename = new QLabel(statusArea, "filename label");
  filename->setFrameStyle(QFrame::Panel | QFrame::Sunken);
  filename->setLineWidth(1);

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
    edit->insertItem("Cu&t", editor, SLOT(editCut()), CTRL+Key_X);
    edit->insertItem("&Copy", editor, SLOT(editCopy()), CTRL+Key_C);
    edit->insertItem("&Paste", editor, SLOT(editPaste()), CTRL+Key_V);
    edit->insertItem("&Delete", editor, SLOT(editDelete()));

    QPopupMenu *help = new QPopupMenu(this);
    menuBar->insertItem("&Help", help);
    help->insertItem("&About ...", this, SLOT(helpAbout()));
  }

  setIcon(QPixmap((char const**)icon_xpm));

  setGeometry(200,200,      // initial location
              400,400);     // initial size

  // initialize things the way File|New does
  fileNew();

  // temporary: insert some dummy text into the buffer
  theBuffer.readFile("buffer.cc");
}


void EditorWindow::fileNew()
{
  // delete all text
  theBuffer.clear();
  editor->resetView();
  editor->updateView();
  editorViewChanged();
  editor->buffer->changed = false;

  setFileName("untitled.txt");
}


void EditorWindow::setFileName(char const *name)
{
  theBuffer.filename = name;
  filename->setText(name);
}


void EditorWindow::fileOpen()
{
  QString name = QFileDialog::getOpenFileName(QString::null, "Files (*)", this);
  if (name.isEmpty()) {
    return;
  }

  theBuffer.clear();
  editor->resetView();

  try {
    // this sets 'changed' to false
    theBuffer.readFile(name);
    editorViewChanged();
    setFileName(name);
  }
  catch (XOpen &x) {
    QMessageBox::information(this, "Can't open file", QString(x.why()));
  }
}


void EditorWindow::fileSave()
{
  if (theBuffer.filename.equals("untitled.txt")) {
    fileSaveAs();
    return;
  }

  writeTheFile();
}

void EditorWindow::writeTheFile()
{
  try {
    theBuffer.writeFile(theBuffer.filename);
    theBuffer.changed = false;
    editorViewChanged();
  }
  catch (XOpen &x) {
    QMessageBox::information(this, "Can't write file", QString(x.why()));
  }
}


void EditorWindow::fileSaveAs()
{
  QString s = QFileDialog::getSaveFileName(QString(theBuffer.filename),
                                           "Files (*)", this);
  if (s.isEmpty()) {
    return;
  }
  
  setFileName(s);
  writeTheFile();
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
  horizScroll->setRange(0, max(editor->buffer->maxLineLength(), 
                               editor->firstVisibleCol));
  horizScroll->setSteps(1, editor->visCols());
  horizScroll->setValue(editor->firstVisibleCol);

  vertScroll->setRange(0, max(editor->buffer->numLines(), 
                              editor->firstVisibleLine));
  vertScroll->setSteps(1, editor->visLines());
  vertScroll->setValue(editor->firstVisibleLine);

  // I want the user to interact with line/col with a 1:1 origin,
  // even though the Buffer interface uses 0:0
  position->setText(QString(stringc << (editor->cursorLine+1) << ":"
                                    << (editor->cursorCol+1)
                                    << (editor->buffer->changed? " *" : "")
                                    ));
}


int main(int argc, char **argv)
{
  TRACE_ARGS();
  QApplication a(argc, argv);

  EditorWindow ed(NULL /*parent*/, "main editor window");
  ed.setCaption("An Editor");

  a.setMainWidget(&ed);
  ed.show();
  return a.exec();
}
