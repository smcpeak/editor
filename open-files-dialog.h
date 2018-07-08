// open-files-dialog.h
// OpenFilesDialog class.

#ifndef OPEN_FILES_DIALOG_H
#define OPEN_FILES_DIALOG_H

#include "file-td-list.h"              // FileTextDocumentList
#include "modal-dialog.h"              // ModalDialog

#include "sm-iostream.h"               // ostream

class QModelIndex;
class QTableView;


// This is mainly for debugging.  I put it here because this is, for
// the moment, the only module that deals with QModelIndex.
ostream& operator<< (ostream &os, QModelIndex const &index);


// Dialog to show and manipulate the open files in a document list.
//
// This is sort of like an "editor" for FileTextDocumentList.
class OpenFilesDialog : public ModalDialog {
  Q_OBJECT

private:     // model data
  // The list we are showing/editing.
  FileTextDocumentList *m_docList;

private:     // dialog controls
  // The main 2D grid control.  It is owned by this dialog, but the Qt
  // infrastructure automatically deallocates it.
  QTableView *m_tableView;

public:      // funcs
  OpenFilesDialog(FileTextDocumentList *docList,
                  QWidget *parent = NULL, Qt::WindowFlags f = Qt::WindowFlags());
  ~OpenFilesDialog();

  // Show the dialog.  When it closes, if the user has indicated they
  // want to switch to a particular file, return that file.  Otherwise
  // return NULL.
  FileTextDocument *runDialog();

public slots:
  void on_doubleClicked(QModelIndex const &index);
};


#endif // OPEN_FILES_DIALOG_H
