// event-recorder.cc
// code for event-recorder.h

#include "event-recorder.h"            // this module

// smbase
#include "dev-warning.h"               // DEV_WARNING
#include "syserr.h"                    // xsyserror

// smqtutil
#include "qtguiutil.h"                 // toString(QKeyEvent)
#include "qtutil.h"                    // operator<<(QString), qObjectPath

// smbase
#include "strutil.h"                   // quoted

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
  : m_out(filename.c_str(), std::ios::binary /*use LF line endings*/)
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


static bool isModifierKey(int key)
{
  return key == Qt::Key_Shift ||
         key == Qt::Key_Control ||
         key == Qt::Key_Alt;
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
      //
      // I see "QWidgetWindow" when running Qt on Windows.  I do not
      // know if it is different on different platforms.
    }
    else if (type == QEvent::KeyPress) {
      if (QKeyEvent const *keyEvent =
            dynamic_cast<QKeyEvent const *>(event)) {
        if (isModifierKey(keyEvent->key())) {
          // Filter out keypresses for isolated modifiers.  They just
          // add noise to the recording.
        }
        else {
          m_out << "KeyPress " << quoted(qObjectPath(receiver))
                << " " << quoted(toString(*keyEvent))
                << " " << quoted(keyEvent->text())
                << endl;
        }
      }
    }
    else if (type == QEvent::Shortcut) {
      if (QShortcutEvent const *shortcutEvent =
            dynamic_cast<QShortcutEvent const *>(event)) {
        m_out << "Shortcut " << quoted(qObjectPath(receiver))
              << " " << quoted(shortcutEvent->key().toString())
              << endl;
      }
    }
    else if (type == QEvent::MouseButtonPress) {
      if (QMouseEvent const *mouseEvent =
            dynamic_cast<QMouseEvent const *>(event)) {
        m_out << "MouseEvent " << quoted(qObjectPath(receiver))
              << " " << quoted(toString(mouseEvent->buttons()))
              << " " << quoted(toString(mouseEvent->pos()))
              << endl;
      }
    }
    else {
      DEV_WARNING("whoops");
    }
  }

  return false;
}


// EOF
