// open-files-dialog.cc
// code for open-files-dialog.h

#include "open-files-dialog.h"         // this module

// editor
#include "editor-global.h"             // EditorGlobal
#include "pixmaps.h"                   // g_editorPixmaps

// smqtutil
#include "smqtutil/qtguiutil.h"        // keysString(QKeyEvent), messageBox
#include "smqtutil/qtutil.h"           // toQString, SET_QOBJECT_NAME
#include "smqtutil/sm-table-widget.h"  // SMTableWidget

// smbase
#include "smbase/array.h"              // ArrayStack
#include "smbase/exc.h"                // GENERIC_CATCH_BEGIN/END
#include "smbase/sm-file-util.h"       // SMFileUtil
#include "smbase/strutil.h"            // hasSubstring_insens_ascii
#include "smbase/trace.h"              // TRACE

// Qt
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QModelIndex>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>


// Initial dialog dimensions in pixels.  I want it relatively tall by
// default so I can see lots of files before having to scroll or resize.
int const INIT_DIALOG_WIDTH = 900;
int const INIT_DIALOG_HEIGHT = 800;


// Iterate over TableColumn.
#define FOREACH_TABLE_COLUMN(tc)            \
  for (TableColumn tc = (TableColumn)0;     \
       tc < NUM_TABLE_COLUMNS;              \
       tc = (TableColumn)(tc+1))


OpenFilesDialog::OpenFilesDialog(EditorGlobal *editorGlobal,
                                 QWidget *parent, Qt::WindowFlags f) :
  ModalDialog(parent, f),
  m_editorGlobal(editorGlobal),
  m_filteredDocuments(),
  m_chosenDocument(nullptr),
  m_tableWidget(NULL),
  m_closeSelButton(NULL),
  m_helpButton(NULL)
{
  this->setObjectName("OpenFilesDialog");
  this->setWindowTitle("Documents");

  QVBoxLayout *vbox = new QVBoxLayout();
  this->setLayout(vbox);

  // Used mnemonics: cdfhr

  QLabel *tableLabel;
  {
    QHBoxLayout *hbox = new QHBoxLayout();
    vbox->addLayout(hbox);

    tableLabel = new QLabel("&Documents");
    SET_QOBJECT_NAME(tableLabel);
    hbox->addWidget(tableLabel);

    QLabel *downArrowLabel = new QLabel();
    downArrowLabel->setPixmap(g_editorPixmaps->downArrow);
    hbox->addWidget(downArrowLabel);

    hbox->addSpacing(15);

    QLabel *filterLabel = new QLabel("&Filter");
    SET_QOBJECT_NAME(filterLabel);
    hbox->addWidget(filterLabel);

    m_filterLineEdit = new QLineEdit();
    SET_QOBJECT_NAME(m_filterLineEdit);
    hbox->addWidget(m_filterLineEdit);
    filterLabel->setBuddy(m_filterLineEdit);

    QObject::connect(m_filterLineEdit, &QLineEdit::textChanged,
                     this, &OpenFilesDialog::slot_filterTextChanged);

    // Intercept certain keystrokes.
    m_filterLineEdit->installEventFilter(this);
  }

  m_tableWidget = new SMTableWidget();
  vbox->addWidget(m_tableWidget);
  SET_QOBJECT_NAME(m_tableWidget);
  tableLabel->setBuddy(m_tableWidget);

  m_tableWidget->configureAsListView();
  m_tableWidget->setColumnsFillWidth(true);

  m_tableWidget->setColumnInfo(std::vector<SMTableWidget::ColumnInfo>{
    // name       init  min  max  prio
    { "File name", 700, 100,  {},    1 },
    { "Lines",      50,  50, 100,    0 },
  });

  m_tableWidget->installEventFilter(this);

  // The table rows are set by 'repopulateTable', which is called by
  // 'runDialog'.

  QObject::connect(m_tableWidget, &QTableView::doubleClicked,
                   this, &OpenFilesDialog::on_doubleClicked);

  {
    QHBoxLayout *hbox = new QHBoxLayout();
    vbox->addLayout(hbox);

    m_closeSelButton = new QPushButton("&Close Selected");
    hbox->addWidget(m_closeSelButton);
    SET_QOBJECT_NAME(m_closeSelButton);
    QObject::connect(m_closeSelButton, &QPushButton::clicked,
                     this, &OpenFilesDialog::on_closeSelected);

    m_reloadAllButton = new QPushButton("&Reload all");
    hbox->addWidget(m_reloadAllButton);
    SET_QOBJECT_NAME(m_reloadAllButton);
    QObject::connect(m_reloadAllButton, &QPushButton::clicked,
                     this, &OpenFilesDialog::on_reloadAll);

    m_helpButton = new QPushButton("&Help");
    hbox->addWidget(m_helpButton);
    SET_QOBJECT_NAME(m_helpButton);
    QObject::connect(m_helpButton, &QPushButton::clicked,
                     this, &OpenFilesDialog::on_help);

    hbox->addStretch(1);

    this->createOkAndCancelButtons(hbox);
  }

  // Expand to show plenty of the files and their names.
  this->resize(INIT_DIALOG_WIDTH, INIT_DIALOG_HEIGHT);
}


