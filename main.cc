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
#include <qvbox.h>           // QVBox
#include <qlayout.h>         // QVBoxLayout
#include <qhbox.h>           // QHBox
#include <qgrid.h>           // QGrid


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

  // menu
  {
    menuBar = new QMenuBar(mainArea, "main menu bar");

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

  QGrid *editArea = new QGrid(2 /*cols*/, QGrid::Horizontal, mainArea, "edit area");

  editor = new Editor(&theBuffer, editArea, "editor widget");
  editor->setFocus();

  vertScroll = new QScrollBar(QScrollBar::Vertical, editArea, "vertical scrollbar");

  horizScroll = new QScrollBar(QScrollBar::Horizontal, editArea, "horizontal scrollbar");

  QHBox *statusArea = new QHBox(mainArea, "status area");
  statusArea->setFixedHeight(20);

  position = new QLabel(statusArea, "cursor position label");
  position->setFixedWidth(100);
  position->setFrameStyle(QFrame::Panel | QFrame::Sunken);
  position->setLineWidth(1);

  filename = new QLabel(statusArea, "filename label");
  filename->setFrameStyle(QFrame::Panel | QFrame::Sunken);
  filename->setLineWidth(1);

  // initialize things the way File|New does
  fileNew();

  // initial size
  resize(400,400);

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
