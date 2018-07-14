// main.h
// Editor module with main() function.

#ifndef MAIN_H
#define MAIN_H

// editor
#include "editor-window.h"             // EditorWindow
#include "file-td.h"                   // FileTextDocument
#include "file-td-list.h"              // FileTextDocumentList
#include "open-files-dialog.h"         // OpenFilesDialog
#include "pixmaps.h"                   // Pixmaps

// smbase
#include "objlist.h"                   // ObjList
#include "owner.h"                     // Owner
#include "refct-serf.h"                // SerfRefCount

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
    const QWidget *widget = NULL) const override;
};


// global state of the editor: files, windows, etc.
class GlobalState : public QApplication,
                    public FileTextDocumentListObserver {
  Q_OBJECT

public:       // data
  // pixmap set
  Pixmaps m_pixmaps;

  // List of open files.  Never empty (see FileTextDocumentList).
  FileTextDocumentList m_documentList;

  // currently open editor windows; nominally, once the
  // last one of these closes, the app quits
  ObjList<EditorWindow> m_windows;

private:     // data
  // Running child processes.
  ObjList<ProcessWatcher> m_processes;

  // Dialog that lets the user pick an open file.  We keep the dialog
  // around persistently so it remembers its size (but not location...)
  // across invocations.  It contains a pointer to m_documentList, so
  // must be destroyed before the list.
  Owner<OpenFilesDialog> m_openFilesDialog;

private:      // funcs
  bool hotkeyAvailable(int key) const;

  FileTextDocument *getNewCommandOutputDocument(
    QString dir, QString command);

private Q_SLOTS:
  // Called when focus changes anywhere in the app.
  void focusChangedHandler(QWidget *from, QWidget *to);

  // Find the watcher feeding data to 'fileDoc', if any.
  ProcessWatcher *findWatcherForDoc(FileTextDocument *fileDoc);

  // Called when a watched process terminates.
  void on_processTerminated(ProcessWatcher *watcher);

public:       // funcs
  // intent is to make one of these in main()
  GlobalState(int argc, char **argv);
  ~GlobalState();

  // To run the app, use the 'exec()' method, inherited
  // from QApplication.

  // Create an empty "untitled" file, add it to the set of documents,
  // and return it.
  FileTextDocument *createNewFile();

  // Return true if any file document has the given file name.
  bool hasFileWithName(string const &fname) const;

  // Return true if any file document has the given title.
  bool hasFileWithTitle(string const &title) const;

  // Calculate a minimal suffix of path components in 'filename' that
  // forms a title not shared by any open file.
  string uniqueTitleFor(string const &filename);

  // Open a new editor window.
  EditorWindow *createNewWindow(FileTextDocument *initFile);

  // Add 'f' to the set of file documents.
  void trackNewDocumentFile(FileTextDocument *f);

  // Remove 'f' from the set of file documents and deallocate it.
  // This will tell all of the editor widgets to forget about it
  // first so they switch to some other file.
  void deleteDocumentFile(FileTextDocument *f);

  // Show the open-files dialog and wait for the user to choose a file
  // or cancel.  If a choice is made, return it.
  //
  // Note that the dialog also allows the user to close files, which
  // uses the FileTextDocumentListObserver notification system.
  FileTextDocument *runOpenFilesDialog();

  // Start a new child process and return the document into which that
  // process' output is written.
  FileTextDocument *launchCommand(QString dir, QString command);

  // GlobalState has to monitor for closing a document that a process
  // is writing to, since that indicates to kill that process.
  virtual void fileTextDocumentRemoved(
    FileTextDocumentList *documentList,
    FileTextDocument *file) NOEXCEPT OVERRIDE;
};


#endif // MAIN_H
