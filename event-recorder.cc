// event-recorder.cc
// code for event-recorder.h

#include "event-recorder.h"            // this module

// editor
#include "editor-widget.h"             // EditorWidget

// smbase
#include "dev-warning.h"               // DEV_WARNING
#include "syserr.h"                    // xsyserror

// smqtutil
#include "qtguiutil.h"                 // keysString(QKeyEvent)
#include "qtutil.h"                    // operator<<(QString), qObjectPath, isModifierKey

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


bool EventRecorder::eventFilter(QObject *receiver, QEvent *event)
{
  QEvent::Type type = event->type();
  if (type == QEvent::KeyPress ||
      type == QEvent::Shortcut ||
      type == QEvent::MouseButtonPress ||
      type == QEvent::FocusIn ||
      type == QEvent::Resize) {
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
          if (receiver == QApplication::focusWidget()) {
            // Normally keypresses go to the focused widget, in which
            // case we can save a lot of noise.
            m_out << "FocusKeyPress";
          }
          else {
            // This happens, e.g., when interacting with the menus.
            // Focus in menus is a bit weird.
            m_out << "KeyPress " << quoted(qObjectPath(receiver));
          }
          m_out << " " << quoted(keysString(*keyEvent))
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
    else if (type == QEvent::Resize) {
      if (QResizeEvent const *resizeEvent =
            dynamic_cast<QResizeEvent const *>(event)) {
        // I only care about resize for the editor widget.  I would
        // prefer that this module not depend on EditorWidget, so at
        // some point I may add a different way of recognizing the
        // widgets I do care about.
        if (qobject_cast<EditorWidget*>(receiver)) {
          m_out << "ResizeEvent " << quoted(qObjectPath(receiver))
                << " " << quoted(toString(resizeEvent->size()))
                << endl;
        }
      }
    }
    else if (type == QEvent::FocusIn) {
      // Filter for QWidgets.  Without this filtering, I see focus
      // events sent to the QProxyStyle, which is a little odd and
      // anyway not relevant.
      if (receiver && qobject_cast<QWidget*>(receiver)) {
        // Focus is not a kind of input event, so we will not replay it.
        // But it is useful for ensuring synchronization between the
        // test and application, so I will automatically emit a check
        // during recording.  (I can then choose whether to keep it when
        // editing the test.)
        m_out << "CheckFocusWidget " << quoted(qObjectPath(receiver)) << endl;
      }
    }
    else {
      DEV_WARNING("whoops");
    }
  }

  return false;
}


// EOF
