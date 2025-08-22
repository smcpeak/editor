// editor-global.h
// EditorGlobal class.

#ifndef EDITOR_EDITOR_GLOBAL_H
#define EDITOR_EDITOR_GLOBAL_H

#include "editor-global-fwd.h"                   // fwds for this module

// editor
#include "apply-command-dialog-fwd.h"            // ApplyCommandDialog
#include "builtin-font.h"                        // BuiltinFont
#include "command-runner-fwd.h"                  // CommandRunner
#include "eclf.h"                                // EditorCommandLineFunction, NUM_EDITOR_COMMAND_LINE_FUNCTIONS
#include "editor-command.ast.gen.fwd.h"          // EditorCommand
#include "editor-settings.h"                     // EditorSettings
#include "editor-window.h"                       // EditorWindow
#include "filename-input.h"                      // FilenameInputDialog
#include "lsp-manager.h"                         // LSPManager, LSPDocumentInfo
#include "named-td-list.h"                       // NamedTextDocumentList
#include "named-td.h"                            // NamedTextDocument
#include "open-files-dialog.h"                   // OpenFilesDialog
#include "pixmaps.h"                             // Pixmaps
#include "vfs-connections.h"                     // VFS_Connections

// smbase
#include "smbase/exclusive-write-file-fwd.h"     // smbase::ExclusiveWriteFile
#include "smbase/objlist.h"                      // ObjList
#include "smbase/refct-serf.h"                   // SerfRefCount
#include "smbase/sm-macros.h"                    // NULLABLE
#include "smbase/sm-override.h"                  // OVERRIDE
#include "smbase/std-optional-fwd.h"             // std::optional
#include "smbase/std-string-fwd.h"               // std::string

// Qt
#include <QApplication>

// libc++
#include <deque>                                 // std::deque
#include <list>                                  // std::list
#include <memory>                                // std::unique_ptr
#include <set>                                   // std::set

class ConnectionsDialog;                         // connections-dialog.h
class ProcessWatcher;                            // process-watcher.h

class QWidget;


