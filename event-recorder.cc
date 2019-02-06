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
  : m_outStream(filename.c_str(), std::ios::binary /*use LF line endings*/),
    m_ordinaryKeyChars()
{
  if (m_outStream.fail()) {
    xsyserror("open", filename);
  }

  QCoreApplication::instance()->installEventFilter(this);
}


EventRecorder::~EventRecorder()
{
  this->flushOrdinaryKeyChars();
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
        this->recordKeyEvent(receiver, keyEvent);
      }
    }
    else if (type == QEvent::Shortcut) {
      if (QShortcutEvent const *shortcutEvent =
            dynamic_cast<QShortcutEvent const *>(event)) {
        this->recordEvent(stringb(
          "Shortcut " << quoted(qObjectPath(receiver)) <<
          " " << quoted(shortcutEvent->key().toString())));
      }
    }
    else if (type == QEvent::MouseButtonPress) {
      if (QMouseEvent const *mouseEvent =
            dynamic_cast<QMouseEvent const *>(event)) {
        this->recordEvent(stringb(
          "MouseEvent " << quoted(qObjectPath(receiver)) <<
          " " << quoted(toString(mouseEvent->buttons())) <<
          " " << quoted(toString(mouseEvent->pos()))));
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
          this->recordEvent(stringb(
            "ResizeEvent " << quoted(qObjectPath(receiver)) <<
            " " << quoted(toString(resizeEvent->size()))));
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
        this->recordEvent(stringb(
          "CheckFocusWidget " << quoted(qObjectPath(receiver))));
      }
    }
    else {
      // Should not get here due to 'if' filter at top.
      DEV_WARNING("whoops");
    }
  }

  return false;
}


// If 'keyEvent' is the press of a key that denotes itself in the
// ordinary way, return the denoted character.  Otherwise return 0.
static char isOrdinaryKeyPress(QKeyEvent const *keyEvent)
{
  if (keyEvent->modifiers() == Qt::NoModifier) {
    int k = keyEvent->key();
    if ((0x20 <= k && k <= 0x40) ||      // Key_Space to Key_At
        (0x5b <= k && k <= 0x60) ||      // Key_BracketLeft to Key_QuoteLeft
        (0x7b <= k && k <= 0x7e)) {      // Key_BraceLeft to Key_AsciiTilde
      // Printable non-letter key codes correspond directly to ASCII.
      return (char)k;
    }
    else if (0x41 <= k && k <= 0x5a) {   // Letters
      // The Qt key codes correspond to the uppercase letters, but when
      // typed without modifiers, denote lowercase letters.
      return (char)(k + 0x20);
    }
  }

  return 0;
}


void EventRecorder::recordKeyEvent(QObject *receiver, QKeyEvent const *keyEvent)
{
  if (isModifierKey(keyEvent->key())) {
    // Filter out keypresses for isolated modifiers.  They just
    // add noise to the recording.
    return;
  }

  string eventPrefix;
  if (receiver == QApplication::focusWidget()) {
    // Normally keypresses go to the focused widget, in which
    // case we can save a lot of noise by using the focus.

    // Compress sequences of ordinary key presses.
    if (char c = isOrdinaryKeyPress(keyEvent)) {
      this->recordOrdinaryKeyPress(c);
      return;
    }

    // The "PR" means press and release.  I am not currently
    // watching the release events, so here I just assume that
    // press is followed immediately by release.  The release
    // event is important because, as it happens, that is when
    // I call selfCheck in the editor.
    eventPrefix = "FocusKeyPR";
  }
  else {
    // This happens, e.g., when interacting with the menus.
    // Focus in menus is a bit weird.
    eventPrefix = stringb("KeyPress " << quoted(qObjectPath(receiver)));
  }
  this->recordEvent(stringb(
    eventPrefix << " " << quoted(keysString(*keyEvent)) <<
    " " << quoted(keyEvent->text())));
}


void EventRecorder::recordOrdinaryKeyPress(char c)
{
  m_ordinaryKeyChars << c;
}


void EventRecorder::flushOrdinaryKeyChars()
{
  if (!m_ordinaryKeyChars.isempty()) {
    // Extract the string first, since 'recordEvent' also flushes.
    string keys = m_ordinaryKeyChars.str();
    m_ordinaryKeyChars.clear();

    this->recordEvent(stringb("FocusKeySequence " << quoted(keys)));
  }
}


void EventRecorder::recordEvent(string const &ev)
{
  this->flushOrdinaryKeyChars();
  m_outStream << ev << endl;
}


// EOF
