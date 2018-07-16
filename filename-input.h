// filename-input.h
// FilenameInputDialog

#ifndef FILENAME_INPUT_H
#define FILENAME_INPUT_H

// editor
#include "named-td-list.h"             // NamedTextDocumentList
#include "modal-dialog.h"              // ModalDialog

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
//   * Show names of directories differently in completion list.
//
//   * Let me double-click on completion list items with mouse.
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

  // The "help" button.
  QPushButton *m_helpButton;

  // Name of the directory whose entries are cached in
  // 'm_cachedDirectoryEntries'.  Empty to indicate nothing is cached.
  string m_cachedDirectory;

  // Results of 'getDirectoryEntries' on 'm_cachedDirectory'.
  ArrayStack<string> m_cachedDirectoryEntries;

  // List of open documents so we can query it as the user types.
  //
  // This is only non-NULL while 'runDialog' is running.
  RCSerf<NamedTextDocumentList const> m_docList;

  // True if we should be prompting for a file to save.  False means
  // we are loading.  Initially false.
  bool m_saveAs;

private:     // funcs
  // Set 'm_filenameLabel'.
  void setFilenameLabel();

  // Populate the directory entry cache.  This returns immediately if
  // the cache already has entries for 'dir'.
  void getEntries(string const &dir);

  // Get the set of possible completions of the filename in
  // 'm_filenameEdit' that would make the name of an existing file.
  void getCompletions(ArrayStack<string> /*OUT*/ &completions);

  // Set 'm_completionsEdit'.
  void setCompletions();

  // Set the feedback displays based on the text in 'm_filenameEdit'.
  void updateFeedback();

  // If possible, extend 'm_filenameEdit' to include the longest common
  // prefix of the candidates in the directory.
  void filenameCompletion();

public:      // funcs
  FilenameInputDialog(QWidget *parent = NULL, Qt::WindowFlags f = Qt::WindowFlags());
  ~FilenameInputDialog();

  void setSaveAs(bool s) { m_saveAs = s; }

  // Show the dialog and return the name of the file the user has
  // chosen, or "" if canceled.
  QString runDialog(NamedTextDocumentList const *docList,
                    QString initialChoice);

  // QObject methods.
  virtual bool eventFilter(QObject *watched, QEvent *event) OVERRIDE;

public Q_SLOTS:
  void on_textEdited(QString const &);
  void on_help();

  // QDialog slots.
  virtual void accept() OVERRIDE;
};


#endif // FILENAME_INPUT_H
