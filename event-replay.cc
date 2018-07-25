// event-replay.cc
// code for event-replay.h

#include "event-replay.h"              // this module

// editor
#include "debug-values.h"              // DEBUG_VALUES

// smqtutil
#include "qtguiutil.h"                 // getKeyPressFromString
#include "qtutil.h"                    // operator<<(QString)
#include "timer-event-loop.h"          // sleepWhilePumpingEvents

// smbase
#include "strutil.h"                   // parseQuotedString, quoted
#include "syserr.h"                    // xsyserror
#include "trace.h"                     // TRACE

// Qt
#include <QAbstractEventDispatcher>
#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QCoreApplication>
#include <QKeyEvent>
#include <QLabel>
#include <QRegularExpression>
#include <QStringList>
#include <QWidget>

// libc++
#include <string>                      // std::string, getline
#include <typeinfo>                    // typeid

// libc
#include <stdlib.h>                    // getenv, atoi


int EventReplay::s_quiescenceEventType = 0;


EventReplay::QuiescenceEvent::QuiescenceEvent()
  : QEvent((QEvent::Type)s_quiescenceEventType)
{}

EventReplay::QuiescenceEvent::~QuiescenceEvent()
{}


EventReplay::EventReplay(string const &fname)
  : m_fname(fname),
    m_in(m_fname.c_str()),
    m_lineNumber(0),
    m_testResult(),
    m_eventLoop(),
    m_eventReplayDelayMS(0),
    m_timerId(0),
    m_sleeping(false)
{
  if (m_in.fail()) {
    // Unfortunately there is no portable way to get the error cause
    // when using std::ifstream.
    throw xBase(stringb("failed to open " << quoted(m_fname)));
  }

  if (s_quiescenceEventType == 0) {
    s_quiescenceEventType = QEvent::registerEventType();
    TRACE("EventReplay", DEBUG_VALUES(s_quiescenceEventType));
    xassert(s_quiescenceEventType > 0);
  }

  if (char const *d = getenv("REPLAY_DELAY_MS")) {
    // Use the timer instead of the 'aboutToBlock' signal.
    m_eventReplayDelayMS = atoi(d);
  }
}


EventReplay::~EventReplay()
{
  // See doc/signals-and-dtors.txt.
  //
  // It is possible we already disconnected, or never connected in the
  // first place, but this is still safe.
  QObject::disconnect(QAbstractEventDispatcher::instance(), NULL,
                      this, NULL);
}


// Throw a string built using 'stringb'.
#define xstringb(msg) throw stringb(msg).str() /* user ; */


// Change the size of the top-level window containing 'widget' so that
// the widget itself reaches the target size.  This assumes that the
// widget expands or contracts the same amount as its parent windows.
static void resizeChildWidget(QWidget *widget, QSize const &targetSize)
{
  // How much should the target widget's size change?
  QSize currentSize = widget->size();
  QSize deltaSize = targetSize - currentSize;

  // Get the top-level window.
  QWidget *window = widget->window();

  // Change window size.
  QSize windowSize = window->size();
  windowSize += deltaSize;
  window->resize(windowSize);

  // Let the resize event be fully processed so the target widget can
  // reach its final size.
  QCoreApplication::processEvents();

  // Check that we got the intended size.
  if (widget->size() != targetSize) {
    xstringb("widget " << qObjectPath(widget) << ": size was " <<
             toString(currentSize) << ", tried to resize to " <<
             toString(targetSize) << ", but instead its size became " <<
             toString(widget->size()));
  }
}


// Get an object from its path from a top-level window, or throw a
// string if we cannot find it.
static QObject *getQObjectFromPath(string const &path)
{
  QStringList elts = toQString(path).split('.');
  if (elts.isEmpty()) {
    throw string("empty path");
  }

  try {
    Q_FOREACH (QObject *object, QApplication::topLevelWidgets()) {
      if (object->objectName() == elts.at(0)) {
        for (int i=1; i < elts.count(); i++) {
          QString elt(elts.at(i));
          if (elt.isEmpty()) {
            xstringb("empty path element " << (i+1));
          }
          object = object->findChild<QObject*>(elt,
                                               Qt::FindDirectChildrenOnly);
          if (!object) {
            xstringb("could not find child " << quoted(elt) <<
                     " at path element " << (i+1));
          }
        }
        return object;
      }
    }

    xstringb("could not find root element " << quoted(elts.at(0)));
  }
  catch (string const &msg) {
    xstringb("in path " << quoted(path) << ": " << msg);
  }
}


