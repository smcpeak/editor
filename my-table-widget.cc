// my-table-widget.cc
// code for my-table-widget.h

#include "my-table-widget.h"             // this module

// smqtutil
#include "qtguiutil.h"                 // toString(QKeyEvent)

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


void MyTableWidget::keyPressEvent(QKeyEvent *event) noexcept
{
  GENERIC_CATCH_BEGIN

  TRACE("MyTableWidget", "keyPressEvent: " << toString(*event));

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


// EOF
