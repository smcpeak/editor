// my-table-widget.cc
// code for my-table-widget.h

#include "my-table-widget.h"           // this module

// smqtutil
#include "qtguiutil.h"                 // keysString(QKeyEvent)

// smbase
#include "exc.h"                       // GENERIC_CATCH_BEGIN/END
#include "trace.h"                     // TRACE

// Qt
#include <QKeyEvent>


MyTableWidget::MyTableWidget(QWidget *parent)
  : QTableWidget(parent)
{}


MyTableWidget::~MyTableWidget()
{}


void MyTableWidget::synthesizeKey(int key, Qt::KeyboardModifiers modifiers)
{
  QKeyEvent press(QEvent::KeyPress, key, modifiers);
  this->QTableWidget::keyPressEvent(&press);

  // I don't think QTableWidget actually cares about key release events,
  // but this seems like the generally right thing to do.
  QKeyEvent release(QEvent::KeyRelease, key, modifiers);
  this->QTableWidget::keyReleaseEvent(&release);
}


void MyTableWidget::keyPressEvent(QKeyEvent *event) NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  TRACE("MyTableWidget", "keyPressEvent: " << keysString(*event));

  switch (event->key()) {
    case Qt::Key_N: {
      // We pass along the same modifiers so that the user can do, e.g.,
      // Shift+N to extend the selection, etc.
      this->synthesizeKey(Qt::Key_Down, event->modifiers());
      return;
    }

    case Qt::Key_P: {
      this->synthesizeKey(Qt::Key_Up, event->modifiers());
      return;
    }
  }

  QTableWidget::keyPressEvent(event);

  GENERIC_CATCH_END
}


void MyTableWidget::configureAsListView()
{
  // Zebra table.
  setAlternatingRowColors(true);

  // Select entire rows at a time.
  setSelectionBehavior(QAbstractItemView::SelectRows);

  // Click to select.  Shift+Click to extend contiguously, Ctrl+Click
  // to toggle one element.
  setSelectionMode(QAbstractItemView::ExtendedSelection);

  // Do not respond to clicks in the tiny top-left corner sliver.
  //
  // I do not appear to be able to get rid of the thin left column
  // altogether.
  setCornerButtonEnabled(false);

  // Do not use Tab to move among cells.  Rather, it should move the
  // focus among controls in the dialog.
  setTabKeyNavigation(false);

  // Do not draw grid lines.  They only add visual clutter.
  setShowGrid(false);
}


void MyTableWidget::initializeColumns(ColumnInitInfo const *columnInfo,
                                      int numColumns)
{
  setColumnCount(numColumns);

  // Header labels.
  QStringList columnLabels;
  for (int i=0; i < numColumns; i++) {
    columnLabels << columnInfo[i].name;
  }
  setHorizontalHeaderLabels(columnLabels);

  // Column widths.
  for (int i=0; i < numColumns; i++) {
    setColumnWidth(i, columnInfo[i].initialWidth);
  }
}


// EOF
