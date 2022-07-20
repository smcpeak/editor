// editor-window.h
// Declares EditorWindow class.

#ifndef EDITOR_WINDOW_H
#define EDITOR_WINDOW_H

#include "editor-window-fwd.h"         // fwds for this module

// editor
#include "named-td.h"                  // NamedTextDocument
#include "named-td-list.h"             // NamedTextDocumentListObserver
#include "vfs-msg-fwd.h"               // VFS_Message

// smqtutil
#include "qtguiutil.h"                 // unhandledExceptionMsgbox

// smbase
#include "sm-override.h"               // OVERRIDE
#include "sm-noexcept.h"               // NOEXCEPT
#include "str.h"                       // string

// Qt
#include <QWidget>

// libc++
#include <memory>                      // std::unique_ptr

class QLabel;
class QMenu;
class QMenuBar;
class QScrollBar;

class EditorWidget;                    // editor-widget.h
class EditorGlobalState;               // main.h
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
  RCSerf<EditorGlobalState> m_globalState;

  // GUI elements
  QMenuBar *m_menuBar;
  EditorWidget *m_editorWidget;
  SearchAndReplacePanel *m_sarPanel;
  QScrollBar *m_vertScroll, *m_horizScroll;
  StatusDisplay *m_statusArea;

  // Actions for toggle options.
  QAction *m_toggleReadOnlyAction;
  QAction *m_toggleVisibleWhitespaceAction;
  QAction *m_toggleVisibleSoftMarginAction;
  QAction *m_toggleHighlightTrailingWSAction;

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

  // Issue 'request' synchronously, expecting to get 'REPLY_TYPE'.
  //
  // If there is an error, pop up an error box and return an empty
  // pointer.
  template <class REPLY_TYPE>
  std::unique_ptr<REPLY_TYPE> vfsQuerySynchronously(
    std::unique_ptr<VFS_Message> request);

  // Read the contents of 'fname', waiting for the reply and blocking
  // user input during the wait.
  //
  // If this returns an empty pointer, then either the user canceled the
  // request, or an error was already reported; nothing further needs to
  // be done.  But if present, the reply object might itself have
  // 'm_success==false', which needs to be handled by the caller.
  std::unique_ptr<VFS_ReadFileReply> readFileSynchronously(
    string const &fname);

  // Get timestamp, etc., for 'fname'.
  std::unique_ptr<VFS_FileStatusReply> getFileStatusSynchronously(
    string const &fname);

  void complain(char const *msg);

  void printUnhandled(xBase const &x)
    { unhandledExceptionMsgbox(this, x); }

protected:   // funcs
  void closeEvent(QCloseEvent *event) OVERRIDE;

public:      // funcs
  EditorWindow(EditorGlobalState *state, NamedTextDocument *initFile,
               QWidget *parent = NULL);
  ~EditorWindow();

  // open and begin editing a particular file
  void fileOpenFile(string const &fname);

  // File user is editing: returns editor->docFile.
  NamedTextDocument *currentDocument();

  // Read the current document's on-disk contents and replace the
  // in-memory contents with what was loaded.  On error, show an error
  // message box.  Return true if we reloaded.
  bool reloadCurrentDocument();

  // Reload if the document has no unsaved changes and the on-disk
  // timestamp is different from what it was before.  Return true if we
  // reloaded.
  bool reloadCurrentDocumentIfChanged();

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

  // The search panel in one window has changed.
  void searchPanelChanged(SearchAndReplacePanel *panel);

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
  // TODO: All of these should be NOEXCEPT.

  void fileNewFile();
  void fileOpen();
  void fileOpenAtCursor();
  void fileOpenNativeDialog();
  void fileOpenQtDialog();
  void fileSave();
  void fileSaveAs();
  void fileClose();
  void fileToggleReadOnly() NOEXCEPT;
  void fileReload() NOEXCEPT;
  void fileReloadAll();
  void fileLaunchCommand();
  void fileRunMake();
  void fileKillProcess();
  void fileExit();

  void editUndo() NOEXCEPT;
  void editRedo() NOEXCEPT;
  void editCut() NOEXCEPT;
  void editCopy() NOEXCEPT;
  void editPaste() NOEXCEPT;
  void editDelete() NOEXCEPT;
  void editKillLine() NOEXCEPT;
  void editSearch() NOEXCEPT;
  void editReplace() NOEXCEPT;
  void editReplaceAndNext() NOEXCEPT;
  void editNextSearchHit() NOEXCEPT;
  void editPreviousSearchHit() NOEXCEPT;
  void editGotoLine();
  void editGrepSource() NOEXCEPT;
  void editRigidIndent() NOEXCEPT;
  void editRigidUnindent() NOEXCEPT;
  void editJustifyParagraph() NOEXCEPT;
  void editApplyCommand();
  void editInsertDateTime() NOEXCEPT;

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

  void helpKeybindings();
  void helpAbout();
  void helpAboutQt();
  void helpDebugDumpWindowObjectTree() NOEXCEPT;
  void helpDebugDumpApplicationObjectTree() NOEXCEPT;
  void helpDebugEditorScreenshot() NOEXCEPT;

  void editorViewChanged();
  void on_closeSARPanel();
  void on_openFilenameInputDialogSignal(QString const &filename, int line);
};


#endif // EDITOR_WINDOW_H