// Get a named object with a particular type using 'qobject_cast' to
// recognize it.
template <class T>
T *getObjectFromPath(string const &path)
{
  QObject *o = getQObjectFromPath(path);
  xassert(o);
  T *t = qobject_cast<T*>(o);
  if (!t) {
    xstringb("object at " << quoted(path) <<
             " has class " << o->metaObject()->className() <<
             ", not " << T::staticMetaObject.className());
  }
  return t;
}


// Get a named object with a particular type using 'dynamic_cast' to
// recognize it.
//
// I'm not really sure it makes sense to have both versions.  I started
// with qobject_cast, but for EventReplayQueryable I want to use
// ordinary dynamic_cast since I don't want to make it a QObject due to
// potential multiple inheritance issues.  Perhaps I should only use
// dynamic_cast?  Although getting readable class names using C++ RTTI
// is annoying.
template <class T>
T *getObjectFromPathDC(string const &path)
{
  QObject *o = getQObjectFromPath(path);
  xassert(o);
  T *t = dynamic_cast<T*>(o);
  if (!t) {
    xstringb("object at " << quoted(path) <<
             " has class " << typeid(*o).name() <<
             ", not " << typeid(T).name());
  }
  return t;
}


static EventReplayQueryable *getQueryableFromPath(string const &path)
{
  return getObjectFromPathDC<EventReplayQueryable>(path);
}


// Get the focus widget, throwing if there is none.
static QWidget *getFocusWidget()
{
  QWidget *widget = QApplication::focusWidget();
  if (!widget) {
    xstringb("No widget has focus.");
  }
  return widget;
}


// Get the string value of a quoted argument to a replay function.
static string getCapturedArg(QRegularExpressionMatch &match, int n)
{
  return parseQuotedString(toString(match.captured(n)));
}


// Complain unless 'numArgs==required'.
#define CHECK_NUM_ARGS(required)                           \
  if (numArgs != required) {                               \
    xstringb("incorrect number of arguments to " <<        \
             funcName << "; " << numArgs <<                \
             " passed but " << required << " required");   \
  }                                                        \
  else ((void)0) /* user ; */


// Bind 'arg1' to the single expected argument of a replay function.
#define BIND_ARGS1(arg1)                              \
  CHECK_NUM_ARGS(1);                                  \
  string arg1 = getCapturedArg(match, 2) /* user ; */

#define BIND_ARGS2(arg1, arg2)                        \
  CHECK_NUM_ARGS(2);                                  \
  string arg1 = getCapturedArg(match, 2);             \
  string arg2 = getCapturedArg(match, 3) /* user ; */

#define BIND_ARGS3(arg1, arg2, arg3)                  \
  CHECK_NUM_ARGS(3);                                  \
  string arg1 = getCapturedArg(match, 2);             \
  string arg2 = getCapturedArg(match, 3);             \
  string arg3 = getCapturedArg(match, 4) /* user ; */



#define EXPECT_EQ(context)                            \
  if (actual != expect) {                             \
    xstringb(context << ": should have been " <<      \
      quoted(expect) << " but was " <<                \
      quoted(actual) << ".");                         \
  }


