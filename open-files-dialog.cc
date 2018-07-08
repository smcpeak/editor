// open-files-dialog.cc
// code for open-files-dialog.h

#include "open-files-dialog.h"         // this module

// editor
#include "ftdl-table-model.h"          // FTDLTableModel
#include "my-table-view.h"             // MyTableView

// smqtutil
#include "qtguiutil.h"                 // toString(QKeyEvent)

// smbase
#include "exc.h"                       // GENERIC_CATCH_BEGIN/END
#include "trace.h"                     // TRACE

// Qt
#include <QVBoxLayout>


// These two are tuned so the columns almost exactly fill the available
// area when a vertical scrollbar is present.  I really want the table
// to automatically ensure its columns fill the width, but I don't know
// if that is possible.
int const INIT_WIDTH_FILENAME = 405;
int const INIT_WIDTH_LINES = 50;

int const ROW_HEIGHT = 20;

int const INIT_DIALOG_WIDTH = 500;
int const INIT_DIALOG_HEIGHT = 500;


OpenFilesDialog::OpenFilesDialog(FileTextDocumentList *docList,
                                 QWidget *parent, Qt::WindowFlags f) :
  ModalDialog(parent, f),
  m_docList(docList),
  m_tableView(NULL)
{
  this->setWindowTitle("Open Files");

  QVBoxLayout *vbox = new QVBoxLayout();
  this->setLayout(vbox);

  m_tableView = new MyTableView();
  vbox->addWidget(m_tableView);

  // Zebra table.
  m_tableView->setAlternatingRowColors(true);

  // Select entire rows at a time.
  m_tableView->setSelectionBehavior(QAbstractItemView::SelectRows);

  // Click to select.  Shift+Click to extend contiguously, Ctrl+Click
  // to toggle one element.
  m_tableView->setSelectionMode(QAbstractItemView::ExtendedSelection);

  // Do not respond to clicks in the tiny top-left corner sliver.
  //
  // TODO: Can I get rid of the thin left column altogether?
  m_tableView->setCornerButtonEnabled(false);

  // Do not use Tab to move among cells.  Rather, it should move the
  // focus among controls in the dialog.
  m_tableView->setTabKeyNavigation(false);

  // Do not draw grid lines.  They only add visual clutter.
  m_tableView->setShowGrid(false);

  // As recommended in Qt docs, grab old selection model.
  QItemSelectionModel *oldSelModel = m_tableView->selectionModel();

  // Assign a table model and let m_tableView take ownership of it.
  m_tableView->setModel(new FTDLTableModel(m_docList, m_tableView));

  // Per Qt docs: now delete the old selection model.  In my testing,
  // it is always NULL here, but this won't hurt.
  delete oldSelModel;

  // Adjust table dimensions.  (I tried responding to 'data()' queries
  // for Qt::SizeHintRole, but that did not work the way I wanted.)
  m_tableView->setColumnWidth(FTDLTableModel::TC_FILENAME,
                                      INIT_WIDTH_FILENAME);
  m_tableView->setColumnWidth(FTDLTableModel::TC_LINES,
                                      INIT_WIDTH_LINES);

  // Apparently I have to set every row's height manually.  QTreeView
  // has a 'uniformRowHeights' property, but QListView does not.
  for (int i=0; i < m_docList->numFiles(); i++) {
    m_tableView->setRowHeight(i, ROW_HEIGHT);
  }

  QObject::connect(m_tableView, &QTableView::doubleClicked,
                   this, &OpenFilesDialog::on_doubleClicked);

  // TODO: This is not what I want.
  this->createOkAndCancel(vbox);

  // Expand to show plenty of the files and their names.
  this->resize(INIT_DIALOG_WIDTH, INIT_DIALOG_HEIGHT);
}


OpenFilesDialog::~OpenFilesDialog()
{}


FileTextDocument *OpenFilesDialog::runDialog()
{
  if (this->exec()) {
    QModelIndex idx = m_tableView->currentIndex();
    if (idx.isValid() && !idx.parent().isValid()) {
      int r = idx.row();
      if (0 <= r && r < m_docList->numFiles()) {
        FileTextDocument *doc = m_docList->getFileAt(r);
        TRACE("OpenFilesDialog", "runDialog: returning: " << doc->filename);
        return doc;
      }
    }
  }

  TRACE("OpenFilesDialog", "runDialog: returning NULL");
  return NULL;
}


void OpenFilesDialog::on_doubleClicked(QModelIndex const &index) noexcept
{
  GENERIC_CATCH_BEGIN

  // I want to switch to the double-clicked item.  This seems to be
  // sufficient since the clicked item becomes 'current' automatically.
  TRACE("OpenFilesDialog", "doubleClicked: " << index);
  this->accept();

  GENERIC_CATCH_END
}


// EOF
