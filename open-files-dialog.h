// open-files-dialog.h
// OpenFilesDialog class.

#ifndef OPEN_FILES_DIALOG_H
#define OPEN_FILES_DIALOG_H

#include "file-td-list.h"              // FileTextDocumentList
#include "modal-dialog.h"              // ModalDialog

#include "sm-iostream.h"               // ostream

class FTDLTableModel;                  // ftdl-table-model.h
class MyTableView;                     // my-table-view.h
class QModelIndex;


// This is mainly for debugging.  I put it here because this is, for
// the moment, the only module that deals with QModelIndex.
ostream& operator<< (ostream &os, QModelIndex const &index);


// Dialog to show and manipulate the open files in a document list.
//
// This is sort of like an "editor" for FileTextDocumentList.
class OpenFilesDialog : public ModalDialog {
  Q_OBJECT

private:     // data
  // The list we are showing/editing.
  FileTextDocumentList *m_docList;

  // The Qt "model" that wraps m_docList.  It is owned by m_tableView.
  FTDLTableModel *m_tableModel;

  // The main 2D grid control.  It is owned by this dialog, but the Qt
  // infrastructure automatically deallocates it.
  MyTableView *m_tableView;

public:      // funcs
  OpenFilesDialog(FileTextDocumentList *docList,
                  QWidget *parent = NULL, Qt::WindowFlags f = Qt::WindowFlags());
  ~OpenFilesDialog();

  // Show the dialog.  When it closes, if the user has indicated they
  // want to switch to a particular file, return that file.  Otherwise
  // return NULL.
  FileTextDocument *runDialog();

public slots:
  void on_doubleClicked(QModelIndex const &index) noexcept;
  void on_closeSelected() noexcept;
  void on_help() noexcept;
};


#endif // OPEN_FILES_DIALOG_H
