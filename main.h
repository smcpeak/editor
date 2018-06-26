// main.h
// editor main GUI window

#ifndef MAIN_H
#define MAIN_H

#include <qwidget.h>                   // QWidget
#include <qapplication.h>              // QApplication

#include <QProxyStyle>

#include "objlist.h"                   // ObjList
#include "pixmaps.h"                   // Pixmaps
#include "text-document-file.h"        // TextDocumentFile

class QLabel;
class QMenu;
class QMenuBar;
class QScrollBar;

class EditorWidget;                    // editor-widget.h
class IncSearch;                       // incsearch.h
class StatusDisplay;                   // status.h

class GlobalState;                     // this file


// toplevel window containing an editor pane
class EditorWindow : public QWidget {
  Q_OBJECT

public:      // static data
  static int objectCount;

public:      // data
  // associated global state
  GlobalState *globalState;                // (serf)

  // GUI elements
  QMenuBar *menuBar;
  EditorWidget *editor;
  QScrollBar *vertScroll, *horizScroll;
  StatusDisplay *statusArea;

  // The Window menu, whose contents changes with the set
  // of open file files.
  QMenu *windowMenu;

  // The action for toggling visible whitespace.
  QAction *toggleVisibleWhitespaceAction;

  // Action for toggling the soft margin display.
  QAction *toggleVisibleSoftMarginAction;

  // Set of Actions in the Window menu that choose a file.
  // These need to be removed from the Window menu when we
  // rebuild it.  These are not exactly owner pointers, since
  // normally the Window menu owns the Actions, but when we
  // pull them out of the Window menu, we have to delete them.
  ArrayStack<QAction*> fileChoiceActions;

  // incremental search system
  IncSearch *isearch;                // (owner)

private:     // funcs
  void buildMenu();
  void setFileName(rostring name, rostring hotkey);
  void writeTheFile();
  void setDocumentFile(TextDocumentFile *b);

  // Update the status displays to reflect a different file being edited.
  void updateForChangedFile();

  void forgetAboutFile(TextDocumentFile *file);
  void rebuildWindowMenu();
  void complain(char const *msg);

  // the above functions are called by GlobalState
  // in places; that class could be viewed as an
  // extension of this one, across multiple windows
  friend class GlobalState;

protected:   // funcs
  void closeEvent(QCloseEvent *event) override;

public:      // funcs
  EditorWindow(GlobalState *state, TextDocumentFile *initFile,
               QWidget *parent = NULL);
  ~EditorWindow();

  // open and begin editing a particular file
  void fileOpenFile(char const *fname);

  // File user is editing: returns editor->docFile.
  TextDocumentFile *theDocFile();

  // Reload the file for 'b' from disk.  If there is an error, show
  // an error message box and return false.
  bool reloadFile(TextDocumentFile *b);

  // Return true if either there are no unsaved changes or the user
  // responds to a GUI dialog and says it is ok to quit.
  bool canQuitApplication();

  // Return the number of files that have unsaved changes, and
  // populate 'msg' with a list of precisely which ones.
  int getUnsavedChanges(stringBuilder &msg);

  // Interactively ask the user if it is ok to discard changes,
  // returning true if they say it is.
  bool okToDiscardChanges(string const &descriptionOfChanges);

public slots:
  void fileNewFile();
  void fileOpen();
  void fileSave();
  void fileSaveAs();
  void fileClose();
  void fileReload();
  void fileReloadAll();
  void fileExit();

  void editISearch();
  void editGotoLine();

  void viewToggleVisibleWhitespace();
  void viewSetWhitespaceOpacity();
  void viewToggleVisibleSoftMargin();
  void viewSetSoftMarginColumn();

  void windowNewWindow();
  void windowOccupyLeft();
  void windowOccupyRight();
  void windowCycleFile();
  void windowFileChoiceActivated(QAction *action);
  void windowFileChoice();

  void helpKeybindings();
  void helpAbout();
  void helpAboutQt();

  void editorViewChanged();
};


// Define my look and feel overrides.
class EditorProxyStyle : public QProxyStyle {
public:
  int pixelMetric(
    PixelMetric metric,
    const QStyleOption *option = NULL,
    const QWidget *widget = NULL) const override;
};


// global state of the editor: files, windows, etc.
class GlobalState : public QApplication {
public:       // data
  // the singleton global state object
  static GlobalState *global_globalState;

  // pixmap set
  Pixmaps pixmaps;

  // List of open files.  There is always at least one; if the last
  // file is closed, we open an "untitled" file.
  ObjList<TextDocumentFile> documentFiles;

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

  TextDocumentFile *createNewFile();
  EditorWindow *createNewWindow(TextDocumentFile *initFile);
  void trackNewDocumentFile(TextDocumentFile *f);
  void rebuildWindowMenus();
  void deleteDocumentFile(TextDocumentFile *f);
};


#endif // MAIN_H
