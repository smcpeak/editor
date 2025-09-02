// event-replay.cc
// code for event-replay.h

#include "event-replay.h"              // this module

// NOTE: There should not be any dependencies here on the editor per se.
// If you want to, say, query an EditorWidget from a test script, use
// the "CheckQuery" generic query.

// editor
#include "debug-values.h"              // DEBUG_VALUES
#include "waiting-counter.h"           // g_waitingCounter, IncDecWaitingCounter

// smqtutil
#include "smqtutil/gdvalue-qt.h"       // gdvpTo(QSize)
#include "smqtutil/qstringb.h"         // qstringb
#include "smqtutil/qtguiutil.h"        // getKeyPressFromString
#include "smqtutil/qtutil.h"           // operator<<(QString)
#include "smqtutil/timer-event-loop.h" // sleepWhilePumpingEvents

// smbase
#include "smbase/c-string-reader.h"    // parseQuotedCString
#include "smbase/exc.h"                // smbase::{XBase, XMessage}, EXN_CONTEXT
#include "smbase/gdvalue-parser.h"     // gdv::{GDValueParser, gdvpTo}
#include "smbase/gdvalue.h"            // gdv::GDValue
#include "smbase/overflow.h"           // safeToInt
#include "smbase/nonport.h"            // getMilliseconds, getFileModificationTime
#include "smbase/sm-file-util.h"       // SMFileUtil
#include "smbase/sm-platform.h"        // PLATFORM_IS_WINDOWS
#include "smbase/string-util.h"        // doubleQuote
#include "smbase/stringb.h"            // stringb
#include "smbase/syserr.h"             // xsyserror
#include "smbase/trace.h"              // TRACE

// Qt
#include <QAbstractButton>
#include <QAbstractEventDispatcher>
#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QCoreApplication>
#include <QImage>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QStringList>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTimer>
#include <QWidget>

// libc++
#include <fstream>                     // std::ofstream
#include <optional>                    // std::optional
#include <regex>                       // std::regex_search
#include <string>                      // std::string, getline
#include <typeinfo>                    // typeid

// libc
#include <stdlib.h>                    // getenv, atoi

using namespace gdv;
using namespace smbase;


// -------------------- EventReplayQueryable --------------------
string EventReplayQueryable::eventReplayQuery(string const &state)
{
  return stringb("unknown state: " << doubleQuote(state));
}

QImage EventReplayQueryable::eventReplayImage(string const &)
{
  return QImage();
}

bool EventReplayQueryable::wantResizeEventsRecorded()
{
  return false;
}


// ------------------------ EventReplay -------------------------
int EventReplay::s_quiescenceEventType = 0;


EventReplay::QuiescenceEvent::QuiescenceEvent()
  : QEvent((QEvent::Type)s_quiescenceEventType)
{}

EventReplay::QuiescenceEvent::~QuiescenceEvent()
{}


