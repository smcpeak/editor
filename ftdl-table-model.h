// ftdl-table-model.h
// Declare FTDLTableModel class.

#ifndef FTDL_TABLE_MODEL_H
#define FTDL_TABLE_MODEL_H

#include "file-td-list.h"              // FileTextDocumentList

#include "macros.h"                    // NO_OBJECT_COPIES

#include <QAbstractTableModel>


// This presents a FileTextDocumentList as a QAbstractItemModel for use
// with the Qt widgets that use models, such as QTableView.
class FTDLTableModel : public QAbstractTableModel {
  // I think copying would be safe, but I do not intend to do it.
  NO_OBJECT_COPIES(FTDLTableModel);

public:      // types
  // The columns of this table.
  enum TableColumn {
    TC_FILENAME,
    TC_LINES,

    NUM_TABLE_COLUMNS
  };

public:      // static data
  static int s_objectCount;

public:      // instance data
  // The list we are presenting as a table.  This is not an owner
  // pointer; the client is responsible for ensuring its lifetime
  // is longer than that of 'this' object.
  FileTextDocumentList *m_docList;

public:      // funcs
  FTDLTableModel(FileTextDocumentList *docList, QObject *parent = NULL);
  ~FTDLTableModel();

  // Return the user-visible column title.
  static char const *columnName(TableColumn tc);

  // Publish two protected QAbstractItemModel methods.
  using QAbstractItemModel::beginResetModel;
  using QAbstractItemModel::endResetModel;

  // QAbstractItemModel methods.
  virtual int rowCount(QModelIndex const &parent) const override;
  virtual int columnCount(QModelIndex const &parent) const override;
  virtual QVariant data(QModelIndex const &index, int role) const override;
  virtual QVariant headerData(int section, Qt::Orientation orientation,
                              int role) const override;
};


#endif // FTDL_TABLE_MODEL_H
