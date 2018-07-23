// event-replay.cc
// code for event-replay.h

#include "event-replay.h"              // this module

// editor
#include "debug-values.h"              // DEBUG_VALUES

// smqtutil
#include "qtguiutil.h"                 // getKeyPressFromString
#include "qtutil.h"                    // operator<<(QString)

// smbase
#include "strutil.h"                   // parseQuotedString, quoted
#include "syserr.h"                    // xsyserror
#include "trace.h"                     // TRACE

// Qt
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
    m_timerId(0)
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
    m_eventReplayDelayMS = atoi(d);
  }
}


EventReplay::~EventReplay()
{}


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
  QWidget *parent = widget;
  while (parent->parentWidget()) {
    parent = parent->parentWidget();
  }

  // Change window size.
  QSize windowSize = parent->size();
  windowSize += deltaSize;
  parent->resize(windowSize);

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

  if (funcName == "KeyPress") {
    BIND_ARGS3(receiver, keys, text);

    QCoreApplication::postEvent(
      getQObjectFromPath(receiver),
      getKeyPressEventFromString(keys, toQString(text)));
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
        // Although C++ does not guarantee this, a failure of
        // istream::read is almost certainly due to the 'read' system
        // call, and the reason conveyed in 'errno'.
        xsyserror("read");
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
  TRACE("EventReplay", "posting new QuiescenceEvent");

  // Post a very-low-priority event.  The idea is to let the app reach a
  // nearly quiescent state before we replay the next event.
  //
  // But note: it might be tempting here to simply enqueue all of the
  // events to be replayed at similarly low priority.  That does not
  // work because the receiver object lookup requires the app to be in
  // the same state it was when the event was recorded.
  QCoreApplication::postEvent(this, new QuiescenceEvent(), -10000);
}


void EventReplay::postFutureEvent()
{
  if (m_eventReplayDelayMS) {
    TRACE("EventReplay", "starting timer");

    this->killTimerIf();
    m_timerId = this->startTimer(m_eventReplayDelayMS);
    xassert(m_timerId != 0);
  }
  else {
    this->postQuiescenceEvent();
  }
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
  // Arrange to receive an event in the future.
  this->postFutureEvent();

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


bool EventReplay::event(QEvent *ev)
{
  if (ev->type() == s_quiescenceEventType ||
      ev->type() == QEvent::Timer) {
    this->killTimerIf();

    // Completely drain the event queue.
    //
    // Even though this event has very low priority, something else
    // could be even lower, and/or there could be another mechanism
    // somewhere playing the same game of constantly posting a
    // low-priority event, so we need to explicitly and completely drain
    // it to be sure to reach quiescence.
    TRACE("EventReplay", "draining queue");
    QCoreApplication::processEvents();
    TRACE("EventReplay", "queue is now empty");

    // Post the next event.
    if (!this->replayNextEvent()) {
      // Test is complete (pass or fail).  Stop the event loop.
      TRACE("EventReplay", "test complete, stopping replay event loop; "
        "result: " << m_testResult);
      m_eventLoop.exit(0);
    }
    else {
      // Test is continuing.  Arrange to receive another event.
      this->postFutureEvent();
    }

    return true;
  }

  // This object should only be receiving QuiescenceEvents, and I don't
  // think QObject::event does anything (besides return false), but I
  // will do this just for overall cleanliness.
  return QObject::event(ev);
}


// EOF
