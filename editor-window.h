// editor-window.h
// Declares EditorWindow class.

#ifndef EDITOR_WINDOW_H
#define EDITOR_WINDOW_H

#include "named-td.h"                  // NamedTextDocument
#include "named-td-list.h"             // NamedTextDocumentListObserver

// smqtutil
#include "qtguiutil.h"                 // unhandledExceptionMsgbox

// smbase
#include "sm-override.h"               // OVERRIDE
#include "sm-noexcept.h"               // NOEXCEPT
#include "str.h"                       // string

// Qt
#include <QWidget>

class QLabel;
class QMenu;
class QMenuBar;
class QScrollBar;

class EditorWidget;                    // editor-widget.h
class GlobalState;                     // main.h
class SearchAndReplacePanel;           // sar-panel.h
class StatusDisplay;                   // status.h


// Top-level window containing an editor pane.
class EditorWindow : public QWidget,
                     public NamedTextDocumentListObserver {
  Q_OBJECT

public:      // static data
  static int s_objectCount;

public:      // types
  // Various kinds of file choosers available.
  enum FileChooseDialogKind {
    FCDK_NATIVE,             // OS native.
    FCDK_QT,                 // Qt version.
    FCDK_FILENAME_INPUT,     // My FilenameInputDialog.
  };

private:     // data
  // associated global state
  RCSerf<GlobalState> m_globalState;

  // GUI elements
  QMenuBar *m_menuBar;
  EditorWidget *m_editorWidget;
  SearchAndReplacePanel *m_sarPanel;
  QScrollBar *m_vertScroll, *m_horizScroll;
  StatusDisplay *m_statusArea;

  // The Window menu, whose contents changes with the set
  // of open file files.
  QMenu *m_windowMenu;

  // Actions for toggling view options.
  QAction *m_toggleVisibleWhitespaceAction;
  QAction *m_toggleVisibleSoftMarginAction;
  QAction *m_toggleHighlightTrailingWSAction;

  // Set of Actions in the Window menu that choose a file.
  // These need to be removed from the Window menu when we
  // rebuild it.  These are not exactly owner pointers, since
  // normally the Window menu owns the Actions, but when we
  // pull them out of the Window menu, we have to delete them.
  ArrayStack<QAction*> m_fileChoiceActions;

private:     // funcs
  void buildMenu();
  void writeTheFile();

  // 'doc' is something that was, in the recent past, the
  // 'currentDocument'.  We opened a dialog to get input from the user,
  // and that dialog has now been closed with the user indicating their
  // desire to proceed (not cancel).  But we need to confirm that the
  // current document is still 'doc'.  If not, show a message box and
  // return false.  The caller is then expected to cancel the operation.
  bool stillCurrentDocument(NamedTextDocument *doc);

  void setDocumentFile(NamedTextDocument *b);

  // Update the status displays to reflect a different file being edited.
  void updateForChangedFile();

  // Change 'file->highlighter' to agree with its file extension.
  // This does *not* send the attribute-change notification.
  void useDefaultHighlighter(NamedTextDocument *file);

  // Start the file chooser.  Return an empty string if it is canceled,
  // otherwise the chosen file name.
  string fileChooseDialog(string const &dir, bool saveAs,
    FileChooseDialogKind dialogKind);

  void rebuildWindowMenu();
  void complain(char const *msg);

  void printUnhandled(xBase const &x)
    { unhandledExceptionMsgbox(this, x); }

protected:   // funcs
  void closeEvent(QCloseEvent *event) override;

public:      // funcs
  EditorWindow(GlobalState *state, NamedTextDocument *initFile,
               QWidget *parent = NULL);
  ~EditorWindow();

  // open and begin editing a particular file
  void fileOpenFile(string const &fname);

  // File user is editing: returns editor->docFile.
  NamedTextDocument *currentDocument();

  // Reload the file for 'b' from disk.  If there is an error, show
  // an error message box and return false.
  bool reloadFile(NamedTextDocument *b);

  // Return true if either there are no unsaved changes or the user
  // responds to a GUI dialog and says it is ok to quit.
  bool canQuitApplication();

  // Return the number of files that have unsaved changes, and
  // populate 'msg' with a list of precisely which ones.
  int getUnsavedChanges(stringBuilder &msg);

  // Interactively ask the user if it is ok to discard changes,
  // returning true if they say it is.
  bool okToDiscardChanges(string const &descriptionOfChanges);

  // QObject methods.
  virtual bool eventFilter(QObject *watched, QEvent *event) NOEXCEPT OVERRIDE;

  // NamedTextDocumentListObserver methods.
  virtual void namedTextDocumentAdded(
    NamedTextDocumentList *documentList, NamedTextDocument *file) NOEXCEPT OVERRIDE;
  virtual void namedTextDocumentRemoved(
    NamedTextDocumentList *documentList, NamedTextDocument *file) NOEXCEPT OVERRIDE;
  virtual void namedTextDocumentAttributeChanged(
    NamedTextDocumentList *documentList, NamedTextDocument *file) NOEXCEPT OVERRIDE;
  virtual void namedTextDocumentListOrderChanged(
    NamedTextDocumentList *documentList) NOEXCEPT OVERRIDE;

public Q_SLOTS:
  void fileNewFile();
  void fileOpen();
  void fileOpenAtCursor();
  void fileOpenNativeDialog();
  void fileOpenQtDialog();
  void fileSave();
  void fileSaveAs();
  void fileClose();
  void fileReload();
  void fileReloadAll();
  void fileLaunchCommand();
  void fileRunMake();
  void fileKillProcess();
  void fileExit();

  void editUndo();
  void editRedo();
  void editCut();
  void editCopy();
  void editPaste();
  void editDelete();
  void editKillLine() NOEXCEPT;
  void editISearch();
  void editGotoLine();
  void editGrepSource() NOEXCEPT;
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
  void on_closeSARPanel();
  void on_openFilenameInputDialogSignal(QString const &filename, int line);
};


#endif // EDITOR_WINDOW_H