EventReplay::EventReplay(string const &fname)
  : m_fname(fname),
    m_in(m_fname.c_str()),
    m_gdvalueReader(m_in, fname),
    m_queuedFocusKeySequence(),
    m_testResult(),
    m_eventLoop(),
    m_eventReplayDelayMS(0),
    m_timerId(0)
{
  if (m_in.fail()) {
    // Unfortunately there is no portable way to get the error cause
    // when using std::ifstream.
    throw XMessage(stringb("failed to open " << doubleQuote(m_fname)));
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
#define xstringb(msg) throw stringb(msg) /* user ; */


void EventReplay::resizeChildWidget(QWidget *widget, QSize const &targetSize)
{
  // How much should the target widget's size change?
  QSize currentSize = widget->size();
  QSize deltaSize = targetSize - currentSize;

  // Get the top-level window.
  QWidget *window = widget->window();

  // Compute desired window size.
  QSize windowSize = window->size();
  windowSize += deltaSize;

  // Check against its minimum.  On Linux when displaying on Windows via
  // X, the main window can end up with a fairly large minimum
  // horizontal size, depending on the window title text.  At least for
  // now, I will just override it during event replay.
  QSize curMinSize = window->minimumSize();
  QSize newMinSize = curMinSize.boundedTo(windowSize);
  if (newMinSize != curMinSize) {
    // Update: It seems that, if I have to change the minimum size, the
    // resize gets reverted quickly afterward, causing a race condition
    // in tests where that happens.  It seems I have to instead make
    // sure my tests never go below the minimum.
    //TRACE("EventReplay", "changing min size from " << toString(curMinSize) <<
    //                     " to " << toString(newMinSize));
    //window->setMinimumSize(newMinSize);
    xstringb("Cannot resize widget to " << toString(targetSize) <<
             " because that would require resizing the window to " <<
             toString(windowSize) <<
             ", which violates the minimum size of " <<
             toString(curMinSize) << ".");
  }

  // Now actually change the size.
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
            xstringb("could not find child " << doubleQuote(elt) <<
                     " at path element " << (i+1));
          }
        }
        return object;
      }
    }

    xstringb("could not find root element " << doubleQuote(elts.at(0)));
  }
  catch (string const &msg) {
    xstringb("in path " << doubleQuote(path) << ": " << msg);
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
    xstringb("object at " << doubleQuote(path) <<
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
    xstringb("object at " << doubleQuote(path) <<
             " has class " << typeid(*o).name() <<
             ", not " << typeid(T).name());
  }
  return t;
}


static EventReplayQueryable *getQueryableFromPath(string const &path)
{
  return getObjectFromPathDC<EventReplayQueryable>(path);
}


QWidget *EventReplay::getFocusWidget(QString const &funcName)
{
  QWidget *focusWidget = QApplication::focusWidget();
  if (!focusWidget) {
    xstringb("No widget has focus.");
  }

  TRACE("EventReplay", toString(funcName) << ": focusWidget: " <<
        doubleQuote(focusWidget->objectName()));

  return focusWidget;
}


// Return true if a substring of 'str' matches 're'.
static bool regexSearch(string const &str, string const &re)
{
  std::regex regex(re.c_str(), std::regex::ECMAScript);
  return std::regex_search(str.c_str(), regex);
}


static int intFromString(string const &s)
{
  return atoi(s.c_str());
}


static string getListWidgetContents(QListWidget *listWidget)
{
  std::vector<std::string> items;
  for (int i=0; i < listWidget->count(); ++i) {
    items.push_back(toString(listWidget->item(i)->text()));
  }
  return join(suffixAll(items, "\n"), "");
}


// Complain unless 'numArgs==required'.
#define CHECK_NUM_ARGS(required)                           \
  if (numArgs != required) {                               \
    xstringb("incorrect number of arguments to " <<        \
             funcName << "; " << numArgs <<                \
             " passed but " << required << " required");   \
  }                                                        \
  else ((void)0) /* user ; */


// Return 0-based argument `n` as a string.
#define GET_STRING_ARG(n) \
  parser.tupleGetValueAt(n).stringGet()


// Bind `arg1` to a (parser for) the single argument of a replay
// function.
#define BIND_GDVALUE_ARGS1(arg1)                              \
  CHECK_NUM_ARGS(1);                                          \
  GDValueParser arg1 = parser.tupleGetValueAt(0) /* user ; */

#define BIND_GDVALUE_ARGS2(arg1, arg2)                        \
  CHECK_NUM_ARGS(2);                                          \
  GDValueParser arg1 = parser.tupleGetValueAt(0);             \
  GDValueParser arg2 = parser.tupleGetValueAt(1) /* user ; */

#define BIND_GDVALUE_ARGS4(arg1, arg2, arg3, arg4)            \
  CHECK_NUM_ARGS(4);                                          \
  GDValueParser arg1 = parser.tupleGetValueAt(0);             \
  GDValueParser arg2 = parser.tupleGetValueAt(1);             \
  GDValueParser arg3 = parser.tupleGetValueAt(2);             \
  GDValueParser arg4 = parser.tupleGetValueAt(3) /* user ; */