void EventReplay::replayCall(QRegularExpressionMatch &match)
{
  QString funcName(match.captured(1));
  int numArgs = match.lastCapturedIndex() - 1;

  // --------------- events/actions ---------------
  if (funcName == "KeyPress") {
    BIND_ARGS3(receiver, keys, text);

    QCoreApplication::postEvent(
      getQObjectFromPath(receiver),
      getKeyPressEventFromString(keys, toQString(text)));
  }

  else if (funcName == "FocusKeyPress") {
    BIND_ARGS2(keys, text);

    QCoreApplication::postEvent(
      getFocusWidget(),
      getKeyPressEventFromString(keys, toQString(text)));
  }

  else if (funcName == "FocusKeyRelease") {
    BIND_ARGS2(keys, text);

    QCoreApplication::postEvent(
      getFocusWidget(),
      getKeyReleaseEventFromString(keys, toQString(text)));
  }

  else if (funcName == "Shortcut") {
    BIND_ARGS2(receiver, keys);

    QCoreApplication::postEvent(
      getQObjectFromPath(receiver),
      getShortcutEventFromString(keys));
  }

  else if (funcName == "ResizeEvent") {
    BIND_ARGS2(receiver, size);

    resizeChildWidget(
      getObjectFromPath<QWidget>(receiver),
      qSizeFromString(size));
  }

  else if (funcName == "TriggerAction") {
    BIND_ARGS1(path);

    QAction *action = getObjectFromPath<QAction>(path);
    action->trigger();
  }

  else if (funcName == "Sleep") {
    BIND_ARGS1(duration);

    int ms = atoi(duration.c_str());
    TRACE("EventReplay", "sleeping for " << ms << " ms");
    RESTORER(bool, m_sleeping, true);
    sleepWhilePumpingEvents(ms);
    TRACE("EventReplay", "done sleeping");
  }

  // -------------------- checks --------------------
  else if (funcName == "CheckQuery") {
    BIND_ARGS3(receiver, state, expect);

    EventReplayQueryable *q = getQueryableFromPath(receiver);
    string actual = q->eventReplayQuery(state);
    EXPECT_EQ("CheckQuery " << quoted(receiver) << ' ' << quoted(state));
  }

  else if (funcName == "CheckLabel") {
    BIND_ARGS2(path, expect);

    QLabel *label = getObjectFromPath<QLabel>(path);
    string actual = toString(label->text());
    EXPECT_EQ("CheckLabel " << quoted(path));
  }

  else if (funcName == "CheckComboBoxText") {
    BIND_ARGS2(path, expect);

    QComboBox *cbox = getObjectFromPath<QComboBox>(path);
    string actual = toString(cbox->currentText());
    EXPECT_EQ("CheckComboBoxText " << quoted(path));
  }

  else if (funcName == "CheckClipboard") {
    BIND_ARGS1(expect);

    string actual = toString(QApplication::clipboard()->text());
    EXPECT_EQ("CheckClipboard");
  }

  else if (funcName == "CheckActionChecked") {
    BIND_ARGS2(path, expect);

    QAction *action = getObjectFromPath<QAction>(path);
    string actual = (action->isChecked()? "true" : "false");
    EXPECT_EQ("CheckActionChecked " << quoted(path));
  }

  else if (funcName == "CheckFocusWindowTitle") {
    BIND_ARGS1(expect);

    string actual = toString(getFocusWidget()->window()->windowTitle());
    EXPECT_EQ("CheckFocusWindowTitle");
  }

  else if (funcName == "CheckFocusWindow") {
    BIND_ARGS1(expect);

    string actual = qObjectPath(getFocusWidget()->window());
    EXPECT_EQ("CheckFocusWindow");
  }

  else if (funcName == "CheckFocusWidget") {
    BIND_ARGS1(expect);

    string actual = qObjectPath(getFocusWidget());
    EXPECT_EQ("CheckFocusWidget");
  }

  else {
    xstringb("unrecognized function: " << quoted(funcName));
  }
}


bool EventReplay::replayNextEvent()
{
  QRegularExpression reBlank("^\\s*(#.*)?$");

  QRegularExpression reCall(
    "^\\s*(\\w+)"                      // 1: Function name
    "(?:\\s+(\"[^\"]*\"))?"            // 2: Quoted optional arg 1
    "(?:\\s+(\"[^\"]*\"))?"            // etc.
    "(?:\\s+(\"[^\"]*\"))?"
    "\\s*$");

  try {
    // Loop in order to skip comments and blank lines.
    while (!m_in.eof()) {
      // Read the next line.
      m_lineNumber++;
      std::string line;
      getline(m_in, line);
      if (m_in.eof()) {
        return false;
      }
      if (m_in.fail()) {
        // There is no portable way to get an error reason here.
        xstringb("reading the next line failed");
      }
      QString qline(line.c_str());

      // Skip comments and blank lines.
      if (reBlank.match(qline).hasMatch()) {
        continue;
      }

      // Match call lines.
      QRegularExpressionMatch match(reCall.match(qline));
      if (match.hasMatch()) {
        TRACE("EventReplay", "replaying: " << line);
        this->replayCall(match);
        return true;
      }

      xstringb("unrecognized line: \"" << qline << "\"");
    }
  }
  catch (string const &msg) {
    m_testResult = stringb(m_fname << ':' << m_lineNumber << ": " << msg);
  }
  catch (xBase &x) {
    m_testResult = stringb(m_fname << ':' << m_lineNumber << ": " << x.why());
  }
  catch (...) {
    m_testResult = "caught unknown exception!";
  }

  // EOF, or test failed.
  return false;
}


void EventReplay::postQuiescenceEvent()
{
  QCoreApplication::postEvent(this, new QuiescenceEvent());
}


void EventReplay::installTimer()
{
  xassert(m_eventReplayDelayMS);
  TRACE("EventReplay", "starting timer");

  this->killTimerIf();
  m_timerId = this->startTimer(m_eventReplayDelayMS);
  xassert(m_timerId != 0);
}


