// event-recorder.cc
// code for event-recorder.h

#include "event-recorder.h"            // this module

// editor
#include "event-replay.h"              // EventReplayQueryable

// smqtutil
#include "smqtutil/gdvalue-qt.h"       // toGDValue(QSize), toGDValue(QMouseEvent)
#include "smqtutil/qtguiutil.h"        // keysString(QKeyEvent)
#include "smqtutil/qtutil.h"           // operator<<(QString), qObjectPath, isModifierKey, isMouseEventType, toStringOpt(QEvent::Type)

// smbase
#include "smbase/dev-warning.h"        // DEV_WARNING
#include "smbase/gdvalue.h"            // gdv::GDValue
#include "smbase/sm-trace.h"           // INIT_TRACE, etc.
#include "smbase/string-util.h"        // doubleQuote
#include "smbase/syserr.h"             // smbase::xsyserror
#include "smbase/xassert.h"            // xassertPtr

// Qt
#include <QApplication>
#include <QCoreApplication>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QShortcutEvent>
#include <QWidget>

// libc
#include <string.h>                    // strcmp

using namespace gdv;
using namespace smbase;


INIT_TRACE("event-recorder");


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
      isMouseEventType(type) ||
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
        recordShortcutEvent(receiver, shortcutEvent);
      }
    }
    else if (isMouseEventType(type)) {
      if (QMouseEvent const *mouseEvent =
            dynamic_cast<QMouseEvent const *>(event)) {
        recordMouseEvent(receiver, mouseEvent);
      }
    }
    else if (type == QEvent::Resize) {
      if (QResizeEvent const *resizeEvent =
            dynamic_cast<QResizeEvent const *>(event)) {
        // Filter for widgets that want resize recorded.  (There are a
        // lot of resize events that flow through the system, and I only
        // care about a small fraction of them.)
        if (EventReplayQueryable *queryable =
              dynamic_cast<EventReplayQueryable*>(receiver)) {
          if (queryable->wantResizeEventsRecorded()) {
            this->recordEvent(GDVTaggedTuple("ResizeEvent"_sym, {
              qObjectPath(receiver),
              toGDValue(resizeEvent->size()),
            }));
          }
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
        this->recordEvent(GDVTaggedTuple("CheckFocusWidget"_sym, {
          qObjectPath(receiver),
        }));
      }
    }
    else {
      // Should not get here due to 'if' filter at top.
      DEV_WARNING("whoops");
    }
  }

  return false;
}



void EventRecorder::recordShortcutEvent(
  QObject *receiver, QShortcutEvent const *shortcutEvent)
{
  TRACE1_GDVN_EXPRS("recordShortcutEvent",
    shortcutEvent->key().toString(),
    shortcutEvent->shortcutId(),
    shortcutEvent->isAmbiguous());

  // I've fixed the problem this was meant to address on the replay
  // side, but I'll keep this code around in case a related problem
  // reappears.
  if (false) {
    if (QLabel *label = dynamic_cast<QLabel*>(receiver)) {
      TRACE1("Shortcut receiver is a label: " << label->objectName());
      if (QWidget *buddy = label->buddy()) {
        TRACE1("Its buddy is: " << buddy->objectName());

        // If we record this as a Shortcut event, it will not work (for
        // unknown reasons).  So turn it into SetFocus.
        this->recordEvent(GDVTaggedTuple("SetFocus"_sym, {
          qObjectPath(buddy),
        }));
        return;
      }
    }
  }

  this->recordEvent(GDVTaggedTuple("Shortcut"_sym, {
    qObjectPath(receiver),
    toString(shortcutEvent->key().toString()),  // ->QString->std::string
  }));
}