// global state of the editor: files, windows, etc.
class EditorGlobal : public QApplication,
                     public NamedTextDocumentListObserver {
  Q_OBJECT

public:       // class data
  // Name of this application, used in the main window title as well as
  // some message boxes.
  static char const appName[];

  // Maximum size of `m_recentCommand`.
  static int const MAX_NUM_RECENT_COMMANDS;

public:       // data
  // pixmap set
  Pixmaps m_pixmaps;

  // List of open files.  Never empty (see NamedTextDocumentList).
  //
  // This list is ordered by how recently each document was switched
  // *away from* by some window, or was active when the OpenFilesDialog
  // was shown (regardless of whether the user switched away), with the
  // most recent at the top.  The documents currently *shown* in a
  // window are not necessarily near the top.
  //
  // TODO: Make this private so I can ensure the invariant relationship
  // with `m_lspManager`.
  //
  NamedTextDocumentList m_documentList;

private:     // data
  // Currently open editor windows.  Once the last one of these closes,
  // the app quits.
  ObjList<EditorWindow> m_editorWindows;

  // General editor-wide log file.  Can be null, depending on an envvar
  // setting.
  std::unique_ptr<smbase::ExclusiveWriteFile> m_editorLogFile;

  // Object to manage communication with the LSP server.
  //
  // Invariant: If `m_lspManager.isRunningNormally()`, then the set of
  // documents open in `m_lspManager` is the same as the set of
  // documents in `m_documentList` that are tracking changes.
  //
  LSPManager m_lspManager;

  // List of LSP protocol errors.  For now, these just accumulate.
  //
  // TODO: Send them to the log file.
  std::list<std::string> m_lspErrorMessages;

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

  // Partial history of recently executed commands.  The element at the
  // back is the most recent, such that the sequence is in chronological
  // order.
  std::deque<std::unique_ptr<EditorCommand>> m_recentCommands;

  // User settings.
  EditorSettings m_settings;

  // When true, we ignore requests to save settings.  This is meant for
  // use during automated testing.
  bool m_doNotSaveSettings;

public:      // data
  // Shared history for a dialog.
  FilenameInputDialog::History m_filenameInputDialogHistory;

  // True to record input to "events.out" for test case creation.
  bool m_recordInputEvents;

  // Name of an event file test to run, or empty for none.
  std::string m_eventFileTest;

private:     // methods
  void processCommandLineOptions(
    EditorWindow *editorWindow, int argc, char **argv);

  NamedTextDocument *getCommandOutputDocument(
    HostName const &hostName, QString dir, QString command);

  // Open the general editor log file.
  static std::unique_ptr<smbase::ExclusiveWriteFile>
    openEditorLogFile();

  // Given a directory in which application state files are generally
  // placed, such as `getXDGStateHome()`, and the name of a specific
  // file associated with the editor application, return the combined
  // name after normalizing path separators and ensuring the directory
  // containing the returned file name exists.
  static std::string getEditorStateFileName(
    std::string const &globalAppStateDir,
    char const *fname);

  // Connect signals from `m_lspManager` to `this`.
  void lspConnectSignals();

  // Disconnect signals and shut down the server.
  void lspDisconnectSignals();

private Q_SLOTS:
  // Called when focus changes anywhere in the app.
  void focusChangedHandler(QWidget *from, QWidget *to);

  // Find the watcher feeding data to 'fileDoc', if any.
  ProcessWatcher *findWatcherForDoc(NamedTextDocument *fileDoc);

  // Called when a watched process terminates.
  void on_processTerminated(ProcessWatcher *watcher);

  // Called when a VFS connection fails.
  void on_vfsConnectionFailed(HostName hostName, std::string reason) NOEXCEPT;

  // Called when `m_lspManager` has pending diagnostics.
  void on_lspHasPendingDiagnostics() NOEXCEPT;

  // Called when `m_lspManager` has an error message to deliver.
  void on_lspHasPendingErrorMessages() NOEXCEPT;

  // Called when `m_lspManager` may have changed its protocol state.
  void on_lspChangedProtocolState() NOEXCEPT;

public:       // funcs
  // intent is to make one of these in main()
  EditorGlobal(int argc, char **argv);
  ~EditorGlobal();

  // Assert invariants on all documents, etc.  Throw on failure.
  void selfCheck() const;

  // To run the app, use the 'exec()' method, inherited
  // from QApplication.

  VFS_Connections *vfsConnections() { return &m_vfsConnections; }

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

  // Calculate a minimal suffix of path components in 'filename' that
  // forms a title not shared by any open file.
  std::string uniqueTitleFor(DocumentName const &docName) const;

  // Open a new editor window.
  EditorWindow *createNewWindow(NamedTextDocument *initFile);

  // Called by the EditorWindow ctor to register its presence.
  void registerEditorWindow(EditorWindow *ew);

  // Called by the EditorWindow dtor to unregister.
  void unregisterEditorWindow(EditorWindow *ew);

  // Return the current number of editor windows.
  int numEditorWindows() const;

  // Add 'f' to the set of file documents.
  void trackNewDocumentFile(NamedTextDocument *f);

  // Remove 'f' from the set of file documents and deallocate it.  This
  // will tell all of the editor widgets to forget about it first so
  // they switch to some other file.
  //
  // This also closes the file with the LSP server if it is open there.
  //
  void deleteDocumentFile(NamedTextDocument *f);

  // Move `f` to the top of the list.
  void makeDocumentTopmost(NamedTextDocument *f);

  // Reload the contents of 'f'.  Return false if there was an error or
  // the reload was canceled.  If interaction is required, a modal
  // dialog may appear on top of 'parentWindow'.
  bool reloadDocumentFile(QWidget *parentWidget,
                          NamedTextDocument *f);

  // Show the open-files dialog and wait for the user to choose a file
  // or cancel.  If a choice is made, return it.
  //
  // Note that the dialog also allows the user to close files, which
  // uses the NamedTextDocumentListObserver notification system.
  NamedTextDocument *runOpenFilesDialog(QWidget *callerWindow);

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

  // Kill the process driving 'doc'.  If there is a problem doing that,
  // return a string explaining it; otherwise "".
  //
  // This does not wait to see if the process actually died; if/once it
  // does, the state of the document will transition to DPS_FINISHED.
  // Otherwise it can remain in DPS_RUNNING indefinitely for an
  // unkillable child.  Even in that state, the document can be safely
  // closed.
  std::string killCommand(NamedTextDocument *doc);

  // EditorGlobal has to monitor for closing a document that a process
  // is writing to, since that indicates to kill that process.
  virtual void namedTextDocumentRemoved(
    NamedTextDocumentList *documentList,
    NamedTextDocument *file) NOEXCEPT OVERRIDE;

  // Tell all windows that their view has changed.
  void broadcastEditorViewChanged();

  // Show the connections dialog.
  void showConnectionsDialog();

  // Get or create a read-only document called `title` with `contents`.
  NamedTextDocument *getOrCreateGeneratedDocument(
    std::string const &title,
    std::string const &contents);

  // Generate a document with `doc/keybindings.txt`.
  NamedTextDocument *getOrCreateKeybindingsDocument();

  // Hide any open modeless dialogs since we are about to quit.
  void hideModelessDialogs();

  BuiltinFont getEditorBuiltinFont() const
    { return m_editorBuiltinFont; }

  // Change the editor font and refresh all of the editor windows
  // accordingly.
  void setEditorBuiltinFont(BuiltinFont newFont);

  // Add `cmd` to the partial command history.
  void recordCommand(std::unique_ptr<EditorCommand> cmd);

  // Get up to `n` recent commands.
  EditorCommandVector getRecentCommands(int n) const;

  // Pop up a warning dialog box on top of `parent`.
  void warningBox(
    QWidget * NULLABLE parent,
    std::string const &str) const;

  // Get the path to the user settings file.
  static std::string getSettingsFileName();

  // Initial name to attempt to use for the general editor logs.  The
  // actual name might be different.
  static std::string getEditorLogFileInitialName();

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

  // If we have a log file, return its name.
  std::optional<std::string> getEditorLogFileNameOpt() const;

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

  // QCoreApplication methods.
  virtual bool notify(QObject *receiver, QEvent *event) OVERRIDE;

  // Get or create the dialog for `eclf`.
  ApplyCommandDialog &getApplyCommandDialog(
    EditorCommandLineFunction eclf);

  // --------------------------- LSP Global ----------------------------
  // Read-only access to the manager.
  LSPManager const *lspManagerC() { return &m_lspManager; }

  // Initial name for the path to the file that holds the stderr from
  // the LSP server process (clangd).
  static std::string lspGetStderrLogFileInitialName();

  // Start the LSP server.  Return an explanation string on failure.
  std::optional<std::string> lspStartServer();

  // Get the LSP protocol state.
  LSPProtocolState lspGetProtocolState() const;

  // True if the LSP connection is normal.
  bool lspIsRunningNormally() const;

  // Return a string that explains why `!lspIsRunningNormally()`.  If it
  // is in fact running normally, say so.
  std::string lspExplainAbnormality() const;

  // Generate a document with the LSP server's capabilities.
  NamedTextDocument *lspGetOrCreateServerCapabilitiesDocument();

  // Append an LSP error message.
  void lspAddErrorMessage(std::string &&msg);

  // Return a string summarizing the overall LSP state.  (This is a
  // temporary substitute for better error reporting.)
  std::string lspGetServerStatus() const;

  // Stop the LSP server, presumably as part of resetting it.  Return a
  // human-readable string describing what happened during the attempt.
  std::string lspStopServer();

  // -------------------------- LSP Per-file ---------------------------
  // True if `ntd` is open w.r.t. the LSP server.
  bool lspFileIsOpen(NamedTextDocument const *ntd) const;

  // If `doc` is "open" w.r.t. the LSP manager, return a pointer to its
  // details.  Otherwise return nullptr.
  RCSerf<LSPDocumentInfo const> lspGetDocInfo(
    NamedTextDocument const *doc) const;

  // Open `ntd` with the server.
  //
  // This can throw `XNumericConversion` if the version of `ntd` cannot
  // be expressed as an LSP version.
  //
  // Requires: !lspFileIsOpen(ntd)
  // Ensures:  lspFileIsOpen(ntd)
  void lspOpenFile(NamedTextDocument *ntd);

  // Update `ntd` with the server.
  //
  // This can throw `XNumericConversion` if the version of `ntd` cannot
  // be expressed as an LSP version.  It can also throw `XMessage` if
  // some unexpected version number issues arise.
  //
  // Requires: lspFileIsOpen(ntd)
  void lspUpdateFile(NamedTextDocument *ntd);

  // Close `ntd` if it is open.  This also discards any diagnostics it
  // may have and tells it to stop tracking changes.
  //
  // Ensures: !lspFileIsOpen(ntd)
  void lspCloseFile(NamedTextDocument *ntd);

  // --------------------------- LSP Queries ---------------------------
  // Cancel request `id` if it is outstanding.  Discard any reply that
  // has already been received.
  //
  // Requires: lspIsRunningNormally()
  void lspCancelRequestWithID(int id);

  // True if we have a reply for `id` waiting to be taken.
  //
  // Requires: lspIsRunningNormally()
  bool lspHasReplyForID(int id) const;

  // Take and return the reply for `id`.
  //
  // Requires: lspIsRunningNormally()
  // Requires: lspHasReplyForID(id)
  gdv::GDValue lspTakeReplyForID(int id);

  // Issue an `lsrk` request for information about the symbol at `coord`
  // in `ntd`.  Returns the request ID.
  //
  // Requires: lspFileIsOpen(ntd)
  int lspRequestRelatedLocation(
    LSPSymbolRequestKind lsrk,
    NamedTextDocument const *ntd,
    TextMCoord coord);

Q_SIGNALS:
  // Emitted when `m_editorBuiltinFont` changes.
  void signal_editorFontChanged();

  // Emitted when `lspGetProtocolState()` potentially changes.
  void signal_lspChangedProtocolState();

public Q_SLOTS:
  // Called when the search panel in some window has changed.  Broadcast
  // that fact to the others.
  void slot_broadcastSearchPanelChanged(SearchAndReplacePanel *panel) NOEXCEPT;
};


// Serialize all of the commands in `commands` as a sequence of GDVN
// lines.
std::string serializeECV(EditorCommandVector const &commands);

// Make a deep copy of `commands`.
EditorCommandVector cloneECV(EditorCommandVector const &commands);


#endif // EDITOR_EDITOR_GLOBAL_H
