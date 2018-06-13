// main.h
// editor main GUI window

#ifndef MAIN_H
#define MAIN_H

#include <qwidget.h>         // QWidget
// TODO: replacement?   #include <qwindowsstyle.h>   // QWindowsStyle
#include <qapplication.h>    // QApplication

#include "bufferstate.h"     // BufferState
#include "objlist.h"         // ObjList
#include "pixmaps.h"         // Pixmaps

class QLabel;
class QMenu;
class QMenuBar;
class QScrollBar;

class Editor;                // editor.h
class IncSearch;             // incsearch.h
class StatusDisplay;         // status.h

class GlobalState;           // this file


// toplevel window containing an editor pane
class EditorWindow : public QWidget {
  Q_OBJECT

public:      // data
  // associated global state
  GlobalState *globalState;                // (serf)

  // GUI elements
  QMenuBar *menuBar;
  Editor *editor;
  QScrollBar *vertScroll, *horizScroll;
  StatusDisplay *statusArea;
  //QLabel *position, *mode, *filename;

  // The Window menu, whose contents changes with the set
  // of open file buffers.
  QMenu *windowMenu;

  // Set of Actions in the Window menu that choose a buffer.
  // These need to be removed from the Window menu when we
  // rebuild it.  These are not exactly owner pointers, since
  // normally the Window menu owns the Actions, but when we
  // pull them out of the Window menu, we have to delete them.
  ArrayStack<QAction*> bufferChoiceActions;

  BufferState *recentMenuBuffer;

  // incremental search system
  IncSearch *isearch;                // (owner)

private:     // funcs
  void setFileName(rostring name, rostring hotkey);
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
               QWidget *parent = NULL);
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
  void editGotoLine();

  void windowNewWindow();
  void windowOccupyLeft();
  void windowOccupyRight();
  void windowCycleBuffer();
  void windowBufferChoiceActivated(QAction *action);
  void windowBufferChoice();

  void helpAbout();
  void helpAboutQt();

  void editorViewChanged();
};


// This test matches /usr/lib/qt-3.3.3/include/qwindowsstyle.h.
// I do not know how portable it is across versions of Qt.
// TODO: replacement?
//#if !defined(QT_NO_STYLE_WINDOWS) || defined(QT_PLUGIN)
//  #define HAS_QWINDOWSSTYLE
//#endif


#ifdef HAS_QWINDOWSSTYLE
// define style as variant of Windows style
class MyWindowsStyle : public QWindowsStyle {
  // I'm hoping that omitting Q_OBJECT will make 'moc' leave
  // this class alone...
  //Q_OBJECT

public:
  // disable "weird jump back" scrolling behavior
  int maximumSliderDragDistance() const { return -1; }
};
#endif // HAS_QWINDOWSSTYLE


// global state of the editor: buffers, windows, etc.
class GlobalState : public QApplication {
public:       // data
  // the singleton global state object
  static GlobalState *state;

  // pixmap set
  Pixmaps pixmaps;

  // list of open files (buffers)
  ObjList<BufferState> buffers;

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
