// editor-global.h
// EditorGlobal class.

#ifndef EDITOR_EDITOR_GLOBAL_H
#define EDITOR_EDITOR_GLOBAL_H

#include "editor-global-fwd.h"                   // fwds for this module

// editor
#include "apply-command-dialog-fwd.h"            // ApplyCommandDialog [n]
#include "builtin-font.h"                        // BuiltinFont
#include "command-runner-fwd.h"                  // CommandRunner [n]
#include "connections-dialog-fwd.h"              // ConnectionsDialog [n]
#include "diagnostic-details-dialog-fwd.h"       // DiagnosticDetailsDialog [n]
#include "doc-type.h"                            // DocumentType
#include "eclf.h"                                // EditorCommandLineFunction, NUM_EDITOR_COMMAND_LINE_FUNCTIONS
#include "editor-command.ast.gen.fwd.h"          // EditorCommand [n]
#include "editor-navigation-options.h"           // EditorNavigationOptions
#include "editor-settings.h"                     // EditorSettings
#include "editor-window-fwd.h"                   // EditorWindow [n]
#include "editor-widget-fwd.h"                   // EditorWidget [n]
#include "filename-input.h"                      // FilenameInputDialog
#include "host-file-line-fwd.h"                  // HostFileLine [n]
#include "json-rpc-reply-fwd.h"                  // JSON_RPC_Reply [n]
#include "line-index-fwd.h"                      // LineIndex [n]
#include "lsp-client-fwd.h"                      // LSPDocumentInfo [n]
#include "lsp-client-manager-fwd.h"              // LSPClientManager [n]
#include "lsp-protocol-state.h"                  // LSPProtocolState
#include "lsp-servers-dialog-fwd.h"              // LSPServersDialog [n]
#include "lsp-symbol-request-kind.h"             // LSPSymbolRequestKind
#include "named-td-fwd.h"                        // NamedTextDocument [n]
#include "named-td-list.h"                       // NamedTextDocumentList
#include "open-files-dialog-fwd.h"               // OpenFilesDialog [n]
#include "pixmaps.h"                             // Pixmaps
#include "process-watcher-fwd.h"                 // ProcessWatcher [n]
#include "recent-items-list-iface.h"             // RecentItemsList
#include "sar-panel-fwd.h"                       // SearchAndReplacePanel [n]
#include "vfs-connections.h"                     // VFS_Connections

// smbase
#include "smbase/exclusive-write-file-fwd.h"     // smbase::ExclusiveWriteFile [n]
#include "smbase/objlist.h"                      // ObjList
#include "smbase/refct-serf.h"                   // SerfRefCount, RCSerf, RCSerfOpt, NNRCSerf
#include "smbase/sm-macros.h"                    // NULLABLE
#include "smbase/sm-override.h"                  // OVERRIDE
#include "smbase/std-optional-fwd.h"             // std::optional [n]
#include "smbase/std-string-fwd.h"               // std::string [n]

// smqtutil
#include "smqtutil/sync-wait-fwd.h"              // SynchronousWaiter [n]

// Qt
#include <QApplication>

// libc++
#include <deque>                                 // std::deque
#include <list>                                  // std::list
#include <memory>                                // std::unique_ptr
#include <optional>                              // std::optional
#include <string>                                // std::string
#include <vector>                                // std::vector

class QWidget;