// Bind 'arg1' to the single expected string argument of a replay
// function.
#define BIND_STRING_ARGS1(arg1)                \
  CHECK_NUM_ARGS(1);                           \
  string arg1 = GET_STRING_ARG(0) /* user ; */

#define BIND_STRING_ARGS2(arg1, arg2)          \
  CHECK_NUM_ARGS(2);                           \
  string arg1 = GET_STRING_ARG(0);             \
  string arg2 = GET_STRING_ARG(1) /* user ; */

#define BIND_STRING_ARGS3(arg1, arg2, arg3)    \
  CHECK_NUM_ARGS(3);                           \
  string arg1 = GET_STRING_ARG(0);             \
  string arg2 = GET_STRING_ARG(1);             \
  string arg3 = GET_STRING_ARG(2) /* user ; */

#define BIND_STRING_ARGS4(arg1, arg2, arg3, arg4) \
  CHECK_NUM_ARGS(4);                              \
  string arg1 = GET_STRING_ARG(0);                \
  string arg2 = GET_STRING_ARG(1);                \
  string arg3 = GET_STRING_ARG(2);                \
  string arg4 = GET_STRING_ARG(3) /* user ; */


#define CHECK_EQ(context)                        \
  if (actual != expect) {                        \
    xstringb(context << ": should have been " << \
      toGDValue(expect) << " but was " <<        \
      toGDValue(actual) << ".");                 \
  }

#define CHECK_RE_MATCH(context)                             \
  if (!regexSearch(actual, expectRE)) {                     \
    xstringb(context << ": the actual string " <<           \
      doubleQuote(actual) << " did not match the regex " << \
      doubleQuote(expectRE) << ".");                        \
  }


