// editor-global.h
// EditorGlobal class.

#ifndef EDITOR_EDITOR_GLOBAL_H
#define EDITOR_EDITOR_GLOBAL_H

#include "editor-global-fwd.h"                   // fwds for this module

// editor
#include "builtin-font.h"                        // BuiltinFont
#include "command-runner-fwd.h"                  // CommandRunner
#include "editor-command.ast.gen.fwd.h"          // EditorCommand
#include "editor-settings.h"                     // EditorSettings
#include "editor-window.h"                       // EditorWindow
#include "filename-input.h"                      // FilenameInputDialog
#include "lsp-manager.h"                         // LSPManager
#include "named-td-list.h"                       // NamedTextDocumentList
#include "named-td.h"                            // NamedTextDocument
#include "open-files-dialog.h"                   // OpenFilesDialog
#include "pixmaps.h"                             // Pixmaps
#include "vfs-connections.h"                     // VFS_Connections

// smbase
#include "smbase/objlist.h"                      // ObjList
#include "smbase/owner.h"                        // Owner
#include "smbase/refct-serf.h"                   // SerfRefCount
#include "smbase/sm-macros.h"                    // NULLABLE
#include "smbase/sm-override.h"                  // OVERRIDE
#include "smbase/std-string-fwd.h"               // std::string

// Qt
#include <QApplication>
#include <QProxyStyle>

// libc++
#include <deque>                                 // std::deque
#include <list>                                  // std::list
#include <set>                                   // std::set

class ConnectionsDialog;                         // connections-dialog.h
class ProcessWatcher;                            // process-watcher.h

class QWidget;


// Define my look and feel overrides.
class EditorProxyStyle : public QProxyStyle {
public:
  virtual int pixelMetric(
    PixelMetric metric,
    const QStyleOption *option = NULL,
    const QWidget *widget = NULL) const override;

  virtual int styleHint(
    StyleHint hint,
    QStyleOption const *option,
    QWidget const *widget,
    QStyleHintReturn *returnData) const override;
};


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
  NamedTextDocumentList m_documentList;

  // currently open editor windows; nominally, once the
  // last one of these closes, the app quits
  ObjList<EditorWindow> m_windows;

  // True to record input to "events.out" for test case creation.
  bool m_recordInputEvents;

  // Name of an event file test to run, or empty for none.
  string m_eventFileTest;

  // Shared history for a dialog.
  FilenameInputDialog::History m_filenameInputDialogHistory;

  // Object to manage communication with the LSP server.
  LSPManager m_lspManager;

  // List of LSP protocol errors.  For now, these just accumulate.
  std::list<std::string> m_lspErrorMessages;

private:     // data
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
  Owner<OpenFilesDialog> m_openFilesDialog;

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

private:      // funcs
  void processCommandLineOptions(
    EditorWindow *ed, int argc, char **argv);

  NamedTextDocument *getCommandOutputDocument(
    HostName const &hostName, QString dir, QString command);

private Q_SLOTS:
  // Called when focus changes anywhere in the app.
  void focusChangedHandler(QWidget *from, QWidget *to);

  // Find the watcher feeding data to 'fileDoc', if any.
  ProcessWatcher *findWatcherForDoc(NamedTextDocument *fileDoc);

  // Called when a watched process terminates.
  void on_processTerminated(ProcessWatcher *watcher);

  // Called when a VFS connection fails.
  void on_connectionFailed(HostName hostName, string reason) NOEXCEPT;

  // Called when `m_lspManager` has pending diagnostics.
  void on_lspHasPendingDiagnostics() NOEXCEPT;

  // Called when `m_lspManager` has an error message to deliver.
  void on_lspHasPendingErrorMessages() NOEXCEPT;

public:       // funcs
  // intent is to make one of these in main()
  EditorGlobal(int argc, char **argv);
  ~EditorGlobal();

  // To run the app, use the 'exec()' method, inherited
  // from QApplication.

  VFS_Connections *vfsConnections() { return &m_vfsConnections; }

  LSPManager &lspManager() { return m_lspManager; }

  // Create an empty "untitled" file, add it to the set of documents,
  // and return it.
  NamedTextDocument *createNewFile(string const &dir);

  // Get the document with `docName` if there is one.
  NamedTextDocument const * NULLABLE
  getFileWithNameC(DocumentName const &docName) const;

  // Non-const version.
  NamedTextDocument * NULLABLE
  getFileWithName(DocumentName &docName);

  // Return true if any file document has the given name.
  bool hasFileWithName(DocumentName const &docName) const;

  // Return true if any file document has the given title.
  bool hasFileWithTitle(string const &title) const;

  // Calculate a minimal suffix of path components in 'filename' that
  // forms a title not shared by any open file.
  string uniqueTitleFor(DocumentName const &docName) const;

  // Open a new editor window.
  EditorWindow *createNewWindow(NamedTextDocument *initFile);

  // Add 'f' to the set of file documents.
  void trackNewDocumentFile(NamedTextDocument *f);

  // Remove 'f' from the set of file documents and deallocate it.
  // This will tell all of the editor widgets to forget about it
  // first so they switch to some other file.
  void deleteDocumentFile(NamedTextDocument *f);

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
  string killCommand(NamedTextDocument *doc);

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

  // Generate a document with the LSP server's capabilities.
  NamedTextDocument *getOrCreateLSPServerCapabilitiesDocument();

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

  // Get the path to the file that holds the stderr from the LSP server
  // process (clangd).
  static std::string getLSPStderrLogFileName();

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

  // Append an LSP error message.
  void addLSPErrorMessage(std::string &&msg);

  // Return a string summarizing the overall LSP state.  (This is a
  // temporary substitute for better error reporting.)
  std::string getLSPStatus() const;

  // QCoreApplication methods.
  virtual bool notify(QObject *receiver, QEvent *event) OVERRIDE;

Q_SIGNALS:
  // Emitted when 'm_editorBuiltinFont' changes.
  void signal_editorFontChanged();

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
