// event-recorder.cc
// code for event-recorder.h

#include "event-recorder.h"            // this module

// smbase
#include "dev-warning.h"               // DEV_WARNING
#include "syserr.h"                    // xsyserror

// smqtutil
#include "qtguiutil.h"                 // toString(QKeyEvent)
#include "qtutil.h"                    // operator<<(QString)

// Qt
#include <QApplication>
#include <QCoreApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QShortcutEvent>
#include <QWidget>

// libc
#include <string.h>                    // strcmp


EventRecorder::EventRecorder(string const &filename)
  : m_out(filename.c_str())
{
  if (m_out.fail()) {
    xsyserror("open", filename);
  }

  QCoreApplication::instance()->installEventFilter(this);
}


EventRecorder::~EventRecorder()
{
  QCoreApplication::instance()->removeEventFilter(this);
}


static string objectPath(QObject const *obj)
{
  if (!obj) {
    return "null";
  }

  if (!obj->parent()) {
    // Root object.
    return stringb(obj->objectName());
  }
  else {
    return stringb(objectPath(obj->parent()) <<
                   "." << obj->objectName());
  }
}


bool EventRecorder::eventFilter(QObject *receiver, QEvent *event)
{
  QEvent::Type type = event->type();
  if (type == QEvent::KeyPress ||
      type == QEvent::Shortcut ||
      type == QEvent::MouseButtonPress) {
    if (receiver &&
        0==strcmp(receiver->metaObject()->className(), "QWidgetWindow")) {
      // Filter these out.  QWidgetWindow is a private implementation
      // class in Qt, and I don't want to synthesize events for it, so
      // I will not record them either.
    }
    else if (type == QEvent::KeyPress) {
      if (QKeyEvent const *keyEvent =
            dynamic_cast<QKeyEvent const *>(event)) {
        m_out << "KeyPress \"" << objectPath(receiver)
              << "\" \"" << toString(*keyEvent) << "\""
              << endl;
      }
    }
    else if (type == QEvent::Shortcut) {
      if (QShortcutEvent const *shortcutEvent =
            dynamic_cast<QShortcutEvent const *>(event)) {
        m_out << "Shortcut \"" << objectPath(receiver)
              << "\" \"" << shortcutEvent->key().toString()
              << "\"" << endl;
      }
    }
    else if (type == QEvent::MouseButtonPress) {
      if (QMouseEvent const *mouseEvent =
            dynamic_cast<QMouseEvent const *>(event)) {
        m_out << "MouseEvent \"" << objectPath(receiver)
              << "\" \"" << toString(mouseEvent->buttons())
              << "\" \"" << toString(mouseEvent->pos())
              << "\"" << endl;
      }
    }
    else {
      DEV_WARNING("whoops");
    }
  }

  return false;
}


// EOF