void EventReplay::replayCall(GDValue const &command)
{
  GDValueParser parser(command);
  parser.checkIsTuple();
  QString const funcName(toQString(parser.taggedContainerGetTagName()));
  int const numArgs = safeToInt(parser.containerSize());

  // --------------- events/actions ---------------
  if (funcName == "KeyPress") {
    BIND_STRING_ARGS3(receiver, keys, text);

    QCoreApplication::postEvent(
      getQObjectFromPath(receiver),
      getKeyPressEventFromString(keys, toQString(text)));
  }

  else if (funcName == "FocusKeyPress") {
    BIND_STRING_ARGS2(keys, text);

    QCoreApplication::postEvent(
      getFocusWidget(funcName),
      getKeyPressEventFromString(keys, toQString(text)));
  }

  else if (funcName == "FocusKeyRelease") {
    BIND_STRING_ARGS2(keys, text);

    QCoreApplication::postEvent(
      getFocusWidget(funcName),
      getKeyReleaseEventFromString(keys, toQString(text)));
  }

  else if (funcName == "FocusKeyPR") {
    BIND_STRING_ARGS2(keys, text);

    QWidget *focusWidget = getFocusWidget(funcName);

    // It is not always safe to post multiple events since, in the real
    // execution, events could intervene or state (e.g., focus!) could
    // change.  But for a press-release sequence this should be safe.
    QCoreApplication::postEvent(
      focusWidget,
      getKeyPressEventFromString(keys, toQString(text)));
    QCoreApplication::postEvent(
      focusWidget,
      getKeyReleaseEventFromString(keys, toQString(text)));
  }

  else if (funcName == "FocusKeySequence") {
    BIND_STRING_ARGS1(keys);

    // Enqueue the keys in reverse order.
    for (int i = keys.length()-1; i >= 0; i--) {
      m_queuedFocusKeySequence.push(keys[i]);
    }

    // Return so 'replayNextEvent' will process them.
  }

  else if (funcName == "Shortcut") {
    BIND_STRING_ARGS2(receiver, keys);

    QCoreApplication::postEvent(
      getQObjectFromPath(receiver),
      getShortcutEventFromString(keys));
  }

  // I tried creating a "FocusShortcut", but it does not work
  // consistently.  So I'll have to stick with the more explicit
  // "Shortcut" action.

  else if (funcName == "SetFocus") {
    BIND_STRING_ARGS1(widget);

    // Give the named widget the focus.
    //
    // Normally, we would want this to happen as a consequence of the
    // recorded events, but for some reason, replaying a shortcut event
    // that utilizes a buddy control has no effect.  The event recorder
    // module automatically turns those into SetFocus events.
    getObjectFromPath<QWidget>(widget)->setFocus();
  }

  else if (funcName == "ResizeEvent") {
    BIND_GDVALUE_ARGS2(receiver, size);

    resizeChildWidget(
      getObjectFromPath<QWidget>(receiver.stringGet()),
      gdvpTo<QSize>(size));
  }

  else if (funcName == "TriggerAction") {
    BIND_STRING_ARGS1(path);

    QAction *action = getObjectFromPath<QAction>(path);
    action->trigger();
  }

  else if (funcName == "Sleep") {
    BIND_GDVALUE_ARGS1(duration);

    sleepForMS(duration.smallIntegerGet());
  }

  else if (funcName == "ClickButton") {
    BIND_STRING_ARGS1(path);

    QAbstractButton *button = getObjectFromPath<QAbstractButton>(path);

    // This is like 'button->click()', except we enqueue the action and
    // continue immediately.  If the effect of clicking the button is to
    // pop up a modal dialog, then 'click()' would wait for it to be
    // dismissed.
    QObject::connect(this, &EventReplay::signal_clickButton,
                     button, &QAbstractButton::click,
                     Qt::QueuedConnection);
    Q_EMIT signal_clickButton();
    QObject::disconnect(this, &EventReplay::signal_clickButton,
                        button, &QAbstractButton::click);
  }

  // -------------------- checks --------------------
  else if (funcName == "DumpObjectTree") {
    BIND_STRING_ARGS1(path);

    // This can be used during test development to get details about
    // what is inside, e.g., some Qt-provided dialog box.
    QWidget *widget = getObjectFromPath<QWidget>(path);
    widget->dumpObjectTree();
  }

  else if (funcName == "WaitUntilCheckQuery") {
    BIND_STRING_ARGS4(duration, receiver, state, expect);

    this->waitUntilCheckQuery(
      intFromString(duration), receiver, state, expect);
  }

  else if (funcName == "CheckQuery") {
    BIND_STRING_ARGS3(receiver, state, expect);

    EventReplayQueryable *q = getQueryableFromPath(receiver);
    string actual = q->eventReplayQuery(state);
    CHECK_EQ("CheckQuery " << doubleQuote(receiver) << ' ' <<
             doubleQuote(state));
  }

  else if (funcName == "CheckLabel") {
    BIND_STRING_ARGS2(path, expect);

    QLabel *label = getObjectFromPath<QLabel>(path);
    string actual = toString(label->text());
    CHECK_EQ("CheckLabel " << doubleQuote(path));
  }

  else if (funcName == "CheckLabelMatches") {
    BIND_STRING_ARGS2(path, expectRE);

    QLabel *label = getObjectFromPath<QLabel>(path);
    string actual = toString(label->text());
    CHECK_RE_MATCH("CheckLabelMatches " << doubleQuote(path));
  }

  else if (funcName == "CheckComboBoxText") {
    BIND_STRING_ARGS2(path, expect);

    QComboBox *cbox = getObjectFromPath<QComboBox>(path);
    string actual = toString(cbox->currentText());
    CHECK_EQ("CheckComboBoxText " << doubleQuote(path));
  }

  else if (funcName == "CheckLineEditText") {
    BIND_STRING_ARGS2(path, expect);

    QLineEdit *lineEdit = getObjectFromPath<QLineEdit>(path);
    string actual = toString(lineEdit->text());
    CHECK_EQ("CheckLineEditText " << doubleQuote(path));
  }

  else if (funcName == "CheckListWidgetCount") {
    BIND_GDVALUE_ARGS2(path, expectP);

    QListWidget *listWidget = getObjectFromPath<QListWidget>(path.stringGet());
    int expect = expectP.integerGetAs<int>();
    int actual = listWidget->count();
    CHECK_EQ("CheckListWidgetCount " << path.getValue());
  }

  else if (funcName == "CheckListWidgetContents") {
    BIND_STRING_ARGS2(path, expect);

    QListWidget *listWidget = getObjectFromPath<QListWidget>(path);
    string actual = getListWidgetContents(listWidget);
    CHECK_EQ("CheckListWidgetContents " << doubleQuote(path));
  }

  else if (funcName == "CheckListWidgetCurrentRow") {
    BIND_GDVALUE_ARGS2(path, expectP);

    QListWidget *listWidget = getObjectFromPath<QListWidget>(path.stringGet());
    int expect = expectP.integerGetAs<int>();
    int actual = listWidget->currentRow();
    CHECK_EQ("CheckListWidgetCurrentRow " << path.getValue());
  }

  else if (funcName == "CheckTableWidgetCellMatches") {
    BIND_GDVALUE_ARGS4(objPath, row, col, expectREP);

    QTableWidget *table = getObjectFromPath<QTableWidget>(objPath.stringGet());
    int r = row.integerGetAs<int>();
    int c = col.integerGetAs<int>();
    string expectRE = expectREP.stringGet();
    QTableWidgetItem *item = table->item(r, c);
    xassert(item);
    string actual = toString(item->text());
    CHECK_RE_MATCH("CheckTableWidgetCellMatches " << objPath.getValue() <<
                   " " << r << " " << c);
  }

  else if (funcName == "CheckMessageBoxDetailsText") {
    BIND_STRING_ARGS2(path, expect);

    QMessageBox *mb = getObjectFromPath<QMessageBox>(path);

    // It's tempting to navigate directly within the object tree here,
    // but most of the objects do not have names, and the exact
    // structure is an internal Qt implementation detail, so I will rely
    // on just calling `QMessageBox::detailedText()`.
    string actual = toString(mb->detailedText());
    CHECK_EQ("CheckMessageBoxDetailsText");
  }

  else if (funcName == "CheckClipboard") {
    BIND_STRING_ARGS1(expect);

    string actual = toString(QApplication::clipboard()->text());
    CHECK_EQ("CheckClipboard");
  }

  else if (funcName == "CheckActionChecked") {
    BIND_GDVALUE_ARGS2(path, expectP);

    QAction *action = getObjectFromPath<QAction>(path.stringGet());
    bool expect = expectP.boolGet();
    bool actual = action->isChecked();
    CHECK_EQ("CheckActionChecked " << path.getValue());
  }

  else if (funcName == "CheckFocusWindowTitle") {
    BIND_STRING_ARGS1(expect);

    checkFocusWorkaround();
    string actual =
      toString(getFocusWidget(funcName)->window()->windowTitle());
    CHECK_EQ("CheckFocusWindowTitle");
  }

  else if (funcName == "CheckFocusWindowTitleMatches") {
    BIND_STRING_ARGS1(expectRE);

    checkFocusWorkaround();
    string actual =
      toString(getFocusWidget(funcName)->window()->windowTitle());
    CHECK_RE_MATCH("CheckFocusWindowTitleMatches");
  }

  else if (funcName == "CheckWindowTitle") {
    BIND_STRING_ARGS2(path, expect);

    QWidget *widget = getObjectFromPath<QWidget>(path);
    string actual = toString(widget->window()->windowTitle());
    CHECK_EQ("CheckWindowTitle");
  }

  else if (funcName == "CheckFocusWindow") {
    BIND_STRING_ARGS1(expect);

    checkFocusWorkaround();
    string actual = qObjectPath(getFocusWidget(funcName)->window());
    CHECK_EQ("CheckFocusWindow");
  }

  else if (funcName == "CheckFocusWidget") {
    BIND_STRING_ARGS1(expect);

    checkFocusWorkaround();
    string actual = qObjectPath(getFocusWidget(funcName));
    CHECK_EQ("CheckFocusWidget");
  }

  else if (funcName == "CheckImage") {
    BIND_STRING_ARGS3(path, what, expectFname);

    QImage expectImage;
    if (!expectImage.load(toQString(expectFname), "PNG")) {
      xstringb("Failed to load screenshot image: " << expectFname);
    }

    EventReplayQueryable *q = getQueryableFromPath(path);
    QImage actualImage = q->eventReplayImage(what);

    if (actualImage != expectImage) {
      QString actualFname("failing-actual-image.png");
      if (!actualImage.save(actualFname, "PNG")) {
        xstringb("CheckImage: Does not match expected image " <<
                 expectFname << ".  Additionally, I "
                 "failed to save the actual image to " <<
                 actualFname);
      }
      else {
        xstringb("CheckImage: Does not match expected image " <<
                 expectFname << ".  Actual image "
                 "saved to " << actualFname);
      }
    }
  }

  else if (funcName == "CheckSize") {
    BIND_GDVALUE_ARGS2(path, expectP);

    QWidget *widget = getObjectFromPath<QWidget>(path.stringGet());
    QSize expect(gdvpTo<QSize>(expectP));
    QSize actual = widget->size();
    CHECK_EQ("CheckSize " << path.getValue());
  }

  else if (funcName == "TouchFile") {
    BIND_STRING_ARGS1(fname);

    std::int64_t beforeModUnixTime = 0;
    getFileModificationTime(fname.c_str(), beforeModUnixTime /*OUT*/);

    SMFileUtil sfu;
    sfu.touchFile(fname);

    std::int64_t afterModUnixTime = 0;
    getFileModificationTime(fname.c_str(), afterModUnixTime /*OUT*/);

    if (beforeModUnixTime == afterModUnixTime &&
        beforeModUnixTime != 0) {
      // The purpose of `TouchFile` is to get a file with a different
      // timestamp, but depending on which tests run in what order, it
      // could be that the file was modified so recently that this
      // "touch" did not affect it at the granularity we measure (one
      // second).  Therefore, sleep one second and try again.
      TRACE("EventReplay", "TouchFile: unchanged file modification time, sleeping...");
      sleepForMS(1000);

      sfu.touchFile(fname);

      // There's not much point in checking again.  If it worked, great.
      // If not, I don't want to sit in a loop, so just keep going.
    }
  }

  else {
    xstringb("unrecognized function: " << doubleQuote(funcName));
  }
}


