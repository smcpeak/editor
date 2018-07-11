// filename-input.h
// FilenameInputDialog

#ifndef FILENAME_INPUT_H
#define FILENAME_INPUT_H

// editor
#include "file-td-list.h"              // FileTextDocumentList
#include "modal-dialog.h"              // ModalDialog

// smbase
#include "refct-serf.h"                // RCSerf

class QLabel;
class QLineEdit;
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
// Planned features:
//
//   * Tab completion of unambiguous fragment.
//
//   * Optional extension filtering.
//
//   * Editing keybindings similar to the main editor window.
//
class FilenameInputDialog : public ModalDialog {
  Q_OBJECT

private:     // data
  // Label above the filename.
  QLabel *m_filenameLabel;

  // Control with the current file name (full path) in it.  Owned by
  // 'this', but deallocated automatically by QObject infrastructure.
  QLineEdit *m_filenameEdit;

  // Text display of possible completions.
  QTextEdit *m_completionsEdit;

  // Name of the directory whose entries are cached in
  // 'm_cachedDirectoryEntries'.  Empty to indicate nothing is cached.
  string m_cachedDirectory;

  // Results of 'getDirectoryEntries' on 'm_cachedDirectory'.
  ArrayStack<string> m_cachedDirectoryEntries;

  // List of open documents so we can query it as the user types.
  //
  // This is only non-NULL while 'runDialog' is running.
  RCSerf<FileTextDocumentList const> m_docList;

private:     // funcs
  // Set 'm_filenameLabel'.
  void setFilenameLabel();

  // Populate the directory entry cache.  This returns immediately if
  // the cache already has entries for 'dir'.
  void getEntries(string const &dir);

  // Set 'm_completionsEdit'.
  void setCompletions();

  // Set the feedback displays based on the text in 'm_filenameEdit'.
  void updateFeedback();

public:      // funcs
  FilenameInputDialog(QWidget *parent = NULL, Qt::WindowFlags f = Qt::WindowFlags());
  ~FilenameInputDialog();

  // Show the dialog and return the name of the file the user has
  // chosen, or "" if canceled.
  QString runDialog(FileTextDocumentList const *docList,
                    QString initialChoice);

public slots:
  void on_textEdited(QString const &);
};


#endif // FILENAME_INPUT_H
