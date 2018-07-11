// open-files-dialog.h
// OpenFilesDialog class.

#ifndef OPEN_FILES_DIALOG_H
#define OPEN_FILES_DIALOG_H

#include "file-td-list.h"              // FileTextDocumentList
#include "modal-dialog.h"              // ModalDialog

#include "refct-serf.h"                // RCSerf
#include "sm-iostream.h"               // ostream

class MyTableWidget;                   // my-table-view.h

class QModelIndex;


// This is mainly for debugging.  I put it here because this is, for
// the moment, the only module that deals with QModelIndex.
ostream& operator<< (ostream &os, QModelIndex const &index);


// Dialog to show and manipulate the open files in a document list.
//
// This is sort of like an "editor" for FileTextDocumentList.
class OpenFilesDialog : public ModalDialog {
  Q_OBJECT

public:      // types
  // The columns of this table.
  enum TableColumn {
    TC_FILENAME,
    TC_LINES,

    NUM_TABLE_COLUMNS
  };

  struct ColumnInfo {
    QString name;            // User-visible column name.
    int initialWidth;        // Initial column width in pixels.
  };

private:     // class data
  // Information about each column.
  static ColumnInfo const s_columnInfo[NUM_TABLE_COLUMNS];

private:     // instance data
  // The list we are showing/editing.
  RCSerf<FileTextDocumentList> m_docList;

  // The main 2D grid control.  It is owned by this dialog, but the Qt
  // infrastructure automatically deallocates it.
  MyTableWidget *m_tableWidget;

private:     // funcs
  // Rebuild the table by copying from 'm_docList'.
  void repopulateTable();

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
