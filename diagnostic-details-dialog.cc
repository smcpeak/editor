// diagnostic-details-dialog.cc
// Code for `diagnostic-details-dialog` module.

#include "diagnostic-details-dialog.h" // this module

#include "smqtutil/qtguiutil.h"        // removeWindowContextHelpButton
#include "smqtutil/qtutil.h"           // SET_QOBJECT_NAME
#include "smqtutil/sm-table-widget.h"  // SMTableWidget

#include "smbase/exc.h"                // GENERIC_CATCH_BEGIN/END
#include "smbase/xassert.h"            // xassert

#include <QHeaderView>
#include <QKeyEvent>
#include <QLabel>
#include <QPlainTextEdit>
#include <QScrollBar>
#include <QSplitter>
#include <QTableWidget>
#include <QVBoxLayout>

#include <utility>                     // std::move
#include <vector>                      // std::vector


void DiagnosticDetailsDialog::updateTopPanel()
{
  int row = m_table->currentRow();
  if (0 <= row && row < m_diagnostics.size()) {
    Element const &elt = m_diagnostics[row];
    m_locationLabel->setText(QString("%1%2:%3")
      .arg(elt.m_dir)
      .arg(elt.m_file)
      .arg(elt.m_line));
    m_messageText->setPlainText(elt.m_message);
  }
}


// Return `s` without its final slash if it has one; otherwise just
// return `s` as-is.
static QString removeFinalSlash(QString const &s)
{
  if (s.endsWith('/')) {
    return s.chopped(1);
  }
  else {
    return s;
  }
}


void DiagnosticDetailsDialog::repopulateTable()
{
  m_table->setRowCount(m_diagnostics.size());

  for (int row=0; row < m_diagnostics.size(); ++row) {
    Element const &elt = m_diagnostics[row];

    // Next column index to use.
    int c = 0;

    {
      // Although `m_dir` has (should have) a final slash, in this
      // column I want to not have it.
      //
      // Also add a trailing space to get a bit more visual separation.
      // (I would prefer to somehow adjust the column's built-in
      // padding, but I think I need a delegate for that, which is
      // overkill for the moment.)
      //
      QTableWidgetItem *item = new QTableWidgetItem(
        removeFinalSlash(elt.m_dir) + " ");

      // Use right alignment so the final part of the path name is
      // visible, as this is mainly for disambiguation among files.
      item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);

      m_table->setItem(row, c++, item);
    }

    {
      QTableWidgetItem *item = new QTableWidgetItem(
        QString("%1:%2").arg(elt.m_file).arg(elt.m_line));
      m_table->setItem(row, c++, item);
    }

    {
      QTableWidgetItem *item = new QTableWidgetItem(elt.m_message);
      m_table->setItem(row, c++, item);
    }

    m_table->setNaturalTextRowHeight(row);
  }

  // The first two column widths are initialized in the ctor, but the
  // final column size is computed from the actual data.
  m_table->resizeColumnToContents(2);

  if (!m_diagnostics.isEmpty()) {
    m_table->selectRow(0);
  }

  updateTopPanel();
}


void DiagnosticDetailsDialog::on_tableSelectionChanged() noexcept
{
  GENERIC_CATCH_BEGIN

  updateTopPanel();

  GENERIC_CATCH_END
}


void DiagnosticDetailsDialog::keyPressEvent(QKeyEvent *event)
{
  GENERIC_CATCH_BEGIN

  switch (event->key()) {
    case Qt::Key_Enter:
    case Qt::Key_Return: {
      int row = m_table->currentRow();
      if (0 <= row && row < m_diagnostics.size()) {
        Element const &elt = m_diagnostics[row];
        Q_EMIT signal_jumpToLocation(
          elt.m_dir + elt.m_file,
          elt.m_line);
      }
      break;
    }

    case Qt::Key_Escape:
      close();
      break;

    default:
      QDialog::keyPressEvent(event);
  }

  GENERIC_CATCH_END
}


void DiagnosticDetailsDialog::showEvent(QShowEvent *event)
{
  QDialog::showEvent(event);

  // Start with focus on the table.  The message label can receive focus
  // in order to scroll it with Up and Down, but we should start with
  // the table.
  m_table->setFocus();
}


DiagnosticDetailsDialog::~DiagnosticDetailsDialog()
{
  QObject::disconnect(m_table->selectionModel(), nullptr, this, nullptr);
}


DiagnosticDetailsDialog::DiagnosticDetailsDialog(QWidget *parent)
  : QDialog(parent),
    m_diagnostics(),
    m_locationLabel(nullptr),
    m_messageText(nullptr),
    m_splitter(nullptr),
    m_table(nullptr)
{
  setWindowTitle("Diagnostic Details");
  resize(800, 600);
  setModal(false);
  removeWindowContextHelpButton(this);

  QVBoxLayout *outerVBox = new QVBoxLayout(this);

  // Eliminate margins on outer box so the table goes right to the
  // dialog edge.
  outerVBox->setContentsMargins(0, 0, 0, 0);

  m_splitter = new QSplitter(Qt::Vertical, this);
  SET_QOBJECT_NAME(m_splitter);

  // Top panel: location and message.
  {
    QWidget *topPanel = new QWidget(this);
    SET_QOBJECT_NAME(topPanel);

    QVBoxLayout *vbox = new QVBoxLayout(topPanel);

    // Eliminate margins between the text boxes and the dialog edge.
    vbox->setContentsMargins(0, 0, 0, 0);

    m_locationLabel = new QLabel(this);
    SET_QOBJECT_NAME(m_locationLabel);
    vbox->addWidget(m_locationLabel);

    m_messageText = new QPlainTextEdit(this);
    SET_QOBJECT_NAME(m_messageText);
    m_messageText->setReadOnly(true);

    // Ensure the message takes all extra vertical space.
    m_messageText->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    vbox->addWidget(m_messageText);

    m_splitter->addWidget(topPanel);
  }

  // Bottom panel (the table).
  {
    m_table = new SMTableWidget(this);
    SET_QOBJECT_NAME(m_table);

    m_table->configureAsListView();

    // Only select one row at a time.
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);

    // Globally disable elision, which allows right-alignment to work
    // the way I want.
    m_table->setTextElideMode(Qt::ElideNone);

    std::vector<SMTableWidget::ColumnInfo> columnInfo = {
      // name       init min max prio
      { "Dir",       100, 50, {}, 0 },
      { "File:Line", 150, 50, {}, 0 },
      { "Message",   400, 50, {}, 1 },
    };
    m_table->setColumnInfo(columnInfo);

    // Set the Message column title to be left aligned.  We make its
    // width very large, so a centered title is often outside the
    // viewport.
    m_table->horizontalHeaderItem(2)->
      setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    m_table->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    m_table->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);

    m_splitter->addWidget(m_table);
  }

  m_splitter->setSizes({ 200, 400 });
  outerVBox->addWidget(m_splitter);

  QObject::connect(
    m_table->selectionModel(), &QItemSelectionModel::currentRowChanged,
    this, &DiagnosticDetailsDialog::on_tableSelectionChanged);
}


void DiagnosticDetailsDialog::setDiagnostics(
  QVector<Element> &&diagnostics)
{
  m_diagnostics = std::move(diagnostics);
  repopulateTable();
}


// EOF