void EventReplay::sleepForMS(int ms)
{
  TRACE("EventReplay", "sleeping for " << ms << " ms");
  IncDecWaitingCounter idwc;
  sleepWhilePumpingEvents(ms);
  TRACE("EventReplay", "done sleeping");
}


// Workaround for problem on Linux I haven't solved: wait a little
// before checking focus.
//
// The problem manifests with `file-open-dialog1.ev` (and this sleep
// disabled), among others.  We replay the Return keypress that should
// close the dialog, but if we immediately check focus, then no widget
// has focus.  I think there is a problem with how my event loops are
// nested.
//
// TODO: Debug and fix.
void EventReplay::checkFocusWorkaround()
{
  if (!PLATFORM_IS_WINDOWS) {
    sleepForMS(100);
  }
}


// Construct a key press or release event for typing 'c'.
static QKeyEvent *charToKeyEvent(QEvent::Type eventType, char c)
{
  Qt::KeyboardModifiers modifiers = Qt::NoModifier;

  // For the characters in use, they correspond to the Qt key codes,
  // except that lowercase letters are denoted using codes that are in
  // the uppercase ASCII range.
  int k = c;
  if ('a' <= k && k <= 'z') {
    k -= ('a' - 'A');
  }

  return new QKeyEvent(eventType, (Qt::Key)k, modifiers, qstringb(c));
}