void EventReplay::killTimerIf()
{
  if (m_timerId != 0) {
    TRACE("EventReplay", "killing timer");
    this->killTimer(m_timerId);
    m_timerId = 0;
  }
}


string EventReplay::runTest()
{
  if (m_eventReplayDelayMS) {
    // Use timer-based notification.
    TRACE("EventReplay", "installing first timer");
    this->installTimer();
  }
  else {
    // Arrange to get notified just before the event dispatcher yields
    // control to the OS.  See doc/event-replay.txt.
    TRACE("EventReplay", "connecting slot_aboutToBlock");
    QObject::connect(QAbstractEventDispatcher::instance(),
                     &QAbstractEventDispatcher::aboutToBlock,
                     this, &EventReplay::slot_aboutToBlock);
  }

  // Process events until the test completes.
  //
  // NOTE: This is simply the substitute for the top-level application
  // event loop.  While the test runs, the app may start other event
  // loops, e.g., for modal dialogs.  Thus, it would not work to simply
  // unpack this loop and replay events here.
  TRACE("EventReplay", "runTest starting top-level event loop");
  m_eventLoop.exec();

  TRACE("EventReplay", "runTest finished; result: " << m_testResult);
  return m_testResult;
}


bool EventReplay::callReplayNextEvent()
{
  bool ret = this->replayNextEvent();

  if (!ret) {
    // Test is complete (pass or fail).  Stop the event loop we started
    // in 'runTest'.  But note that that might not be the innermost
    // event loop at the moment, e.g., if we're running a modal dialog.
    TRACE("EventReplay", "test complete, stopping "
      "replay event loop; result: " << m_testResult);
    m_eventLoop.exit(0);
  }

  TRACE("EventReplay", "callReplayNextEvent returning " << ret);
  return ret;
}


bool EventReplay::event(QEvent *ev)
{
  if (ev->type() == s_quiescenceEventType) {
    TRACE("EventReplay", "received QuiescenceEvent");
    if (this->callReplayNextEvent()) {
      // Test is continuing.  We don't have to do anything to keep
      // receiving 'aboutToBlock' and hence enqueueing QuiescenceEvents.
    }
    else {
      // Disconnect from the event dispatcher to stop getting signals.
      TRACE("EventReplay", "disconnecting slot_aboutToBlock");
      QObject::disconnect(QAbstractEventDispatcher::instance(), NULL,
                          this, NULL);
    }

    return true;
  }

  if (ev->type() == QEvent::Timer) {
    TRACE("EventReplay", "received TimerEvent");
    this->killTimerIf();

    // Post the next event.
    if (this->callReplayNextEvent()) {
      // Test is continuing.  Arrange to receive another event.
      this->installTimer();
    }
    else {
      // The timer has been killed so we will not get any more events.
      TRACE("EventReplay", "refraining from installing another timer");
    }

    return true;
  }

  // This object should only be receiving TimerEvents, and I don't
  // think QObject::event does anything (besides return false), but I
  // will do this just for overall cleanliness.
  return QObject::event(ev);
}


void EventReplay::slot_aboutToBlock() NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  if (m_sleeping) {
    // Ignore the quiescence, and don't print anything so we don't spam
    // the log.
    return;
  }

  TRACE("EventReplay", "in slot_aboutToBlock");

  if (m_in.eof()) {
    // This should not happen since, once EOF is hit, we disconnect this
    // slot, but I will be defensive here.
    TRACE("EventReplay", "we already hit EOF in the input file");
    return;
  }

  // Getting here means the application really is quiescent; if we did
  // not do this, the app would block waiting for some external event.
  // So, post an event that will trigger the next event to replay.
  this->postQuiescenceEvent();

  // At this point, we're about to return to the site of a call to
  // WaitForMultipleObjectsEx (or poll/select on unix) and we know the
  // OS event queue is empty.  In this state, WaitFor will block, but we
  // want to keep running in order to process the QuiescenceEvent we
  // just posted to the Qt event queue.  So, tell the dispatcher to wake
  // up.  That posts a message to the OS event queue which will ensure
  // that WaitFor returns immediately.  Then, control will cycle through
  // the usual event dispatch steps, including handling the
  // QuiescenceEvent, before eventually returning to 'aboutToBlock' once
  // the app is again quiescent.
  QAbstractEventDispatcher::instance()->wakeUp();

  GENERIC_CATCH_END
}


// EOF
