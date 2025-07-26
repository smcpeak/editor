// editor-window.h
// Declares EditorWindow class.

#ifndef EDITOR_WINDOW_H
#define EDITOR_WINDOW_H

#include "editor-window-fwd.h"                   // fwds for this module

// editor
#include "eclf.h"                                // EditorCommandLineFunction
#include "editor-global-fwd.h"                   // EditorGlobal
#include "editor-settings-fwd.h"                 // EditorSettings
#include "editor-widget-frame-fwd.h"             // EditorWidgetFrame
#include "editor-widget-fwd.h"                   // EditorWidget
#include "host-file-and-line-opt.h"              // HostFileAndLineOpt
#include "lsp-manager-fwd.h"                     // LSPManager
#include "named-td.h"                            // NamedTextDocument
#include "named-td-list.h"                       // NamedTextDocumentListObserver
#include "vfs-connections-fwd.h"                 // VFS_Connections
#include "vfs-msg-fwd.h"                         // VFS_Message

// smqtutil
#include "smqtutil/qtguiutil.h"                  // unhandledExceptionMsgbox

// smbase
#include "smbase/exc.h"                          // smbase::XBase
#include "smbase/sm-noexcept.h"                  // NOEXCEPT
#include "smbase/sm-override.h"                  // OVERRIDE
#include "smbase/std-string-view-fwd.h"          // std::string_view
#include "smbase/str.h"                          // string

// Qt
#include <QWidget>

// libc++
#include <memory>                                // std::unique_ptr

class QLabel;
class QMenu;
class QMenuBar;

class SearchAndReplacePanel;                     // sar-panel.h
class StatusDisplay;                             // status.h


