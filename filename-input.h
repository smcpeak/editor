// filename-input.h
// FilenameInputDialog

#ifndef FILENAME_INPUT_H
#define FILENAME_INPUT_H

// editor
#include "event-replay.h"              // EventReplayQueryable
#include "modal-dialog.h"              // ModalDialog
#include "named-td-list.h"             // NamedTextDocumentList
#include "vfs-connections.h"           // VFS_Connections

// smbase
#include "refct-serf.h"                // RCSerf

class QLabel;
class QLineEdit;
class QPushButton;
class QTextEdit;


// Prompt for a file name.
//
// I'm trying to develop this into a fairly powerful keyboard-driven
// interface for selecting a file.  Currently it has:
//
//   * Immediate feedback on existence of what has been typed, including
//     existence, type of entry if so, whether it is already open, and
//     invalid directory.
//
//   * Feedback on possible completions.
//
//   * Tab completion of unambiguous fragment.
//
// Planned features:
//
//   * Optional extension filtering.
//
//   * Editing keybindings similar to the main editor window.
//
//   * Let me double-click on completion list items with mouse.
//
class FilenameInputDialog : public ModalDialog,
                            public EventReplayQueryable {
  Q_OBJECT

public:      // types
  // Data saved across invocations of the dialog.
  class History : public SerfRefCount {
  public:    // data
    // Onscreen size of the dialog, in pixels.
    QSize m_dialogSize;

  public:    // funcs
    History();
    ~History();
  };

private:     // data
  // Cross-invocation history.
  RCSerf<History> m_history;

  // ---- controls ----
  // Label above the filename.
  QLabel *m_filenameLabel;

  // Control with the current file name (full path) in it.  Owned by
  // 'this', but deallocated automatically by QObject infrastructure.
  QLineEdit *m_filenameEdit;

  // Text display of possible completions.
  QTextEdit *m_completionsEdit;

  // The "help" button.
  QPushButton *m_helpButton;

  // ---- vfs access state ----
  // Interface to issue VFS queries.
  VFS_Connections *m_vfsConnections;

  // If not zero, the ID of the current pending request.
  VFS_Connections::RequestID m_currentRequestID;

  // If not "", the directory we are currently querying.
  string m_currentRequestDir;

  // Name of the directory whose entries are cached in
  // 'm_cachedDirectoryEntries'.  Empty to indicate nothing is cached.
  string m_cachedDirectory;

  // Results of 'VFS_GetDirEntryRequest' on 'm_cachedDirectory'.
  std::vector<SMFileUtil::DirEntryInfo> m_cachedDirectoryEntries;

  // ---- other state ----
  // List of open documents so we can query it as the user types.
  //
  // This is only non-NULL while 'runDialog' is running.
  RCSerf<NamedTextDocumentList const> m_docList;

  // True if we should be prompting for a file to save.  False means
  // we are loading.  Initially false.
  bool m_saveAs;

private:     // funcs
  // Issue a query for information about the directory containing the
  // file name in 'm_filenameEdit'.  If we already have its information,
  // do nothing.  Otherwise issue a query, unless there is already a
  // pending query for that directory.
  void queryDirectoryIfNeeded();

  // If there is a current request, cancel it.
  void cancelCurrentRequestIfAny();

  // Set 'm_filenameLabel'.
  void setFilenameLabel();

  // Find the kind associated with 'name' in the directory cache.
  // Return FK_NONE if it is not found.
  SMFileUtil::FileKind lookupInCachedDirectoryEntries(string const &name);

  // Populate the directory entry cache.  This returns immediately if
  // the cache already has entries for 'dir'.
  void getEntries(string const &dir);

  // Get the set of possible completions of the filename in
  // 'm_filenameEdit' that would make the name of an existing file.
  //
  // Return false if we have not loaded the information required.
  bool getCompletions(ArrayStack<string> /*OUT*/ &completions);

  // Set 'm_completionsEdit'.
  void setCompletions();

  // Set the feedback displays based on the text in 'm_filenameEdit'
  // and the contents of the directory cache.
  void updateFeedback();

  // If possible, extend 'm_filenameEdit' to include the longest common
  // prefix of the candidates in the directory cache.
  void filenameCompletion();

public:      // funcs
  FilenameInputDialog(History *history,
                      VFS_Connections *vfsConnections,
                      QWidget *parent = NULL, Qt::WindowFlags f = Qt::WindowFlags());
  ~FilenameInputDialog();

  void setSaveAs(bool s) { m_saveAs = s; }

  // Show the dialog and return the name of the file the user has
  // chosen, or "" if canceled.
  QString runDialog(NamedTextDocumentList const *docList,
                    QString initialChoice);

  // QObject methods.
  virtual bool eventFilter(QObject *watched, QEvent *event) OVERRIDE;

  // EventReplayQueryable methods.
  virtual bool wantResizeEventsRecorded() OVERRIDE;

public Q_SLOTS:
  void on_textEdited(QString const &) NOEXCEPT;
  void on_help() NOEXCEPT;

  // VFS_Connections slots.
  void on_replyAvailable(VFS_Connections::RequestID requestID) NOEXCEPT;

  // QDialog slots.
  virtual void accept() NOEXCEPT OVERRIDE;
};


#endif // FILENAME_INPUT_H
