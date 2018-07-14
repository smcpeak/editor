// open-files-dialog.cc
// code for open-files-dialog.h

#include "open-files-dialog.h"         // this module

// editor
#include "my-table-widget.h"           // MyTableWidget

// smqtutil
#include "qtguiutil.h"                 // toString(QKeyEvent)
#include "qtutil.h"                    // toQString

// smbase
#include "array.h"                     // ArrayStack
#include "dev-warning.h"               // DEV_WARNING
#include "exc.h"                       // GENERIC_CATCH_BEGIN/END
#include "trace.h"                     // TRACE

// Qt
#include <QMessageBox>
#include <QModelIndex>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>


// For use with TRACE.
ostream& operator<< (ostream &os, QModelIndex const &index)
{
  if (!index.isValid()) {
    return os << "root";
  }
  else {
    return os << index.parent()
              << ".(r=" << index.row()
              << ", c=" << index.column() << ')';
  }
}


// Height of each row in pixels.
int const ROW_HEIGHT = 20;

// Initial dialog dimensions in pixels.
int const INIT_DIALOG_WIDTH = 500;
int const INIT_DIALOG_HEIGHT = 500;


// The widths are tuned so the columns almost exactly fill the available
// area when a vertical scrollbar is present.  I really want the table
// to automatically ensure its columns fill the width, but I don't know
// if that is possible.
OpenFilesDialog::ColumnInfo const
OpenFilesDialog::s_columnInfo[NUM_TABLE_COLUMNS] = {
  {
    "File name",
    400,
  },
  {
    "Lines",
    50,
  },
};


// Iterate over TableColumn.
#define FOREACH_TABLE_COLUMN(tc)            \
  for (TableColumn tc = (TableColumn)0;     \
       tc < NUM_TABLE_COLUMNS;              \
       tc = (TableColumn)(tc+1))


OpenFilesDialog::OpenFilesDialog(NamedTextDocumentList *docList,
                                 QWidget *parent, Qt::WindowFlags f) :
  ModalDialog(parent, f),
  m_docList(docList),
  m_tableWidget(NULL)
{
  this->setWindowTitle("File Picker");

  QVBoxLayout *vbox = new QVBoxLayout();
  this->setLayout(vbox);

  // This uses the "convenience" class combining a model and a view,
  // rather than using a separate model and view.  Originally I used
  // separate objects since I thought I would be able to take advantage
  // of my existing change notification infrastructure for
  // NamedTextDocumentList and simply relay to the Qt model change
  // notifications, thereby saving the cost of building a copy of the
  // table.
  //
  // However, the problem is the Qt model change design requires every
  // change to be accompanied by a pre-change broadcast and a
  // post-change broadcast.  In contrast, my own system only uses
  // post-change broadcasts.  Rather than complicate by design by adding
  // pre-change notifications, I have chosen to just pay the minor cost
  // of having an extra copy of the table in memory.
  m_tableWidget = new MyTableWidget();
  vbox->addWidget(m_tableWidget);

  // Zebra table.
  m_tableWidget->setAlternatingRowColors(true);

  // Select entire rows at a time.
  m_tableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);

  // Click to select.  Shift+Click to extend contiguously, Ctrl+Click
  // to toggle one element.
  m_tableWidget->setSelectionMode(QAbstractItemView::ExtendedSelection);

  // Do not respond to clicks in the tiny top-left corner sliver.
  //
  // I do not appear to be able to get rid of the thin left column
  // altogether.
  m_tableWidget->setCornerButtonEnabled(false);

  // Do not use Tab to move among cells.  Rather, it should move the
  // focus among controls in the dialog.
  m_tableWidget->setTabKeyNavigation(false);

  // Do not draw grid lines.  They only add visual clutter.
  m_tableWidget->setShowGrid(false);

  // Initialize columns.
  {
    m_tableWidget->setColumnCount(NUM_TABLE_COLUMNS);

    // Header labels.
    QStringList columnLabels;
    FOREACH_TABLE_COLUMN(tc) {
      columnLabels << s_columnInfo[tc].name;
    }
    m_tableWidget->setHorizontalHeaderLabels(columnLabels);

    // Column widths.
    FOREACH_TABLE_COLUMN(tc) {
      m_tableWidget->setColumnWidth(tc, s_columnInfo[tc].initialWidth);
    }
  }

  // The table rows are set by 'repopulateTable', which is called by
  // 'runDialog'.

  QObject::connect(m_tableWidget, &QTableView::doubleClicked,
                   this, &OpenFilesDialog::on_doubleClicked);

  {
    QHBoxLayout *hbox = new QHBoxLayout();
    vbox->addLayout(hbox);

    QPushButton *closeSelButton = new QPushButton("&Close Selected");
    hbox->addWidget(closeSelButton);
    QObject::connect(closeSelButton, &QPushButton::clicked,
                     this, &OpenFilesDialog::on_closeSelected);

    QPushButton *helpButton = new QPushButton("&Help");
    hbox->addWidget(helpButton);
    QObject::connect(helpButton, &QPushButton::clicked,
                     this, &OpenFilesDialog::on_help);

    hbox->addStretch(1);

    this->createOkAndCancelButtons(hbox);
  }

  // Expand to show plenty of the files and their names.
  this->resize(INIT_DIALOG_WIDTH, INIT_DIALOG_HEIGHT);
}


