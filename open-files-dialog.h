// open-files-dialog.h
// OpenFilesDialog class.

#ifndef OPEN_FILES_DIALOG_H
#define OPEN_FILES_DIALOG_H

#include "editor-global-fwd.h"                   // EditorGlobal
#include "event-replay.h"                        // EventReplayQueryable
#include "modal-dialog.h"                        // ModalDialog
#include "named-td-list.h"                       // NamedTextDocumentList

#include "smqtutil/sm-table-widget-fwd.h"        // SMTableWidget

#include "smbase/refct-serf.h"                   // RCSerf
#include "smbase/sm-iostream.h"                  // ostream
#include "smbase/sm-noexcept.h"                  // NOEXCEPT

#include <vector>                                // std::vector

class QLineEdit;
class QModelIndex;
class QPushButton;


// This is mainly for debugging.  I put it here because this is, for
// the moment, the only module that deals with QModelIndex.
ostream& operator<< (ostream &os, QModelIndex const &index);


// Dialog to show and manipulate the open files in a document list.
//
// This is sort of like an "editor" for NamedTextDocumentList.
class OpenFilesDialog : public ModalDialog,
                        public EventReplayQueryable {
  Q_OBJECT

private:     // instance data
  // Global editor state, which grants access to the list we are
  // showing/editing.
  EditorGlobal *m_editorGlobal;

  // Sequence of open documents that satisfy 'm_filterLineEdit'.
  //
  // The elements are pointers to the documents owned by EditorGlobal,
  // and the order is the same as in the EditorGlobal, modulo filtering.
  std::vector<NamedTextDocument *> m_filteredDocuments;

  // Set to `nullptr` when the dialog starts, it is set to a non-null
  // value indicating which document has been chosen when one is.  If
  // the user cancels, it will remain `nullptr`.
  NamedTextDocument * NULLABLE m_chosenDocument;

  // The main 2D grid control.  It is owned by this dialog, but the Qt
  // infrastructure automatically deallocates it.
  SMTableWidget *m_tableWidget;

  // Controls.
  QLineEdit *m_filterLineEdit;
  QPushButton *m_closeSelButton;
  QPushButton *m_reloadAllButton;
  QPushButton *m_helpButton;

private:     // funcs
  // Allow access with similar syntax to other classes.
  EditorGlobal *editorGlobal() const;

  // Recompute 'm_filteredDocuments' from 'm_editorGlobal' and
  // 'm_filterLineEdit'.
  void computeFilteredDocuments();

  // Rebuild the table by recomputing the filtered list and then copying
  // it into the table widget.
  void repopulateTable();

  // Get the unfiltered document list to edit.
  NamedTextDocumentList *unfilteredDocList() const;

  // If 'r' is a valid index into 'm_filteredDocuments', return the
  // element at that index.  Otherwise return nullptr.
  NamedTextDocument *getDocAtIf(int r);

public:      // funcs
  OpenFilesDialog(EditorGlobal *editorGlobal,
                  QWidget *parent = NULL, Qt::WindowFlags f = Qt::WindowFlags());
  ~OpenFilesDialog();

  // Show the dialog.  When it closes, if the user has indicated they
  // want to switch to a particular file, return that file.  Otherwise
  // return NULL.
  NamedTextDocument *runDialog(QWidget *callerWindow);

  // QObject methods.
  virtual bool eventFilter(QObject *watched, QEvent *event) OVERRIDE;

  // EventReplayQueryable methods.
  virtual string eventReplayQuery(string const &state) OVERRIDE;

public Q_SLOTS:
  void on_doubleClicked(QModelIndex const &index) NOEXCEPT;
  void on_closeSelected() NOEXCEPT;
  void on_reloadAll() NOEXCEPT;
  void on_help() NOEXCEPT;
  void slot_filterTextChanged(QString const &newText) NOEXCEPT;
  virtual void accept() NOEXCEPT OVERRIDE;
};


#endif // OPEN_FILES_DIALOG_H
