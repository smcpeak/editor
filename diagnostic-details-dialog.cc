// diagnostic-details-dialog.cc
// Code for `diagnostic-details-dialog` module.

#include "diagnostic-details-dialog.h" // this module

#include "smqtutil/qtutil.h"           // SET_QOBJECT_NAME

#include "smbase/exc.h"                // GENERIC_CATCH_BEGIN/END

#include <QHeaderView>
#include <QKeyEvent>
#include <QLabel>
#include <QScrollBar>
#include <QSplitter>
#include <QTableWidget>
#include <QVBoxLayout>

#include <utility>                     // std::move


void DiagnosticDetailsDialog::updateTopPanel()
{
  int row = m_table->currentRow();
  if (0 <= row && row < m_diagnostics.size()) {
    Element const &elt = m_diagnostics[row];
    m_locationLabel->setText(QString("%1%2:%3")
      .arg(elt.m_dir)
      .arg(elt.m_file)
      .arg(elt.m_line));
    m_messageLabel->setText(elt.m_message);
  }
}


void DiagnosticDetailsDialog::repopulateTable()
{
  m_table->setRowCount(m_diagnostics.size());

  // Accumulates the max width; used for sizing the last column.
  int maxMessageWidth = 300;

  for (int row=0; row < m_diagnostics.size(); ++row) {
    Element const &elt = m_diagnostics[row];

    // Next column index to use.
    int c = 0;

    {
      QTableWidgetItem *item = new QTableWidgetItem(elt.m_dir);

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

    int w = m_table->fontMetrics().horizontalAdvance(elt.m_message);
    maxMessageWidth = qMax(maxMessageWidth, w);
  }

  m_table->setColumnWidth(0, 100);
  m_table->setColumnWidth(1, 150);
  m_table->setColumnWidth(2, maxMessageWidth + 20 /*padding guess*/);

  if (!m_diagnostics.isEmpty()) {
    m_table->selectRow(0);
  }
}


void DiagnosticDetailsDialog::scrollTableHorizontally(int delta)
{
  QScrollBar *sb = m_table->horizontalScrollBar();
  sb->setValue(sb->value() + delta);
}


void DiagnosticDetailsDialog::scrollTableToLeftSide()
{
  QScrollBar *sb = m_table->horizontalScrollBar();
  sb->setValue(0);
}


void DiagnosticDetailsDialog::scrollTableToRightSide()
{
  QScrollBar *sb = m_table->horizontalScrollBar();
  sb->setValue(sb->maximum());
}


void DiagnosticDetailsDialog::moveTableSelection(int delta)
{
  int row = m_table->currentRow();
  int newRow = qBound(0, row + delta, m_table->rowCount() - 1);
  if (newRow != row) {
    m_table->selectRow(newRow);
    m_table->scrollToItem(m_table->item(newRow, 0));
  }
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
    case Qt::Key_P:
    case Qt::Key_Up:
      moveTableSelection(-1);
      break;

    case Qt::Key_N:
    case Qt::Key_Down:
      moveTableSelection(+1);
      break;

    case Qt::Key_B:
    case Qt::Key_Left:
      scrollTableHorizontally(-100);
      break;

    case Qt::Key_F:
    case Qt::Key_Right:
      scrollTableHorizontally(+100);
      break;

    case Qt::Key_A:
      scrollTableToLeftSide();
      break;

    case Qt::Key_E:
      scrollTableToRightSide();
      break;

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


DiagnosticDetailsDialog::~DiagnosticDetailsDialog()
{
  QObject::disconnect(m_table->selectionModel(), nullptr, this, nullptr);
}


DiagnosticDetailsDialog::DiagnosticDetailsDialog(QWidget *parent)
  : QDialog(parent),
    m_diagnostics(),
    m_locationLabel(nullptr),
    m_messageLabel(nullptr),
    m_splitter(nullptr),
    m_table(nullptr)
{
  setWindowTitle("Diagnostic Details");
  resize(800, 600);
  setModal(false);

  QVBoxLayout *outerVBox = new QVBoxLayout(this);
  m_splitter = new QSplitter(Qt::Vertical, this);

  // Top panel: location and message.
  {
    QWidget *topPanel = new QWidget(this);
    QVBoxLayout *vbox = new QVBoxLayout(topPanel);

    m_locationLabel = new QLabel(this);
    SET_QOBJECT_NAME(m_locationLabel);
    vbox->addWidget(m_locationLabel);

    m_messageLabel = new QLabel(this);
    SET_QOBJECT_NAME(m_messageLabel);
    m_messageLabel->setWordWrap(true);
    m_messageLabel->setAlignment(Qt::AlignTop);

    // Ensure the message takes all extra vertical space.
    m_messageLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    vbox->addWidget(m_messageLabel);

    m_splitter->addWidget(topPanel);
  }

  // Bottom panel (the table).
  {
    m_table = new QTableWidget(this);
    m_table->setColumnCount(3);
    m_table->setHorizontalHeaderLabels({
      "Dir", "File:Line", "Message" });
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_table->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_table->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    m_table->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);

    // This is the only widget that can receive focus.
    m_table->setFocusPolicy(Qt::StrongFocus);

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