OpenFilesDialog::~OpenFilesDialog()
{}


static char const *documentProcessStatusIndicator(
  NamedTextDocument const *doc)
{
  switch (doc->documentProcessStatus()) {
    default:
      DEV_WARNING("Bad document process status");
      // fallthrough
    case DPS_NONE:           return "";
    case DPS_RUNNING:        return " <running>";
    case DPS_FINISHED:       return " <finished>";
  }
}


void OpenFilesDialog::repopulateTable()
{
  m_tableWidget->clearContents();
  m_tableWidget->setRowCount(m_docList->numFiles());

  // Populate the rows.
  for (int r=0; r < m_docList->numFiles(); r++) {
    NamedTextDocument const *doc = m_docList->getFileAtC(r);

    // Remove the row label.  (The default, a NULL item, renders as a
    // row number, which isn't useful here.)
    m_tableWidget->setVerticalHeaderItem(r, new QTableWidgetItem(""));

    // Flags for the items.  The point is to omit Qt::ItemIsEditable.
    Qt::ItemFlags itemFlags =
      Qt::ItemIsSelectable | Qt::ItemIsEnabled;

    // Filename.
    {
      stringBuilder sb;
      sb << doc->name();
      sb << documentProcessStatusIndicator(doc);
      if (doc->unsavedChanges()) {
        sb << " *";
      }
      QTableWidgetItem *item = new QTableWidgetItem(toQString(sb));
      item->setFlags(itemFlags);
      m_tableWidget->setItem(r, TC_FILENAME, item);
    }

    // Lines.
    {
      QTableWidgetItem *item = new QTableWidgetItem(
        qstringb(doc->numLinesExceptFinalEmpty()));
      item->setFlags(itemFlags);
      item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
      m_tableWidget->setItem(r, TC_LINES, item);
    }

    // Apparently I have to set every row's height manually.  QTreeView
    // has a 'uniformRowHeights' property, but QListView does not.
    m_tableWidget->setRowHeight(r, ROW_HEIGHT);
  }
}


NamedTextDocument *OpenFilesDialog::runDialog()
{
  TRACE("OpenFilesDialog", "runDialog started");
  this->repopulateTable();
  m_tableWidget->setFocus();

  if (this->exec()) {
    QModelIndex idx = m_tableWidget->currentIndex();
    if (idx.isValid() && !idx.parent().isValid()) {
      int r = idx.row();
      if (0 <= r && r < m_docList->numFiles()) {
        NamedTextDocument *doc = m_docList->getFileAt(r);
        TRACE("OpenFilesDialog", "runDialog: returning: " << doc->name());
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
  ArrayStack<NamedTextDocument*> docsToClose;
  bool someHaveUnsavedChanges = false;
  {
    QItemSelectionModel *selectionModel = m_tableWidget->selectionModel();
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
          NamedTextDocument *doc = m_docList->getFileAt(r);
          TRACE("OpenFilesDialog", "  toClose: " << doc->name());
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

  // Close the files.
  for (int i=0; i < docsToClose.length(); i++) {
    NamedTextDocument *doc = docsToClose[i];
    TRACE("OpenFilesDialog", "  removeFile: " << doc->name());
    m_docList->removeFile(doc);
    delete doc;
  }

  // Refresh table contents.
  this->repopulateTable();

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
    "Hint: While the list box has focus, use N and P in place of Down "
    "and Up arrow keys for more convenient keyboard interaction."
  );
  mb.exec();

  GENERIC_CATCH_END
}


// EOF