// Top-level window containing an editor pane.
//
// Logically, its parent is an EditorGlobal, and its primary children
// are one or more EditorWidgetFrames.  (TODO: Currently only one.)
class EditorWindow : public QWidget,
                     public NamedTextDocumentListObserver {
  Q_OBJECT

private:     // types
  // Things we can do with one file and LSP.
  enum LSPFileOperation {
    LSPFO_OPEN,              // Open the file.
    LSPFO_UPDATE,            // Update the file.
    LSPFO_UPDATE_IF_OPEN,    // Update if it is open.
    LSPFO_CLOSE,             // Close the file.
  };

public:      // static data
  static int s_objectCount;

private:     // data
  // Global editor state.
  RCSerf<EditorGlobal> m_editorGlobal;

  // Contained GUI elements, in layout order from top to bottom.
  QMenuBar *m_menuBar;
  EditorWidgetFrame *m_editorWidgetFrame;
  SearchAndReplacePanel *m_sarPanel;
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

  // Move the current document to the top of `m_documentList`.  This is
  // done just before switching to a new document.
  void makeCurrentDocumentTopmost();

  void setDocumentFile(NamedTextDocument *b);

  // Update the status displays to reflect a different file being edited.
  void updateForChangedFile();

  // Change 'file->highlighter' to agree with its file extension.
  // This does *not* send the attribute-change notification.
  void useDefaultHighlighter(NamedTextDocument *file);

  // Start the file chooser.  Return an empty string if it is canceled,
  // otherwise the chosen file name.
  string fileChooseDialog(HostName /*INOUT*/ &hostName,
    string const &dir, bool saveAs);

  // Issue 'request' synchronously, expecting to get 'REPLY_TYPE'.
  //
  // If there is an error, pop up an error box and return an empty
  // pointer.
  template <class REPLY_TYPE>
  std::unique_ptr<REPLY_TYPE> vfsQuerySynchronously(
    HostName const &hostName, std::unique_ptr<VFS_Message> request);

  // Return true if 'harn' exists, blocking during the check.
  bool checkFileExistenceSynchronously(HostAndResourceName const &harn);

  void printUnhandled(smbase::XBase const &x)
    { unhandledExceptionMsgbox(this, x); }

  // Find or start a document running `command` in `dir` on `hostName`,
  // and switch to it.
  void innerLaunchCommand(
    HostName const &hostName,
    QString dir,
    bool prefixStderrLines,
    QString command);

  // Do `operation` with the current file.
  void doLSPFileOperation(LSPFileOperation operation);

protected:   // funcs
  void closeEvent(QCloseEvent *event) OVERRIDE;

public:      // funcs
  EditorWindow(EditorGlobal *editorGlobal, NamedTextDocument *initFile,
               QWidget *parent = NULL);
  ~EditorWindow();

  // Get the global state we are a part of.
  EditorGlobal *editorGlobal() const { return m_editorGlobal; }

  // Current user settings object.
  EditorSettings const &editorSettings() const;

  // Global LSP manager.
  LSPManager &lspManager();

  // For now, the one editor widget in the one frame.
  EditorWidget *editorWidget() const;

  // The status area widget.
  StatusDisplay *statusArea() const { return m_statusArea; }

  // Get VFS query object.
  VFS_Connections *vfsConnections() const;

  // open and begin editing a particular file
  void fileOpenFile(HostAndResourceName const &harn);

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

  // Prompt for the command to run for Alt+A or Alt+R.  Return false if
  // the user cancles.  Otherwise, set `command` to the specified
  // command line.
  bool promptForCommandLine(
    QString /*OUT*/ &command,
    bool /*OUT*/ &prefixStderrLines,
    EditorCommandLineFunction whichFunction);

  // Pop up a message related to a problem.
  void complain(std::string_view msg);

  // Pop up a message for general information.
  void inform(std::string_view msg);

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
  void fileNewFile() NOEXCEPT;
  void fileOpen() NOEXCEPT;
  void fileOpenAtCursor() NOEXCEPT;
  void fileSave() NOEXCEPT;
  void fileSaveAs() NOEXCEPT;
  void fileClose() NOEXCEPT;
  void fileToggleReadOnly() NOEXCEPT;
  void fileReload() NOEXCEPT;
  void fileCheckForChanges() NOEXCEPT;
  void fileLaunchCommand() NOEXCEPT;
  void fileRunMake() NOEXCEPT;
  void fileKillProcess() NOEXCEPT;
  void fileManageConnections() NOEXCEPT;
  void fileLoadSettings() NOEXCEPT;
  void fileSaveSettings() NOEXCEPT;
  void fileExit() NOEXCEPT;

  void editUndo() NOEXCEPT;
  void editRedo() NOEXCEPT;
  void editCut() NOEXCEPT;
  void editCopy() NOEXCEPT;
  void editPaste() NOEXCEPT;
  void editDelete() NOEXCEPT;
  void editKillLine() NOEXCEPT;
  void editSelectEntireFile() NOEXCEPT;
  void editSearch() NOEXCEPT;
  void editReplace() NOEXCEPT;
  void editReplaceAndNext() NOEXCEPT;
  void editNextSearchHit() NOEXCEPT;
  void editPreviousSearchHit() NOEXCEPT;
  void editGotoLine() NOEXCEPT;
  void editGrepSource() NOEXCEPT;
  void editRigidIndent() NOEXCEPT;
  void editRigidUnindent() NOEXCEPT;
  void editJustifyParagraph() NOEXCEPT;
  void editApplyCommand() NOEXCEPT;
  void editInsertDateTime() NOEXCEPT;

  void viewToggleVisibleWhitespace() NOEXCEPT;
  void viewSetWhitespaceOpacity() NOEXCEPT;
  void viewToggleVisibleSoftMargin() NOEXCEPT;
  void viewSetSoftMarginColumn() NOEXCEPT;
  void viewToggleHighlightTrailingWS() NOEXCEPT;
  void viewSetHighlighting() NOEXCEPT;
  void viewSetApplicationFont() NOEXCEPT;
  void viewSetEditorFont() NOEXCEPT;
  void viewFontHelp() NOEXCEPT;

  void macroCreateMacro() NOEXCEPT;
  void macroRunDialog() NOEXCEPT;
  void macroRunMostRecent() NOEXCEPT;

  void lspStartServer() NOEXCEPT;
  void lspStopServer() NOEXCEPT;
  void lspCheckStatus() NOEXCEPT;
  void lspOpenFile() NOEXCEPT;
  void lspUpdateFile() NOEXCEPT;
  void lspCloseFile() NOEXCEPT;
  void lspReviewDiagnostics() NOEXCEPT;
  void lspShowDiagnosticAtCursor() NOEXCEPT;
  void lspInsertFakeDiagnostics() NOEXCEPT;
  void lspRemoveDiagnostics() NOEXCEPT;

  void windowOpenFilesList() NOEXCEPT;
  void windowPreviousFile() NOEXCEPT;
  void windowNewWindow() NOEXCEPT;
  void windowCloseWindow() NOEXCEPT;
  void windowOccupyLeft() NOEXCEPT;
  void windowOccupyRight() NOEXCEPT;

  void helpKeybindings() NOEXCEPT;
  void helpAbout() NOEXCEPT;
  void helpAboutQt() NOEXCEPT;
  void helpDebugDumpWindowObjectTree() NOEXCEPT;
  void helpDebugDumpApplicationObjectTree() NOEXCEPT;
  void helpDebugEditorScreenshot() NOEXCEPT;

  void editorViewChanged() NOEXCEPT;
  void slot_editorFontChanged() NOEXCEPT;
  void on_closeSARPanel() NOEXCEPT;
  void on_openFilenameInputDialogSignal(HostFileAndLineOpt hfl) NOEXCEPT;
};


#endif // EDITOR_WINDOW_H
