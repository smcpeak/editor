// main.h
// editor main GUI window

#ifndef MAIN_H
#define MAIN_H

#include <qwidget.h>         // QWidget
#include <qwindowsstyle.h>   // QWindowsStyle

#include "bufferstate.h"     // BufferState

class QMenuBar;
class QScrollBar;
class QLabel;

class Editor;                // editor.h
class IncSearch;             // incsearch.h


// toplevel window containing an editor pane
class EditorWindow : public QWidget {
  Q_OBJECT

public:      // data
  // GUI elements
  QMenuBar *menuBar;
  Editor *editor;
  QScrollBar *vertScroll, *horizScroll;
  QLabel *position, *filename;

  // right now, one buffer
  BufferState theBuffer;

  // incremental search system
  IncSearch *isearch;          // (owner)

private:     // funcs
  void setFileName(char const *name);
  void writeTheFile();

public:      // funcs
  EditorWindow(QWidget *parent=0, char const *name=0);
  ~EditorWindow();

  // open and begin editing a particular file
  void fileOpenFile(char const *fname);

public slots:
  void fileNew();
  void fileOpen();
  void fileSave();
  void fileSaveAs();
  
  void editISearch();
  
  void windowOccupyLeft();
  void windowOccupyRight();
  
  void helpAbout();
  
  void editorViewChanged();
};


// define style as variant of Windows style
class MyWindowsStyle : public QWindowsStyle {
  Q_OBJECT

public:
  // disable "weird jump back" scrolling behavior
  int maximumSliderDragDistance() const { return -1; }
};


#endif // MAIN_H
