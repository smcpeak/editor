// my-table-view.cc
// code for my-table-view.h

#include "my-table-view.h"             // this module

// smqtutil
#include "qtguiutil.h"                 // toString(QKeyEvent)

// smbase
#include "exc.h"                       // GENERIC_CATCH_BEGIN/END
#include "trace.h"                     // TRACE

// Qt
#include <QKeyEvent>


MyTableView::MyTableView(QWidget *parent)
  : QTableView(parent)
{}


MyTableView::~MyTableView()
{}


void MyTableView::synthesizeKey(int key, Qt::KeyboardModifiers modifiers)
{
  QKeyEvent press(QEvent::KeyPress, key, modifiers);
  this->QTableView::keyPressEvent(&press);

  // I don't think QTableView actually cares about key release events,
  // but this seems like the generally right thing to do.
  QKeyEvent release(QEvent::KeyRelease, key, modifiers);
  this->QTableView::keyReleaseEvent(&release);
}


void MyTableView::keyPressEvent(QKeyEvent *event) noexcept
{
  GENERIC_CATCH_BEGIN

  TRACE("MyTableView", "keyPressEvent: " << toString(*event));

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

  QTableView::keyPressEvent(event);

  GENERIC_CATCH_END
}


// EOF
