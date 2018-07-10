// editor-window.h
// Declares EditorWindow class.

#ifndef EDITOR_WINDOW_H
#define EDITOR_WINDOW_H

#include "file-td.h"                   // FileTextDocument
#include "file-td-list.h"              // FileTextDocumentListObserver

// smqtutil
#include "qtguiutil.h"                 // unhandledExceptionMsgbox

// smbase
#include "str.h"                       // string

// Qt
#include <QWidget>

class QLabel;
class QMenu;
class QMenuBar;
class QScrollBar;

class EditorWidget;                    // editor-widget.h
class GlobalState;                     // main.h
class IncSearch;                       // incsearch.h
class StatusDisplay;                   // status.h


// Top-level window containing an editor pane.
class EditorWindow : public QWidget,
                     public FileTextDocumentListObserver {
  Q_OBJECT

public:      // static data
  static int s_objectCount;

public:      // data
  // associated global state
  GlobalState *globalState;                // (serf)

  // GUI elements
  QMenuBar *menuBar;
  EditorWidget *editorWidget;
  QScrollBar *vertScroll, *horizScroll;
  StatusDisplay *statusArea;

  // The Window menu, whose contents changes with the set
  // of open file files.
  QMenu *windowMenu;

  // Actions for toggling view options.
  QAction *toggleVisibleWhitespaceAction;
  QAction *toggleVisibleSoftMarginAction;
  QAction *toggleHighlightTrailingWSAction;

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
  void writeTheFile();
  void setDocumentFile(FileTextDocument *b);

  // Update the status displays to reflect a different file being edited.
  void updateForChangedFile();

  // Change 'file->highlighter' to agree with its file extension.
  // This does *not* send the attribute-change notification.
  void useDefaultHighlighter(FileTextDocument *file);

  // Start the file chooser.  Return an empty string if it is canceled,
  // otherwise the chosen file name.
  string fileChooseDialog(string const &initialName, bool saveAs);

  void rebuildWindowMenu();
  void complain(char const *msg);

  void printUnhandled(xBase const &x)
    { unhandledExceptionMsgbox(this, x); }

protected:   // funcs
  void closeEvent(QCloseEvent *event) override;

public:      // funcs
  EditorWindow(GlobalState *state, FileTextDocument *initFile,
               QWidget *parent = NULL);
  ~EditorWindow();

  // open and begin editing a particular file
  void fileOpenFile(string const &fname);

  // File user is editing: returns editor->docFile.
  FileTextDocument *currentDocument();

  // Reload the file for 'b' from disk.  If there is an error, show
  // an error message box and return false.
  bool reloadFile(FileTextDocument *b);

  // Return true if either there are no unsaved changes or the user
  // responds to a GUI dialog and says it is ok to quit.
  bool canQuitApplication();

  // Return the number of files that have unsaved changes, and
  // populate 'msg' with a list of precisely which ones.
  int getUnsavedChanges(stringBuilder &msg);

  // Interactively ask the user if it is ok to discard changes,
  // returning true if they say it is.
  bool okToDiscardChanges(string const &descriptionOfChanges);

  // FileTextDocumentListObserver methods.
  void fileTextDocumentAdded(
    FileTextDocumentList *documentList, FileTextDocument *file) noexcept override;
  void fileTextDocumentRemoved(
    FileTextDocumentList *documentList, FileTextDocument *file) noexcept override;
  void fileTextDocumentAttributeChanged(
    FileTextDocumentList *documentList, FileTextDocument *file) noexcept override;
  void fileTextDocumentListOrderChanged(
    FileTextDocumentList *documentList) noexcept override;

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
  void editApplyCommand();

  void viewToggleVisibleWhitespace();
  void viewSetWhitespaceOpacity();
  void viewToggleVisibleSoftMargin();
  void viewSetSoftMarginColumn();
  void viewToggleHighlightTrailingWS();
  void viewSetHighlighting();

  void windowOpenFilesList();
  void windowNewWindow();
  void windowCloseWindow();
  void windowOccupyLeft();
  void windowOccupyRight();
  void windowFileChoiceActivated(QAction *action);
  void windowFileChoice();

  void helpKeybindings();
  void helpAbout();
  void helpAboutQt();

  void editorViewChanged();
  void on_openFileSignal(QString const &filename);
};


#endif // EDITOR_WINDOW_H