OpenFilesDialog::~OpenFilesDialog()
{
  // See doc/signals-and-dtors.txt.
  QObject::disconnect(m_filterLineEdit, NULL, this, NULL);
  QObject::disconnect(m_tableWidget, NULL, this, NULL);
  QObject::disconnect(m_closeSelButton, NULL, this, NULL);
  QObject::disconnect(m_helpButton, NULL, this, NULL);
}


void OpenFilesDialog::computeFilteredDocuments()
{
  m_filteredDocuments.clear();
  string filter = toString(m_filterLineEdit->text());

  for (int r=0; r < unfilteredDocList()->numDocuments(); r++) {
    NamedTextDocument *doc = unfilteredDocList()->getDocumentAt(r);

    if (hasSubstring_insens_ascii(
          doc->nameWithStatusIndicators(), filter)) {
      m_filteredDocuments.push_back(doc);
    }
  }
}


void OpenFilesDialog::repopulateTable()
{
  computeFilteredDocuments();

  m_tableWidget->clearContents();
  m_tableWidget->setRowCount(m_filteredDocuments.size());

  // Populate the rows.
  for (int r=0; r < (int)m_filteredDocuments.size(); ++r) {
    NamedTextDocument const *doc = m_filteredDocuments.at(r);

    // Remove the row label.  (The default, a NULL item, renders as a
    // row number, which isn't useful here.)
    m_tableWidget->setVerticalHeaderItem(r, new QTableWidgetItem(""));

    // Flags for the items.  The point is to omit Qt::ItemIsEditable.
    Qt::ItemFlags itemFlags =
      Qt::ItemIsSelectable | Qt::ItemIsEnabled;

    // Filename.
    {
      QTableWidgetItem *item = new QTableWidgetItem(
        toQString(doc->nameWithStatusIndicators()));
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

    // Apparently I have to set every row's height explicitly.
    m_tableWidget->setNaturalTextRowHeight(r);
  }
}


NamedTextDocumentList *OpenFilesDialog::unfilteredDocList() const
{
  return &(m_editorGlobal->m_documentList);
}


NamedTextDocument *OpenFilesDialog::getDocAtIf(int r)
{
  if (0 <= r && r < (int)m_filteredDocuments.size()) {
    return m_filteredDocuments.at(r);
  }
  else {
    return nullptr;
  }
}


NamedTextDocument *OpenFilesDialog::runDialog(QWidget *callerWindow)
{
  TRACE("OpenFilesDialog", "runDialog started");

  m_chosenDocument = nullptr;
  m_filterLineEdit->setText("");
  this->repopulateTable();
  m_tableWidget->setCurrentCell(0, 0);
  m_tableWidget->setFocus();

  this->execCentered(callerWindow);

  // Clear the filtered documents in order to not leave dangling
  // pointers in data structures.
  m_filteredDocuments.clear();

  return m_chosenDocument;
}


static string filenamePathBase(NamedTextDocument const *doc)
{
  if (doc->hasFilename()) {
    SMFileUtil sfu;
    return sfu.splitPathBase(doc->filename());
  }
  else {
    // Does not have a file name, return complete name.
    return doc->resourceName();
  }
}


string OpenFilesDialog::eventReplayQuery(string const &state)
{
  if (state == "cursorRow") {
    QModelIndex idx = m_tableWidget->currentIndex();
    if (idx.isValid() && !idx.parent().isValid()) {
      int r = idx.row();
      return stringb(r);
    }
    return "-1";
  }

  else if (state == "numRows") {
    return stringb(m_tableWidget->rowCount());
  }

  else if (state == "cursorDocumentFilenamePathBase") {
    QModelIndex idx = m_tableWidget->currentIndex();
    if (idx.isValid() && !idx.parent().isValid()) {
      int r = idx.row();
      if (NamedTextDocument *doc = getDocAtIf(r)) {
        return filenamePathBase(doc);
      }
    }
    return "(none)";
  }

  else if (state == "allDocumentsFilenamePathBase") {
    stringBuilder sb;
    for (int r=0; r < m_tableWidget->rowCount(); r++) {
      NamedTextDocument *doc = m_filteredDocuments.at(r);
      sb << filenamePathBase(doc) << '\n';
    }
    return sb.str();
  }

  else {
    return EventReplayQueryable::eventReplayQuery(state);
  }
}


void OpenFilesDialog::on_doubleClicked(QModelIndex const &index) NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  // I want to switch to the double-clicked item.  This seems to be
  // sufficient since the clicked item becomes 'current' automatically.
  TRACE("OpenFilesDialog", "doubleClicked: " << index);
  this->accept();

  GENERIC_CATCH_END
}