void EventReplay::replayFocusKey(char c)
{
  QWidget *focusWidget = getFocusWidget("replayFocusKey");

  // As for 'FocusKeyPR', posting both events at once should be safe.
  QCoreApplication::postEvent(
    focusWidget,
    charToKeyEvent(QEvent::KeyPress, c));
  QCoreApplication::postEvent(
    focusWidget,
    charToKeyEvent(QEvent::KeyRelease, c));
}


bool EventReplay::replayNextEvent()
{
  try {
    // Process any queued focus keys first.
    if (!m_queuedFocusKeySequence.isEmpty()) {
      // Note that the error reporting in the 'catch' blocks is
      // appropriate since the file and line number are still correct
      // when we get here.
      this->replayFocusKey(m_queuedFocusKeySequence.pop());
      return true;
    }

    // Get the next command.
    m_gdvalueReader.skipWhitespaceAndComments();
    FileLineCol loc = m_gdvalueReader.getLocation();
    if (std::optional<GDValue> command = m_gdvalueReader.readNextValue()) {
      TRACE("EventReplay", "replaying: " << *command);

      // Use the location of `command` as context.  (We do not push it
      // until now because a parse error in `readNextValue` will already
      // include it as context.)
      EXN_CONTEXT(loc);

      this->replayCall(*command);
      return true;
    }
    else {
      // EOF.
      return false;
    }
  }
  catch (string const &msg) {
    m_testResult = msg;
  }
  catch (XBase &x) {
    m_testResult = x.getMessage();
  }
  catch (...) {
    m_testResult = "caught unknown exception!";
  }

  // Test failed.
  return false;
}