void EventRecorder::recordMouseEvent(
  QObject *receiver,
  QMouseEvent const *mouseEvent)
{
  QEvent::Type type = mouseEvent->type();
  if (type == QEvent::MouseMove &&
      mouseEvent->buttons() == Qt::NoButton) {
    // This is a mouse move without any button held, which is just
    // noise for my recorder.
    return;
  }

  std::string receiverPath = qObjectPath(receiver);
  TRACE1_GDVN_EXPRS("recordMouseEvent", receiverPath, *mouseEvent);

  // Use the event type as the "function name".
  GDVSymbol eventSym(xassertPtr(toStringOpt(type)));

  // Begin constructing the "function call" tuple.
  GDValue event(GDVK_TAGGED_TUPLE, eventSym);

  // Start with the receiver object.
  event.tupleAppend(receiverPath);

  // Always include the position.  This is just the "local" position;
  // the other coordinates can be calculated from this during replay.
  event.tupleAppend(toGDValue(mouseEvent->pos()));

  // Include the initiating button if it is not merely a movement.
  if (type != QEvent::MouseMove) {
    event.tupleAppend(toGDValue(mouseEvent->button()));
  }

  // Mouse buttons held *other than* `button()`, since recording that
  // one is redundant.
  Qt::MouseButtons const buttons =
    mouseEvent->buttons() & ~(mouseEvent->button());

  event.tupleAppend(toGDValue(buttons));

  // Keyboard modifier buttons.
  event.tupleAppend(toGDValue(mouseEvent->modifiers()));

  this->recordEvent(event);
}


// If 'keyEvent' is the press of a key that denotes itself in the
// ordinary way, return the denoted character.  Otherwise return 0.
//
// The purpose of this is to identify events we can safely and easily
// compress into a single string in order to make the event log more
// compact.  It is fine to be conservative about what is "ordinary".
static char isOrdinaryKeyPress(QKeyEvent const *keyEvent)
{
  // No modifiers.
  if (keyEvent->modifiers() == Qt::NoModifier) {
    // Require that the denoted text be a single character that agrees
    // with the key code in a simple way.  Among other things, this has
    // the effect of treating text typed with caps lock as not being
    // ordinary"
    QString text = keyEvent->text();
    if (text.length() == 1) {
      // Only retain characters in the printable ASCII range.
      int c = text[0].unicode();
      if (!( 0x20 <= c && c <= 0x7e )) {
        return 0;             // not ordinary
      }

      // Require that the character be the same as its key, except that
      // letters have their case inverted.
      int k = keyEvent->key();
      if ('A' <= k && k <= 'Z') {
        k += ('a' - 'A');
      }
      if (k == c) {
        return (char)c;       // ordinary
      }
    }
  }

  return 0;                   // not ordinary
}


void EventRecorder::recordKeyEvent(QObject *receiver, QKeyEvent const *keyEvent)
{
  if (isModifierKey(keyEvent->key())) {
    // Filter out keypresses for isolated modifiers.  They just
    // add noise to the recording.
    return;
  }

  GDValue event(GDVK_TAGGED_TUPLE);

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
    event.taggedContainerSetTag("FocusKeyPR"_sym);
  }
  else {
    // This happens, e.g., when interacting with the menus.
    // Focus in menus is a bit weird.
    event.taggedContainerSetTag("KeyPress"_sym);
    event.tupleAppend(qObjectPath(receiver));
  }

  event.tupleAppend(keysString(*keyEvent));
  event.tupleAppend(toString(keyEvent->text()));

  this->recordEvent(event);
}


void EventRecorder::recordOrdinaryKeyPress(char c)
{
  m_ordinaryKeyChars << c;
}


void EventRecorder::flushOrdinaryKeyChars()
{
  // Extract the string first, since 'recordEvent' also flushes.
  string keys = m_ordinaryKeyChars.str();
  if (!keys.empty()) {
    m_ordinaryKeyChars.str("");

    this->recordEvent(GDVTaggedTuple("FocusKeySequence"_sym, {
      keys,
    }));
  }
}


void EventRecorder::recordEvent(GDValue const &event)
{
  this->flushOrdinaryKeyChars();
  m_outStream << event << endl;
}


// EOF
