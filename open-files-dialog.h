// open-files-dialog.h
// OpenFilesDialog class.

#ifndef OPEN_FILES_DIALOG_H
#define OPEN_FILES_DIALOG_H

#include "named-td-list.h"             // NamedTextDocumentList
#include "modal-dialog.h"              // ModalDialog

#include "refct-serf.h"                // RCSerf
#include "sm-iostream.h"               // ostream
#include "sm-noexcept.h"               // NOEXCEPT

class MyTableWidget;                   // my-table-widget.h

class QModelIndex;
class QPushButton;


// This is mainly for debugging.  I put it here because this is, for
// the moment, the only module that deals with QModelIndex.
ostream& operator<< (ostream &os, QModelIndex const &index);


// Dialog to show and manipulate the open files in a document list.
//
// This is sort of like an "editor" for NamedTextDocumentList.
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
  RCSerf<NamedTextDocumentList> m_docList;

  // The main 2D grid control.  It is owned by this dialog, but the Qt
  // infrastructure automatically deallocates it.
  MyTableWidget *m_tableWidget;

  // Buttons.
  QPushButton *m_closeSelButton;
  QPushButton *m_helpButton;

private:     // funcs
  // Rebuild the table by copying from 'm_docList'.
  void repopulateTable();

public:      // funcs
  OpenFilesDialog(NamedTextDocumentList *docList,
                  QWidget *parent = NULL, Qt::WindowFlags f = Qt::WindowFlags());
  ~OpenFilesDialog();

  // Show the dialog.  When it closes, if the user has indicated they
  // want to switch to a particular file, return that file.  Otherwise
  // return NULL.
  NamedTextDocument *runDialog();

public Q_SLOTS:
  void on_doubleClicked(QModelIndex const &index) NOEXCEPT;
  void on_closeSelected() NOEXCEPT;
  void on_help() NOEXCEPT;
};


#endif // OPEN_FILES_DIALOG_H
