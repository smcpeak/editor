// main.cc
// code for main.cc, and application main() function

#include "main.h"            // EditorWindow
#include "buffer.h"          // Buffer
#include "editor.h"          // Editor
#include "exc.h"             // XOpen

#include <qapplication.h>    // QApplication
#include <qmenubar.h>        // QMenuBar
#include <qscrollbar.h>      // QScrollBar
#include <qlabel.h>          // QLabel
#include <qfiledialog.h>     // QFileDialog
#include <qmessagebox.h>     // QMessageBox


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
  // for now, fixed size
  resize(400,400);      
  setFixedSize(400,400);
  
  // menu
  {
    menuBar = new QMenuBar(this, "main menu bar");
    // geometry?

    QPopupMenu *file = new QPopupMenu(this);
    menuBar->insertItem("&File", file);
    file->insertItem("&New", this, SLOT(fileNew()));
    file->insertItem("&Open ...", this, SLOT(fileOpen()), Key_F3);
    file->insertItem("&Save", this, SLOT(fileSave()), Key_F2);
    file->insertItem("Save &as ...", this, SLOT(fileSaveAs()));
    file->insertSeparator();
    file->insertItem("E&xit", this, SLOT(close()));

    QPopupMenu *help = new QPopupMenu(this);
    menuBar->insertItem("&Help", help);
    help->insertItem("&About ...", this, SLOT(helpAbout()));
  }

  editor = new Editor(&theBuffer, this, "editor widget");
  editor->setGeometry(0,20, 380,340);
  editor->setFocus();

  vertScroll = new QScrollBar(QScrollBar::Vertical, this, "vertical scrollbar");
  vertScroll->setGeometry(380,20, 20,340);

  horizScroll = new QScrollBar(QScrollBar::Horizontal, this, "horizontal scrollbar");
  horizScroll->setGeometry(0,360, 380,20);
  
  position = new QLabel(this, "cursor position label");
  position->setGeometry(0,380, 100,20);
  position->setFrameStyle(QFrame::Panel | QFrame::Sunken);
  position->setLineWidth(2);

  filename = new QLabel(this, "filename label");
  filename->setGeometry(100,380, 100,20);
  filename->setFrameStyle(QFrame::Panel | QFrame::Sunken);
  filename->setLineWidth(2);

  // initialize things the way File|New does
  fileNew();
}


void EditorWindow::fileNew()
{           
  // delete all text
  theBuffer.clear();
  editor->cursor.set(0,0);

  // temporary: insert some dummy text into the buffer
  {
    Position start(&theBuffer);
    theBuffer.insertText(start, "hi there", 8);
  }

  position->setText("0:0");
  filename->setText("untitled.txt");
}


void EditorWindow::fileOpen()
{
  QString name = QFileDialog::getOpenFileName(QString::null, "Files (*)", this);
  if (name.isEmpty()) {
    return;
  }

  theBuffer.clear();
  editor->cursor.set(0,0);
                           
  try {
    theBuffer.readFile(name);
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
  
  theBuffer.filename = string(s);
  writeTheFile();
}


void EditorWindow::helpAbout()
{
  QMessageBox::about(this, "An Editor",
                     "This is a text editor, in case you didn't know.");
}


int main(int argc, char **argv)
{
  QApplication a(argc, argv);

  EditorWindow ed(NULL /*parent*/, "main editor window");
  ed.setCaption("An Editor");

  a.setMainWidget(&ed);
  ed.show();
  return a.exec();
}