void EventReplay::waitUntilCheckQuery(
  long durationMS,
  string const &receiver,
  string const &state,
  string const &expect)
{
  long startMS = getMilliseconds();
  int checkCount = 0;

  TRACE("EventReplay", "waiting for up to " << durationMS << " ms");

  // Arrange to receive an event after 'durationMS'.  We do not directly
  // handle the event; rather, we use it to cause 'processEvents' to
  // return.
  QTimer timer;
  timer.start(durationMS);

  while (true) {
    checkCount++;
    EventReplayQueryable *q = getQueryableFromPath(receiver);
    string actual = q->eventReplayQuery(state);
    if (actual == expect) {
      break;
    }
    long elapsedMS = getMilliseconds() - startMS;
    long remainingMS = durationMS - elapsedMS;
    if (remainingMS <= 0) {
      xstringb("WaitUntilCheckQuery: Slept for " << elapsedMS <<
               " ms but value is " << doubleQuote(actual) << ", not " <<
               doubleQuote(expect));
    }

    // Wait for something to happen.  This does not busy-wait.
    IncDecWaitingCounter idwc;
    QCoreApplication::processEvents(QEventLoop::WaitForMoreEvents);

    // I tried using the 'processEvents' that accepts a timeout, but
    // that one appears to ignore the 'WaitForMoreEvents' flag.
  }

  long elapsedMS = getMilliseconds() - startMS;
  TRACE("EventReplay", "condition satisfied after " << elapsedMS <<
                       " ms and " << checkCount << " checks");
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
    // in 'runTest'.
    if (!m_testResult.empty()) {
      // Our event loop might not be the innermost event loop at the
      // moment, e.g., if we're running a modal dialog.  So, print the
      // result to the console now so the user can see that the test has
      // failed.  (Otherwise it looks like the test fails when the
      // dialog is closed, which is misleading.)
      cout << "test FAILED: " << m_testResult << endl;
    }
    TRACE("EventReplay", "test complete, stopping replay event loop");
    m_eventLoop.exit(0);
  }

  TRACE("EventReplay", "callReplayNextEvent returning " << ret);
  return ret;
}


bool EventReplay::event(QEvent *ev)
{
  if (ev->type() == s_quiescenceEventType) {
    TRACE("EventReplay", "received QuiescenceEvent");
    if (g_waitingCounter) {
      TRACE("EventReplay", "ignoring QuiescenceEvent because g_waitingCounter is " <<
                           g_waitingCounter);
    }
    else if (this->callReplayNextEvent()) {
      // Test is continuing.  We don't have to do anything to keep
      // receiving 'aboutToBlock' and hence enqueueing QuiescenceEvents.
    }
    else {
      // Disconnect from the event dispatcher to stop getting signals.
      TRACE("EventReplay", "disconnecting slot_aboutToBlock");
      QObject::disconnect(QAbstractEventDispatcher::instance(), NULL,
                          this, NULL);
    }

    TRACE("EventReplay", "finished with QuiescenceEvent");
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

  if (g_waitingCounter > 0) {
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