void OpenFilesDialog::on_closeSelected() NOEXCEPT
{
  GENERIC_CATCH_BEGIN
  TRACE("OpenFilesDialog", "closeSelected");

  // Cursor row, or -1 if no specific row has the cursor.
  QItemSelectionModel *selectionModel = m_tableWidget->selectionModel();
  int cursorRow = -1;
  {
    QModelIndex cursorIndex = selectionModel->currentIndex();
    if (cursorIndex.isValid()) {
      cursorRow = cursorIndex.row();
      TRACE("OpenFilesDialog", "  cursorIndex: " << cursorIndex);
    }
  }
  TRACE("OpenFilesDialog", "  original cursorRow: " << cursorRow);

  // Get the set of documents to close in a first pass so we have them
  // all before doing anything that might jeopardize the indexes.
  ArrayStack<NamedTextDocument*> docsToClose;
  bool someHaveUnsavedChanges = false;
  {
    QModelIndexList selectedRows = selectionModel->selectedRows();
    for (QModelIndexList::iterator it(selectedRows.begin());
         it != selectedRows.end();
         ++it)
    {
      QModelIndex index = *it;
      TRACE("OpenFilesDialog", "  selRow: " << index);

      if (index.isValid() && !index.parent().isValid()) {
        int r = index.row();
        if (NamedTextDocument *doc = getDocAtIf(r)) {
          TRACE("OpenFilesDialog", "  toClose: " << doc->documentName());
          docsToClose.push(doc);
          if (doc->unsavedChanges()) {
            someHaveUnsavedChanges = true;
          }

          // Adjust the desired cursor row.  Removing something at or
          // below does not change it, but removing something above
          // moves it up one.
          //
          // I'm not sure about this logic though.  It seems to behave
          // not quite right when dealing with many selected rows; it
          // ends up in the middle somehow?  But I do that so rarely it
          // doesn't matter much.
          if (cursorRow > 0 &&
              (r < cursorRow ||
               cursorRow == (int)m_filteredDocuments.size()-1)) {
            cursorRow--;
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
    TRACE("OpenFilesDialog", "  removeFile: " << doc->documentName());
    unfilteredDocList()->removeDocument(doc);
    delete doc;
  }

  // Refresh table contents.
  this->repopulateTable();

  // Re-select the desired row.
  TRACE("OpenFilesDialog",  "  final cursorRow: " << cursorRow);
  if (cursorRow >= 0) {
    m_tableWidget->setCurrentCell(cursorRow, 0);
  }

  GENERIC_CATCH_END
}


void OpenFilesDialog::on_reloadAll() NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  int successCount = 0;
  int failureCount = 0;

  for (NamedTextDocument *doc : m_filteredDocuments) {
    if (m_editorGlobal->reloadDocumentFile(this, doc)) {
      successCount++;
    }
    else {
      failureCount++;
    }
  }

  // Update window titles and status bars to remove "[DISKMOD]".
  m_editorGlobal->broadcastEditorViewChanged();

  // Update table entries.
  repopulateTable();

  messageBox(this, "Done", qstringb(
    "Successfully refreshed " << successCount <<
    " files.  Failed to refresh " << failureCount << "."));

  GENERIC_CATCH_END
}


void OpenFilesDialog::on_help() NOEXCEPT
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


void OpenFilesDialog::slot_filterTextChanged(QString const &newText) NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  TRACE("OpenFilesDialog", "filterTextChanged: " << toString(newText));
  repopulateTable();

  GENERIC_CATCH_END
}


bool OpenFilesDialog::eventFilter(QObject *watched, QEvent *event)
{
  if (event->type() == QEvent::KeyPress) {
    QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);
    if (keyEvent->modifiers() == Qt::NoModifier) {
      switch (keyEvent->key()) {
        case Qt::Key_Down:
          if (watched == m_filterLineEdit) {
            // Navigate to the table.
            TRACE("OpenFilesDialog", "down arrow from filter text");
            m_tableWidget->setCurrentCell(0, 0);
            m_tableWidget->setFocus();
            return true;           // Prevent further processing.
          }
          break;

        case Qt::Key_F:
          if (watched == m_tableWidget) {
            // Switch to the filter line.
            TRACE("OpenFilesDialog", "F key from table");
            m_filterLineEdit->setFocus();
            return true;
          }
          break;
      }
    }
  }
  return false;
}


void OpenFilesDialog::accept() NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  QModelIndex idx = m_tableWidget->currentIndex();
  if (idx.isValid() && !idx.parent().isValid()) {
    int r = idx.row();
    if (NamedTextDocument *doc = getDocAtIf(r)) {
      TRACE("OpenFilesDialog", "accept: chosen: " << doc->documentName());
      m_chosenDocument = doc;

      // This causes the dialog to close.
      ModalDialog::accept();
    }
    else {
      // I don't think this is possible.
      TRACE("OpenFilesDialog", "accept: nothing chosen because "
                               "index " << r << " is out of range");
    }
  }
  else {
    TRACE("OpenFilesDialog", "accept: nothing chosen because "
                             "nothing is selected");
  }

  GENERIC_CATCH_END
}


// EOF
