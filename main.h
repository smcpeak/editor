// main.h
// editor main GUI window

#ifndef MAIN_H
#define MAIN_H

#include <qwidget.h>         // QWidget
#include <qwindowsstyle.h>   // QWindowsStyle
#include <qapplication.h>    // QApplication

#include "bufferstate.h"     // BufferState
#include "objlist.h"         // ObjList
#include "pixmaps.h"         // Pixmaps

class QMenuBar;
class QScrollBar;
class QLabel;

class Editor;                // editor.h
class IncSearch;             // incsearch.h
class StatusDisplay;         // status.h

class GlobalState;           // this file


// toplevel window containing an editor pane
class EditorWindow : public QWidget {
  Q_OBJECT

public:      // data
  // associated global state
  GlobalState *state;                // (serf)

  // GUI elements
  QMenuBar *menuBar;
  Editor *editor;
  QScrollBar *vertScroll, *horizScroll;
  StatusDisplay *statusArea;
  //QLabel *position, *mode, *filename;

  // information to maintain the list of buffers in
  // the window menu
  QPopupMenu *windowMenu;
  ArrayStack<int> bufferChoiceIds;   // ids of menu items that select buffers
  BufferState *recentMenuBuffer;

  // incremental search system
  IncSearch *isearch;                // (owner)

private:     // funcs
  void setFileName(char const *name);
  void writeTheFile();
  void setBuffer(BufferState *b);
  void rebuildWindowMenu();
  void complain(char const *msg);

  // the above functions are called by GlobalState
  // in places; that class could be viewed as an
  // extension of this one, across multiple windows
  friend class GlobalState;

public:      // funcs
  EditorWindow(GlobalState *state, BufferState *initBuffer,
               QWidget *parent=0, char const *name=0);
  ~EditorWindow();

  // open and begin editing a particular file
  void fileOpenFile(char const *fname);

  // buffer user is editing: returns editor->buffer
  BufferState *theBuffer();

public slots:
  void fileNewFile();
  void fileOpen();
  void fileSave();
  void fileSaveAs();
  void fileClose();

  void editISearch();

  void windowNewWindow();
  void windowOccupyLeft();
  void windowOccupyRight();
  void windowCycleBuffer();
  void windowBufferChoiceActivated(int itemId);
  void windowBufferChoice();

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


// global state of the editor: buffers, windows, etc.
class GlobalState : public QApplication {
public:       // data
  // the singleton global state object
  static GlobalState *state;

  // pixmap set
  Pixmaps pixmaps;

  // list of open files (buffers)
  ObjList<BufferState> buffers;

  // next menu id to use for a window menu item
  int nextMenuId;

  // currently open editor windows; nominally, once the
  // last one of these closes, the app quits
  ObjList<EditorWindow> windows;

private:      // funcs
  bool hotkeyAvailable(int key) const;

public:       // funcs
  // intent is to make one of these in main()
  GlobalState(int argc, char **argv);
  ~GlobalState();
  
  // to run the app, use the 'exec()' method, inherited
  // from QApplication
  
  BufferState *createNewFile();
  EditorWindow *createNewWindow(BufferState *initBuffer);
  void trackNewBuffer(BufferState *b);
  void rebuildWindowMenus();
  void deleteBuffer(BufferState *b);
};




#endif // MAIN_H
