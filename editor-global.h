// editor-global.h
// EditorGlobalState class.

#ifndef EDITOR_EDITOR_GLOBAL_H
#define EDITOR_EDITOR_GLOBAL_H

// editor
#include "editor-window.h"             // EditorWindow
#include "filename-input.h"            // FilenameInputDialog
#include "named-td-list.h"             // NamedTextDocumentList
#include "named-td.h"                  // NamedTextDocument
#include "open-files-dialog.h"         // OpenFilesDialog
#include "pixmaps.h"                   // Pixmaps
#include "vfs-connections.h"           // VFS_Connections

// smbase
#include "objlist.h"                   // ObjList
#include "owner.h"                     // Owner
#include "refct-serf.h"                // SerfRefCount
#include "sm-override.h"               // OVERRIDE

// Qt
#include <QApplication>
#include <QProxyStyle>

class ProcessWatcher;                  // process-watcher.h


// Define my look and feel overrides.
class EditorProxyStyle : public QProxyStyle {
public:
  int pixelMetric(
    PixelMetric metric,
    const QStyleOption *option = NULL,
    const QWidget *widget = NULL) const OVERRIDE;
};


// global state of the editor: files, windows, etc.
class EditorGlobalState : public QApplication,
                          public NamedTextDocumentListObserver {
  Q_OBJECT

public:       // data
  // pixmap set
  Pixmaps m_pixmaps;

  // List of open files.  Never empty (see NamedTextDocumentList).
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

private:     // data
  // Connections to local and remote file systems.
  VFS_Connections m_vfsConnections;

  // Running child processes.
  ObjList<ProcessWatcher> m_processes;

  // Dialog that lets the user pick an open file.  We keep the dialog
  // around persistently so it remembers its size (but not location...)
  // across invocations.  It contains a pointer to m_documentList, so
  // must be destroyed before the list.
  Owner<OpenFilesDialog> m_openFilesDialog;

private:      // funcs
  NamedTextDocument *getNewCommandOutputDocument(
    QString dir, QString command);

private Q_SLOTS:
  // Called when focus changes anywhere in the app.
  void focusChangedHandler(QWidget *from, QWidget *to);

  // Find the watcher feeding data to 'fileDoc', if any.
  ProcessWatcher *findWatcherForDoc(NamedTextDocument *fileDoc);

  // Called when a watched process terminates.
  void on_processTerminated(ProcessWatcher *watcher);

  // Called when a VFS connection is lost.
  void on_vfsConnectionLost(string reason) NOEXCEPT;

public:       // funcs
  // intent is to make one of these in main()
  EditorGlobalState(int argc, char **argv);
  ~EditorGlobalState();

  // To run the app, use the 'exec()' method, inherited
  // from QApplication.

  VFS_Connections *vfsConnections() { return &m_vfsConnections; }

  // Create an empty "untitled" file, add it to the set of documents,
  // and return it.
  NamedTextDocument *createNewFile(string const &dir);

  // Return true if any file document has the given file name.
  bool hasFileWithName(string const &fname) const;

  // Return true if any file document has the given title.
  bool hasFileWithTitle(string const &title) const;

  // Calculate a minimal suffix of path components in 'filename' that
  // forms a title not shared by any open file.
  string uniqueTitleFor(string const &filename);

  // Open a new editor window.
  EditorWindow *createNewWindow(NamedTextDocument *initFile);

  // Add 'f' to the set of file documents.
  void trackNewDocumentFile(NamedTextDocument *f);

  // Remove 'f' from the set of file documents and deallocate it.
  // This will tell all of the editor widgets to forget about it
  // first so they switch to some other file.
  void deleteDocumentFile(NamedTextDocument *f);

  // Show the open-files dialog and wait for the user to choose a file
  // or cancel.  If a choice is made, return it.
  //
  // Note that the dialog also allows the user to close files, which
  // uses the NamedTextDocumentListObserver notification system.
  NamedTextDocument *runOpenFilesDialog(QWidget *callerWindow);

  // Start a new child process and return the document into which that
  // process' output is written.
  NamedTextDocument *launchCommand(QString dir,
    bool prefixStderrLines, QString command);

  // Kill the process driving 'doc'.  If there is a problem doing that,
  // return a string explaining it; otherwise "".
  //
  // This does not wait to see if the process actually died; if/once it
  // does, the state of the document will transition to DPS_FINISHED.
  // Otherwise it can remain in DPS_RUNNING indefinitely for an
  // unkillable child.  Even in that state, the document can be safely
  // closed.
  string killCommand(NamedTextDocument *doc);

  // EditorGlobalState has to monitor for closing a document that a process
  // is writing to, since that indicates to kill that process.
  virtual void namedTextDocumentRemoved(
    NamedTextDocumentList *documentList,
    NamedTextDocument *file) NOEXCEPT OVERRIDE;

  // QCoreApplication methods.
  virtual bool notify(QObject *receiver, QEvent *event) OVERRIDE;

public Q_SLOTS:
  // Called when the search panel in some window has changed.  Broadcast
  // that fact to the others.
  void slot_broadcastSearchPanelChanged(SearchAndReplacePanel *panel) NOEXCEPT;
};


#endif // EDITOR_EDITOR_GLOBAL_H