// Global state of the editor: files, windows, etc.
class EditorGlobal : public QApplication,
                     public NamedTextDocumentListObserver {
  Q_OBJECT

public:      // class data
  // Name of this application, used in the main window title as well as
  // some message boxes.
  static char const appName[];

  // Maximum size of `m_recentCommand`.
  static int const MAX_NUM_RECENT_COMMANDS;

private:     // instance data
  // Pixmap set.  This appears unused at first, but creating it sets a
  // global pointer that allows other classes to use it.
  Pixmaps m_pixmaps;

  // List of open files.  Never empty (see NamedTextDocumentList).
  //
  // This list is ordered by how recently each document was switched
  // *away from* by some window, or was active when the OpenFilesDialog
  // was shown (regardless of whether the user switched away), with the
  // most recent at the top.  The documents currently *shown* in a
  // window are not necessarily near the top.
  //
  NamedTextDocumentList m_documentList;

  // Currently open editor windows.  Once the last one of these closes,
  // the app quits.
  //
  // Invariant: Not empty.
  ObjList<EditorWindow> m_editorWindows;

  // List of recently used editor widgets.  This is used to support
  // opening files in the "other" window.
  //
  // Invariant: Every widget is the widget for some window in
  // `m_editorWindows`.
  RecentItemsList<RCSerf<EditorWidget>> m_recentEditorWidgets;

  // General editor-wide log file.  Can be null, depending on an envvar
  // setting.
  std::unique_ptr<smbase::ExclusiveWriteFile> m_editorLogFile;

  // If true, then `m_lspClientManager` uses the "fake" server that is
  // just a Python script doing very simple textual analysis.  This
  // avoids using the real `clangd` during testing, both for dependency
  // and speed reasons.
  bool m_lspIsFakeServer;

  /* Object to manage communication with the LSP servers.

     Invariant: Never null, except during the first part of the ctor.
     Consequently, the protocol is to *not* check this for null before
     use; it is the responsibility of the ctor to initialize it before
     anything else can observe it being null.
  */
  std::unique_ptr<LSPClientManager> m_lspClientManager;

  // Counter for window numbering in the Qt object naming system.  This
  // starts at 1 and counts up each time a window is created.
  int m_windowCounter;

  // Built-in font to use in the editor widgets.
  BuiltinFont m_editorBuiltinFont;

  // Connections to local and remote file systems.
  VFS_Connections m_vfsConnections;

  // Running child processes.
  ObjList<ProcessWatcher> m_processes;

  // Dialog that lets the user pick an open file.  We keep the dialog
  // around persistently so it remembers its size (but not location...)
  // across invocations.  It contains a pointer to m_documentList, so
  // must be destroyed before the list.
  std::unique_ptr<OpenFilesDialog> m_openFilesDialog;

  // Each of the two dialogs used to prompt for a command line.
  std::unique_ptr<ApplyCommandDialog>
  m_applyCommandDialogs[NUM_EDITOR_COMMAND_LINE_FUNCTIONS];

  // Connections dialog, even when not shown.
  std::unique_ptr<ConnectionsDialog> m_connectionsDialog;

  // Dialog for showing LSP diagnostics.
  std::unique_ptr<DiagnosticDetailsDialog> m_diagnosticDetailsDialog;

  // Dialog for LSP servers.  This is null until the dialog is shown for
  // the first time, after which it persists until `EditorGlobal` is
  // destroyed.
  std::unique_ptr<LSPServersDialog> m_lspServersDialog;

  // Partial history of recently executed commands.  The element at the
  // back is the most recent, such that the sequence is in chronological
  // order.
  std::deque<std::unique_ptr<EditorCommand>> m_recentCommands;

  // User settings.
  EditorSettings m_settings;

  // When true, load user settings on startup, and save them in response
  // to various events.  This is set to false during automated testing,
  // both to provide a consistent starting point and to avoid clobbering
  // the real settings as a result of test behavior.
  bool m_useUserSettingsFile;

public:      // data
  // Shared history for a dialog.
  FilenameInputDialog::History m_filenameInputDialogHistory;

  // True to record input to "events.out" for test case creation.
  bool m_recordInputEvents;

  // Name of an event file test to run, or empty for none.
  //
  // This member and the next are public because `innerMain` accesses
  // them as it initiates the replay procedure.
  std::string m_eventTestFileName;

  // Sequence of commands to run for the test.
  std::vector<gdv::GDValue> m_eventTestCommands;

private:     // methods
  // Process the command line.  Return the sequence of files to be
  // opened initially.
  std::vector<std::string> processCommandLineOptions(
    int argc, char **argv);

  NamedTextDocument *getCommandOutputDocument(
    HostName const &hostName, QString dir, QString command);

  // Given a directory like `getXDGConfigHome()` that is meant to store
  // files of a certain kind for all applications, return the name of
  // a directory specifically for this editor application.
  //
  // The name has its path separators normalized to forward slashes and
  // does not end with a separator.  This function creates the directory
  // (and any needed parents) if needed.
  static std::string getEditorStateDirectory(
    std::string const &globalAppStateDir);

  // Open the general editor log file.
  static std::unique_ptr<smbase::ExclusiveWriteFile>
    openEditorLogFile();

private Q_SLOTS:
  // Called when a VFS connection fails.
  void on_vfsConnectionFailed(HostName hostName, std::string reason) NOEXCEPT;

  // Find the watcher feeding data to 'fileDoc', if any.
  ProcessWatcher *findWatcherForDoc(NamedTextDocument *fileDoc);

  // Called when a watched process terminates.
  void on_processTerminated(ProcessWatcher *watcher);

  // Called when focus changes anywhere in the app.
  void focusChangedHandler(QWidget *from, QWidget *to);

public:       // funcs
  // Construct application state from `main()`.
  EditorGlobal(int argc, char **argv);
  ~EditorGlobal();

  // Assert invariants on all documents, etc.  Throw on failure.
  void selfCheck() const;

  // To run the app, use the 'exec()' method, inherited
  // from QApplication.

  VFS_Connections *vfsConnections() { return &m_vfsConnections; }

  // --------------------- Documents being edited ----------------------
  // Read-only access to the document list.
  NamedTextDocumentList const *documentList() const;

  // Current number of documents.
  int numDocuments() const;

  // Get a document in `m_documentList` order.
  //
  // Requires: 0 <= index < numDocuments()
  NamedTextDocument const *getDocumentByIndexC(int index) const;
  NamedTextDocument       *getDocumentByIndex (int index);

  // Create an empty "untitled" file, add it to the set of documents,
  // and return it.
  NamedTextDocument *createNewFile(std::string const &dir);

  // Get the document with `docName` if there is one.
  NamedTextDocument const * NULLABLE
  getFileWithNameC(DocumentName const &docName) const;

  // Non-const version.
  NamedTextDocument * NULLABLE
  getFileWithName(DocumentName &docName);

  // Return true if any file document has the given name.
  bool hasFileWithName(DocumentName const &docName) const;

  // Return true if any file document has the given title.
  bool hasFileWithTitle(std::string const &title) const;

  // Add 'f' to the set of file documents.
  void trackNewDocumentFile(NamedTextDocument *f);

  // Remove 'f' from the set of file documents and deallocate it.  This
  // will tell all of the editor widgets to forget about it first so
  // they switch to some other file.
  //
  // This also closes the file with the LSP server if it is open there.
  //
  void deleteDocumentFile(NamedTextDocument *f);

  // True if `ntd` is among the documents we know about.
  bool hasDocumentFile(NamedTextDocument const *ntd) const;

  // Move `f` to the front of `m_documentList`, so it is regarded as
  // being the most recent when switching among them.
  void makeDocumentTopmost(NamedTextDocument *f);

  // Reload the contents of 'f'.  Return false if there was an error or
  // the reload was canceled.  If interaction is required, a modal
  // dialog may appear on top of 'parentWindow'.
  bool reloadDocumentFile(QWidget *parentWidget,
                          NamedTextDocument *f);

  // ------------------------ Special documents ------------------------
  // Find a document that is untitled and has no modifications if one
  // exists.
  NamedTextDocument * NULLABLE findUntitledUnmodifiedDocument();

  // Get or create a read-only document called `title` with `contents`.
  NamedTextDocument *getOrCreateGeneratedDocument(
    std::string const &title,
    std::string const &contents,
    DocumentType documentType);

  // Generate a document with `doc/keybindings.txt`.
  NamedTextDocument *getOrCreateKeybindingsDocument();

  // --------------------- Multi-document queries ----------------------
  // Calculate a minimal suffix of path components in 'filename' that
  // forms a title not shared by any open file.
  std::string uniqueTitleFor(DocumentName const &docName) const;

  // If there is some observer of the document list (an editor widget)
  // that already has a view for `ntd`, set `view` to that and return
  // true.  Otherwise return false.
  bool getInitialViewForFile(
    NamedTextDocument *ntd,
    NamedTextDocumentInitialView &view /*OUT*/);

  // Put into `dirs` the unique set of directories containing files
  // currently open, in order from most to least recently used.  Any
  // existing entries in `dirs` are *retained* ahead of added entries.
  void getUniqueDocumentDirectories(
    std::vector<HostAndResourceName> &dirs /*INOUT*/) const;

  // ------------------------- Editor windows --------------------------
  // Return the current number of editor windows.
  int numEditorWindows() const;

  // Open a new editor window.
  EditorWindow *createNewWindow(NamedTextDocument *initFile);

  // Called by the EditorWindow ctor to register its presence.
  void registerEditorWindow(EditorWindow *ew);

  // Called by the EditorWindow dtor to unregister.
  void unregisterEditorWindow(EditorWindow *ew);

  // -------------------------- Notification ---------------------------
  // Notify observers of the document list that an attribute of one of
  // the documents has changed.
  void notifyDocumentAttributeChanged(NamedTextDocument *ntd);

  // Tell all windows that their view has changed.
  void broadcastEditorViewChanged();

  // Add/remove a document list observer.
  void addDocumentListObserver(
    NamedTextDocumentListObserver *observer);
  void removeDocumentListObserver(
    NamedTextDocumentListObserver *observer);

  // --------------------- Running child processes ---------------------
  // Find or start a child process and return the document into which
  // that process' output is written.
  NamedTextDocument *launchCommand(
    HostName const &hostName,
    QString dir,
    bool prefixStderrLines,
    QString command,
    bool &stillRunning /*OUT*/);

  // Set the working directory and command line of 'cr' so that it will
  // run 'command' in 'dir' on 'hostName'.
  void configureCommandRunner(
    CommandRunner &cr,
    HostName const &hostName,
    QString dir,
    QString command);

  // EditorGlobal has to monitor for closing a document that a process
  // is writing to, since that indicates to kill that process.
  virtual void namedTextDocumentRemoved(
    NamedTextDocumentList const *documentList,
    NamedTextDocument *file) NOEXCEPT OVERRIDE;

  // Kill the process driving 'doc'.  If there is a problem doing that,
  // return a string explaining it; otherwise "".
  //
  // This does not wait to see if the process actually died; if/once it
  // does, the state of the document will transition to DPS_FINISHED.
  // Otherwise it can remain in DPS_RUNNING indefinitely for an
  // unkillable child.  Even in that state, the document can be safely
  // closed.
  std::string killCommand(NamedTextDocument *doc);

  // ------------------------------ Fonts ------------------------------
  BuiltinFont getEditorBuiltinFont() const
    { return m_editorBuiltinFont; }

  // Change the editor font and refresh all of the editor windows
  // accordingly.
  void setEditorBuiltinFont(BuiltinFont newFont);

  // ------------------------- Macro recorder --------------------------
  // Add `cmd` to the partial command history.
  void recordCommand(std::unique_ptr<EditorCommand> cmd);

  // Get up to `n` recent commands.
  EditorCommandVector getRecentCommands(int n) const;

  // ------------------------- Editor settings -------------------------
  // Get the path to the user settings file.
  static std::string getSettingsFileName();

  // Write settings to the user settings file and return true.  If there
  // is a problem, this pops up a dialog box above the given `parent`,
  // then return false.
  bool saveSettingsFile(QWidget * NULLABLE parent) NOEXCEPT;

  // If the settings file exists, load it and return true.  If there is
  // a problem, this pops up a dialog box above the given `parent`, then
  // returns false.
  bool loadSettingsFile(QWidget * NULLABLE parent) NOEXCEPT;

  // Load the settings, reporting problems by throwing.
  void loadSettingsFile_throwIfError();

  // Read-only settings access.  (Writing is done through methods that
  // also save the settings to a file.)
  EditorSettings const &getSettings() { return m_settings; }

  // Methods to change `m_settings` via methods that have names and
  // signatures that reflect those of `EditorSettings`.  Each call saves
  // the settings file afterward.  Any error is reported with a message
  // box that appears on top of `parent`.
  void settings_addMacro(
    QWidget * NULLABLE parent,
    std::string const &name,
    EditorCommandVector const &commands);
  bool settings_deleteMacro(
    QWidget * NULLABLE parent,
    std::string const &name);
  void settings_setMostRecentlyRunMacro(
    QWidget * NULLABLE parent,
    std::string const &name);
  std::string settings_getMostRecentlyRunMacro(
    QWidget * NULLABLE parent);
  bool settings_addHistoryCommand(
    QWidget * NULLABLE parent,
    EditorCommandLineFunction whichFunction,
    std::string const &cmd,
    bool useSubstitution,
    bool prefixStderrLines);
  bool settings_removeHistoryCommand(
    QWidget * NULLABLE parent,
    EditorCommandLineFunction whichFunction,
    std::string const &cmd);
  void settings_setLeftWindowPos(
    QWidget * NULLABLE parent,
    WindowPosition const &pos);
  void settings_setRightWindowPos(
    QWidget * NULLABLE parent,
    WindowPosition const &pos);
  void settings_setGrepsrcSearchesSubrepos(
    QWidget * NULLABLE parent,
    bool b);

  // ----------------------------- Dialogs -----------------------------
  // Show the open-files dialog and wait for the user to choose a file
  // or cancel.  If a choice is made, return it.
  //
  // Note that the dialog also allows the user to close files, which
  // uses the `NamedTextDocumentListObserver` notification system.
  NamedTextDocument *runOpenFilesDialog(QWidget *callerWindow);

  // Show the connections dialog.
  void showConnectionsDialog();

  // Get or create the dialog for `eclf`.
  ApplyCommandDialog &getApplyCommandDialog(
    EditorCommandLineFunction eclf);

  // Get this dialog, creating it if needed.
  RCSerf<DiagnosticDetailsDialog> getDiagnosticDetailsDialog();

  // Show the LSP servers dialog.
  void showLSPServersDialog();

  // Pop up a warning dialog box on top of `parent`.
  void warningBox(
    QWidget * NULLABLE parent,
    std::string const &str) const;

  // Hide any open modeless dialogs since we are about to quit, and if
  // they remain open, then the app will not terminate.
  void hideModelessDialogs();

  // ---------------------- Recent editor widgets ----------------------
  // Add or move `ew` to the front of "recent" list.
  void addRecentEditorWidget(EditorWidget *ew);

  // Remove `ew` from the "recent" list.
  void removeRecentEditorWidget(EditorWidget *ew);

  // Get the most recently used editor widget.  Never null.
  EditorWidget *getRecentEditorWidget();

  // Get the most recently used widget other than `ew`.  Return `ew` if
  // there is no alternative.
  EditorWidget *getOtherEditorWidget(EditorWidget *ew);

  // If `opts` is normal, return `ew`.  Otherwise return the "other"
  // widget, per `getOtherEditorWidget`.
  EditorWidget *selectEditorWidget(
    EditorWidget *ew, EditorNavigationOptions opts);

  // ----------------------------- Logging -----------------------------
  // Get directory where we will write log files.
  static std::string getLogFileDirectory();

  // Initial name to attempt to use for the general editor logs.  The
  // actual name might be different.
  static std::string getEditorLogFileInitialName();

  // If we have a log file, return its name.
  std::optional<std::string> getEditorLogFileNameOpt() const;

  // Append `msg` to the log file.  A newline will be added and the log
  // immediately flushed.  If there is no log file, this does nothing.
  void log(std::string const &msg);

  // Pop up a modal warning dialog with `dialogMessage` on `parent`, and
  // log `logMessage`.  The dialog also directs the user to the log file
  // for more information.
  void logAndWarn(
    QWidget * NULLABLE parent,
    std::string const &dialogMessage,
    std::string const &logMessage);

  // --------------------------- LSP Global ----------------------------
  // True if, whenever we start an LSP server process, we will use the
  // "fake" server meant for automated testing.
  bool lspIsFakeServer() const { return m_lspIsFakeServer; }

  // Client manager object, through which all LSP interaction takes
  // place.  Never null.
  NNRCSerf<LSPClientManager const> lspClientManagerC() const;
  NNRCSerf<LSPClientManager> lspClientManager();

  // Generate a document with the LSP server's capabilities for the
  // server relevant to `doc`.
  NamedTextDocument *lspGetOrCreateServerCapabilitiesDocument(
    NamedTextDocument const *doc);

  // Generate a document with all LSP server details.
  NamedTextDocument *lspGetOrCreateAllServerDetailsDocument();

  // -------------------- Qt infrastructure-related --------------------
  // QCoreApplication methods.
  virtual bool notify(QObject *receiver, QEvent *event) OVERRIDE;

Q_SIGNALS:
  // Emitted when `m_editorBuiltinFont` changes.
  void signal_editorFontChanged();

public Q_SLOTS:
  // Called when the search panel in some window has changed.  Broadcast
  // that fact to the others.
  void slot_broadcastSearchPanelChanged(SearchAndReplacePanel *panel) NOEXCEPT;

  // In the most recently used editor widget, open and show `fname`.
  // Show an error if there is a problem.
  void openLocalFileInEditor(std::string fname) NOEXCEPT;
};


// Serialize all of the commands in `commands` as a sequence of GDVN
// lines.
std::string serializeECV(EditorCommandVector const &commands);

// Make a deep copy of `commands`.
EditorCommandVector cloneECV(EditorCommandVector const &commands);


#endif // EDITOR_EDITOR_GLOBAL_H
