// open-files-dialog.cc
// code for open-files-dialog.h

#include "open-files-dialog.h"         // this module

// editor
#include "ftdl-table-model.h"          // FTDLTableModel
#include "my-table-view.h"             // MyTableView

// smqtutil
#include "qtguiutil.h"                 // toString(QKeyEvent)

// smbase
#include "array.h"                     // ArrayStack
#include "exc.h"                       // GENERIC_CATCH_BEGIN/END
#include "trace.h"                     // TRACE

// Qt
#include <QMessageBox>
#include <QPushButton>
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
  m_tableModel(NULL),
  m_tableView(NULL)
{
  this->setWindowTitle("File Picker");

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

  // Build the table model, put it in the view, and let the view take
  // ownership of it.
  m_tableModel = new FTDLTableModel(m_docList, m_tableView);
  m_tableView->setModel(m_tableModel);

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

  {
    QHBoxLayout *hbox = new QHBoxLayout();
    vbox->addLayout(hbox);

    QPushButton *closeSelButton = new QPushButton("Close Selected");
    hbox->addWidget(closeSelButton);
    QObject::connect(closeSelButton, &QPushButton::clicked,
                     this, &OpenFilesDialog::on_closeSelected);

    QPushButton *helpButton = new QPushButton("Help");
    hbox->addWidget(helpButton);
    QObject::connect(helpButton, &QPushButton::clicked,
                     this, &OpenFilesDialog::on_help);

    hbox->addStretch(1);

    QPushButton *okButton = new QPushButton("Ok");
    hbox->addWidget(okButton);
    okButton->setDefault(true);
    QObject::connect(okButton, &QPushButton::clicked,
                     this, &OpenFilesDialog::accept);

    QPushButton *cancelButton = new QPushButton("Cancel");
    hbox->addWidget(cancelButton);
    QObject::connect(cancelButton, &QPushButton::clicked,
                     this, &OpenFilesDialog::reject);
  }

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


void OpenFilesDialog::on_closeSelected() noexcept
{
  GENERIC_CATCH_BEGIN
  TRACE("OpenFilesDialog", "closeSelected");

  // Get the set of documents to close in a first pass so we have them
  // all before doing anything that might jeopardize the indexes.
  ArrayStack<FileTextDocument*> docsToClose;
  bool someHaveUnsavedChanges = false;
  {
    QItemSelectionModel *selectionModel = m_tableView->selectionModel();
    QModelIndexList selectedRows = selectionModel->selectedRows();
    for (QModelIndexList::iterator it(selectedRows.begin());
         it != selectedRows.end();
         ++it)
    {
      QModelIndex index = *it;
      TRACE("OpenFilesDialog", "  selRow: " << index);

      if (index.isValid() && !index.parent().isValid()) {
        int r = index.row();
        if (0 <= r && r < m_docList->numFiles()) {
          FileTextDocument *doc = m_docList->getFileAt(r);
          TRACE("OpenFilesDialog", "  toClose: " << doc->filename);
          docsToClose.push(doc);
          if (doc->unsavedChanges()) {
            someHaveUnsavedChanges = true;
          }
        }
      }
    }
  }

  if (someHaveUnsavedChanges) {
    QMessageBox mb(this);
    mb.setWindowTitle("Discard Unsaved Changes?");
    mb.setText(
      "At least one of the selected files has unsaved changes.  "
      "Are you sure you want to discard them?");
    mb.addButton(QMessageBox::Discard);
    mb.addButton(QMessageBox::Cancel);
    if (mb.exec() != QMessageBox::Discard) {
      return;
    }
  }

  // The table widget must be informed that its contents are changing.
  // This is the "crude sledgehammer" approach, saying that everything
  // is changing.  I'm also slightly abusing the Qt model system here
  // by telling the model to broadcast begin/endResetModel rather than
  // letting it be in control of that.
  TRACE("OpenFilesDialog", "  calling beginReset");
  m_tableModel->beginResetModel();

  // Close the files.
  for (int i=0; i < docsToClose.length(); i++) {
    FileTextDocument *doc = docsToClose[i];
    TRACE("OpenFilesDialog", "  removeFile: " << doc->filename);
    m_docList->removeFile(doc);
    delete doc;
  }

  // Finish the broadcast notification.
  TRACE("OpenFilesDialog", "  calling endReset");
  m_tableModel->endResetModel();
  TRACE("OpenFilesDialog", "  done reset");

  GENERIC_CATCH_END
}


void OpenFilesDialog::on_help() noexcept
{
  GENERIC_CATCH_BEGIN

  TRACE("OpenFilesDialog", "help");
  QMessageBox mb(this);
  mb.setWindowTitle("File Picker Help");
  mb.setText(
    "Choose a file to edit.\n"
    "\n"
    "Select files and then \"Close Selected\" to close them.  "
    "Use Shift+click and Ctrl+click to multiselect.\n"
    "\n"
    "Hint: Use N and P in place of Down and Up arrow keys for "
    "more convenient keyboard interaction."
  );
  mb.exec();

  GENERIC_CATCH_END
}


// EOF
