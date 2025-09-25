// event-replay.cc
// code for event-replay.h

#include "event-replay.h"                        // this module

// NOTE: There should not be any dependencies here on the editor per se.
// If you want to, say, query an EditorWidget from a test script, use
// the "CheckQuery" generic query.

// editor
#include "debug-values.h"                        // DEBUG_VALUES
#include "waiting-counter.h"                     // g_waitingCounter, IncDecWaitingCounter

// smqtutil
#include "smqtutil/gdvalue-qt.h"                 // gdvpTo(QSize)
#include "smqtutil/qstringb.h"                   // qstringb
#include "smqtutil/qtguiutil.h"                  // getKeyPressFromString, showRaiseAndActivateWindow
#include "smqtutil/qtutil.h"                     // operator<<(QString)
#include "smqtutil/timer-event-loop.h"           // sleepWhilePumpingEvents

// smbase
#include "smbase/c-string-reader.h"              // parseQuotedCString
#include "smbase/chained-cond.h"                 // smbase::cc::z_le_lt
#include "smbase/exc.h"                          // smbase::{XBase, XMessage}, EXN_CONTEXT
#include "smbase/gdvalue-parser.h"               // gdv::{GDValueParser, gdvpTo}
#include "smbase/gdvalue-tuple.h"                // gdv::gdvpToTuple
#include "smbase/gdvalue-vector.h"               // gdv::toGDValue(std::vector)
#include "smbase/gdvalue.h"                      // gdv::GDValue
#include "smbase/overflow.h"                     // safeToInt
#include "smbase/nonport.h"                      // getMilliseconds, getFileModificationTime
#include "smbase/run-process.h"                  // RunProcess
#include "smbase/sm-file-util.h"                 // SMFileUtil
#include "smbase/sm-is-equal.h"                  // smbase::is_equal
#include "smbase/sm-macros.h"                    // IMEMBFP
#include "smbase/sm-platform.h"                  // PLATFORM_IS_WINDOWS
#include "smbase/sm-trace.h"                     // INIT_TRACE, etc.
#include "smbase/string-util.h"                  // doubleQuote
#include "smbase/stringb.h"                      // stringb
#include "smbase/syserr.h"                       // xsyserror

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
#include <QModelIndex>
#include <QPushButton>
#include <QStringList>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTextEdit>
#include <QTimer>
#include <QWidget>

// libc++
#include <exception>                             // std::exception
#include <fstream>                               // std::ofstream
#include <functional>                            // std::function
#include <optional>                              // std::optional
#include <regex>                                 // std::regex_search
#include <string>                                // std::string, getline
#include <tuple>                                 // std::tuple
#include <typeinfo>                              // typeid

// libc
#include <stdlib.h>                              // getenv, atoi

using namespace gdv;
using namespace smbase;


INIT_TRACE("event-replay");


// -------------------- EventReplayQueryable --------------------
GDValue EventReplayQueryable::eventReplayQuery(string const &state)
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


EventReplay::EventReplay(
  std::vector<gdv::GDValue> const &testCommands,
  std::function<void ()> globalSelfCheck)
:
  IMEMBFP(testCommands),
  m_nextTestCommandIndex(0),
  m_queuedFocusKeySequence(),
  m_testResult(),
  m_eventLoop(),
  m_eventReplayDelayMS(0),
  m_timerId(0),
  IMEMBFP(globalSelfCheck)
{
  if (s_quiescenceEventType == 0) {
    s_quiescenceEventType = QEvent::registerEventType();
    TRACE2(DEBUG_VALUES(s_quiescenceEventType));
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


// Throw an `XMessage` exception built using `stringb`.
#define xmessagesb(msg) xmessage(stringb(msg))


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
    //TRACE1("changing min size from " << toString(curMinSize) <<
    //       " to " << toString(newMinSize));
    //window->setMinimumSize(newMinSize);
    xmessagesb("Cannot resize widget to " << toString(targetSize) <<
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
    xmessagesb("widget " << qObjectPath(widget) << ": size was " <<
               toString(currentSize) << ", tried to resize to " <<
               toString(targetSize) << ", but instead its size became " <<
               toString(widget->size()));
  }
}


// Get an object from its path from a top-level window, or throw
// `XMessage` if we cannot find it.
static QObject *getQObjectFromPath(string const &path)
{
  QStringList elts = toQString(path).split('.');
  if (elts.isEmpty()) {
    xmessagesb("empty path");
  }

  try {
    Q_FOREACH (QObject *object, QApplication::topLevelWidgets()) {
      if (object->objectName() == elts.at(0)) {
        for (int i=1; i < elts.count(); i++) {
          QString elt(elts.at(i));
          if (elt.isEmpty()) {
            xmessagesb("empty path element " << (i+1));
          }

          if (elt.at(0) == '#') {
            // This is an index conjured by my `qObjectPath` function.
            int const index = elt.mid(1).toInt();
            int const count = object->children().count();
            if (cc::z_le_lt(index, count)) {
              object = object->children().at(index);
            }
            else {
              xmessagesb("Invalid child index " << index <<
                         " for object with " << count <<
                         "children.");
            }
          }
          else{
            object = object->findChild<QObject*>(elt,
                                                 Qt::FindDirectChildrenOnly);
            if (!object) {
              xmessagesb("could not find child " << doubleQuote(elt) <<
                         " at path element " << (i+1));
            }
          }
        }
        return object;
      }
    }

    xmessagesb("could not find root element " << doubleQuote(elts.at(0)));
  }
  catch (XMessage &msg) {
    xmessagesb("in path " << doubleQuote(path) << ": " <<
               msg.getRelayMessage());
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
    xmessagesb("object at " << doubleQuote(path) <<
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
    xmessagesb("object at " << doubleQuote(path) <<
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
    xmessagesb("No widget has focus.");
  }

  TRACE2(toString(funcName) << ": focusWidget: " <<
         qObjectPath(focusWidget));

  return focusWidget;
}


// Return true if a substring of 'str' matches 're'.
static bool regexSearch(string const &str, string const &re)
{
  std::regex regex(re.c_str(), std::regex::ECMAScript);
  return std::regex_search(str.c_str(), regex);
}


static void checkRegexSearch(string const &actual, string const &expectRE)
{
  if (!regexSearch(actual, expectRE)) {
    xmessagesb("re match fail: " << GDVN_OMAP_EXPRS(actual, expectRE));
  }
}


static std::vector<std::string> getListWidgetContents(
  QListWidget *listWidget)
{
  std::vector<std::string> items;
  for (int i=0; i < listWidget->count(); ++i) {
    items.push_back(toString(listWidget->item(i)->text()));
  }
  return items;
}


template <typename AT, typename ET>
void checkEquality(
  char const *actualName,
  AT const &actual,
  ET const &expect)
{
  if (!is_equal(actual, expect)) {
    xmessagesb(actualName << ": mismatch: " <<
               GDVN_OMAP_EXPRS(actual, expect));
  }
}

#define CHECK_EQUALITY(actual, expect) \
  checkEquality(#actual, actual, expect)


// Check `actual` against `expect`, which can be either a string, for
// exact comparison, or a tuple like re("blah"), for regex match.
static void checkString(
  char const *actualName,
  std::string const &actual,
  GDValueParser const &expect)
{
  if (expect.isString()) {
    checkEquality(actualName, actual, expect.stringGet());
    return;
  }

  if (expect.isTaggedTupleSize("re", 1)) {
    std::string expectRE = expect.tupleGetValueAt(0).stringGet();
    checkRegexSearch(actual, expectRE);
    return;
  }

  xmessagesb("unrecognized match target: " << expect.getValue());
}


static std::string getTableWidgetCell(
  QTableWidget const *table,
  int row,
  int col)
{
  QTableWidgetItem *item = table->item(row, col);
  xassert(item);
  return toString(item->text());
}


static void checkTableWidgetCell(
  QTableWidget const *table,
  int row,
  int col,
  GDValueParser const &expect)
{
  EXN_CONTEXT_EXPR(col);
  checkString("cell text", getTableWidgetCell(table, row, col), expect);
}


static std::vector<std::string> getTableWidgetRow(
  QTableWidget const *table,
  int row)
{
  std::vector<std::string> ret;

  int const numColumns = table->columnCount();
  for (int col=0; col < numColumns; ++col) {
    ret.push_back(getTableWidgetCell(table, row, col));
  }

  return ret;
}


static void checkTableWidgetRow(
  QTableWidget const *table,
  int row,
  GDValueParser const &expect)
{
  EXN_CONTEXT_EXPR(row);

  int const numColumns = table->columnCount();
  CHECK_EQUALITY(numColumns, expect.sequenceSize());

  for (int col=0; col < numColumns; ++col) {
    checkTableWidgetCell(table, row, col,
      expect.sequenceGetValueAt(col));
  }
}


static std::vector<std::vector<std::string>> getTableWidgetContents(
  QTableWidget const *table)
{
  std::vector<std::vector<std::string>> ret;

  int const numRows = table->rowCount();

  for (int row=0; row < numRows; ++row) {
    ret.push_back(getTableWidgetRow(table, row));
  }

  return ret;
}


// TODO: Generalize this to a GDValue-vs-GDValue matcher.
static void checkTableWidgetContents(
  QTableWidget const *table,
  GDValueParser const &expect)
{
  EXN_CONTEXT("actual contents: " <<
              toGDValue(getTableWidgetContents(table)).asIndentedString());

  int const numRows = table->rowCount();
  CHECK_EQUALITY(numRows, expect.sequenceSize());

  for (int row=0; row < numRows; ++row) {
    checkTableWidgetRow(table, row,
      expect.sequenceGetValueAt(row));
  }
}


// TODO: Move to `smqtutil`.
static QPointF toQPointF(QPoint const &pt)
{
  return QPointF(pt.x(), pt.y());
}


// Complain unless 'numArgs==required'.
#define CHECK_NUM_ARGS(required)                           \
  if (numArgs != required) {                               \
    xmessagesb("incorrect number of arguments to " <<      \
               funcName << "; " << numArgs <<              \
               " passed but " << required << " required"); \
  }                                                        \
  else ((void)0) /* user ; */


// Return 0-based argument `n` as a string.
#define GET_STRING_ARG(n) \
  parser.tupleGetValueAt(n).stringGet()


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


#define CHECK_EQ(context)                          \
  if (actual != expect) {                          \
    xmessagesb(context << ": should have been " << \
      toGDValue(expect) << " but was " <<          \
      toGDValue(actual) << ".");                   \
  }

#define CHECK_RE_MATCH(context)                             \
  if (!regexSearch(actual, expectRE)) {                     \
    xmessagesb(context << ": the actual string " <<         \
      doubleQuote(actual) << " did not match the regex " << \
      doubleQuote(expectRE) << ".");                        \
  }


void EventReplay::replayCall(GDValue const &command)
{
  GDValueParser parser(command);
  parser.checkIsTuple();

  QString const funcName(toQString(parser.taggedContainerGetTagName()));
  EXN_CONTEXT_EXPR(funcName);

  int const numArgs = safeToInt(parser.containerSize());

  // ----------------------------- actions -----------------------------
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
    BIND_STRING_ARGS2(receiverPath, keys);

    replayShortcut(receiverPath, keys);
  }

  // I tried creating a "FocusShortcut", but it does not work
  // consistently.  So I'll have to stick with the more explicit
  // "Shortcut" action.

  else if (funcName == "SetFocus") {
    BIND_STRING_ARGS1(widget);

    /* Give the named widget the focus.

       Ordinarilly, we would let focus change in response to user input.
       This command was originally created as a workaround for a problem
       with buddy control focus changes.  I think I've solved that
       problem in the Shortcut event handler, but I will keep SetFocus
       as a potential diagnostic tool, and because some tests are still
       using it, which seems harmless.
    */
    getObjectFromPath<QWidget>(widget)->setFocus();
  }

  else if (funcName == "ActivateWindow") {
    auto [receiver] =
      gdvpToTuple<std::string>(parser);

    QWidget *widget = getObjectFromPath<QWidget>(receiver);
    showRaiseAndActivateWindow(widget);
  }

  else if (funcName == "ResizeEvent") {
    auto [receiver, size] =
      gdvpToTuple<std::string, QSize>(parser);

    resizeChildWidget(
      getObjectFromPath<QWidget>(receiver),
      size);
  }

  else if (funcName == "TriggerAction") {
    BIND_STRING_ARGS1(path);

    QAction *action = getObjectFromPath<QAction>(path);
    action->trigger();
  }

  else if (funcName == "Sleep") {
    auto [duration] =
      gdvpToTuple<int>(parser);

    sleepForMS(duration);
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

  else if (funcName == "MouseButtonPress" ||
           funcName == "MouseButtonRelease" ||
           funcName == "MouseButtonDblClick") {
    auto [receiver, pos, button, buttons, modifiers] =
      gdvpToTuple<std::string,
                  QPoint,
                  Qt::MouseButton,
                  Qt::MouseButtons,
                  Qt::KeyboardModifiers>(parser);

    // Adjust `buttons`, re-adding the redundant bit that was removed
    // during recording.
    if (funcName == "MouseButtonPress" ||
        funcName == "MouseButtonDblClick") {
      buttons |= button;
    }

    QMouseEvent *ev = new QMouseEvent(
      qtEnumeratorFromNameOpt<QEvent::Type>(toString(funcName)).value(),
      toQPointF(pos),
      button,
      buttons,
      modifiers);

    QApplication::postEvent(
      getObjectFromPath<QWidget>(receiver),
      ev /*ownership transfer*/);
  }

  else if (funcName == "MouseMove") {
    auto [receiver, pos, buttons, modifiers] =
      gdvpToTuple<std::string,
                  QPoint,
                  Qt::MouseButtons,
                  Qt::KeyboardModifiers>(parser);

    QMouseEvent *ev = new QMouseEvent(
      QEvent::MouseMove,
      toQPointF(pos),
      Qt::NoButton,
      buttons,
      modifiers);

    QApplication::postEvent(
      getObjectFromPath<QWidget>(receiver),
      ev /*ownership transfer*/);
  }

  // ----------------------------- checks ------------------------------
  else if (funcName == "DumpObjectTree") {
    BIND_STRING_ARGS1(path);

    // This can be used during test development to get details about
    // what is inside, e.g., some Qt-provided dialog box.
    QWidget *widget = getObjectFromPath<QWidget>(path);
    widget->dumpObjectTree();
  }

  else if (funcName == "WaitUntilCheckQuery") {
    auto [durationMS, receiver, state, expect] =
      gdvpToTuple<int, std::string, std::string, GDValue>(parser);

    this->waitUntilCheckQuery(
      durationMS, receiver, state, expect);
  }

  else if (funcName == "CheckQuery") {
    auto [receiver, state, expect] =
      gdvpToTuple<std::string, std::string, GDValue>(parser);

    // Provide the location of the expect string in case the
    // `CheckQuery` came from an included file.
    EXN_CONTEXT(expect.sourceLocation().asString());

    EventReplayQueryable *q = getQueryableFromPath(receiver);
    GDValue actual = q->eventReplayQuery(state);
    CHECK_EQ("CheckQuery " << doubleQuote(receiver) << ' ' <<
             doubleQuote(state));
  }

  else if (funcName == "CheckQueryMatches") {
    auto [receiver, state, expectRE] =
      gdvpToTuple<std::string, std::string, std::string>(parser);

    EventReplayQueryable *q = getQueryableFromPath(receiver);
    std::string actual = q->eventReplayQuery(state).stringGet();
    CHECK_RE_MATCH("CheckQueryMatches " << doubleQuote(receiver) << ' ' <<
                   doubleQuote(state));
  }

  else if (funcName == "CheckLabel") {
    BIND_STRING_ARGS2(path, expect);

    QLabel *label = getObjectFromPath<QLabel>(path);
    string actual = toString(label->text());
    CHECK_EQ("CheckLabel " << doubleQuote(path));
  }

  else if (funcName == "WaitUntilCheckLabel") {
    auto [durationMS, path, expect] =
      gdvpToTuple<int, std::string, GDValue>(parser);

    QLabel *label = getObjectFromPath<QLabel>(path);
    auto gdvFunc = [label]() -> GDValue {
      return toString(label->text());
    };
    waitUntilCheckGDValueFunction(durationMS, gdvFunc, expect);
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

  else if (funcName == "CheckTextEditText") {
    auto [path, expect] =
      gdvpToTuple<std::string, std::string>(parser);

    auto *textEdit = getObjectFromPath<QTextEdit>(path);
    string actual = toString(textEdit->toPlainText());
    CHECK_EQ("CheckTextEditText " << doubleQuote(path));
  }

  else if (funcName == "CheckListViewSelectedItem") {
    auto [path, expect] =
      gdvpToTuple<std::string, std::string>(parser);

    auto *listView = getObjectFromPath<QListView>(path);

    // https://stackoverflow.com/a/38772280
    QModelIndex index = listView->currentIndex();
    QString itemText = index.data(Qt::DisplayRole).toString();

    string actual = toString(itemText);
    CHECK_EQ("CheckListViewSelectedItem " << doubleQuote(path));
  }

  else if (funcName == "CheckListWidgetCount") {
    auto [path, expect] =
      gdvpToTuple<std::string, int>(parser);

    QListWidget *listWidget = getObjectFromPath<QListWidget>(path);
    int actual = listWidget->count();
    CHECK_EQ("CheckListWidgetCount " << doubleQuote(path));
  }

  else if (funcName == "CheckListWidgetContents") {
    auto [path, expect] =
      gdvpToTuple<std::string, std::vector<std::string>>(parser);

    QListWidget *listWidget = getObjectFromPath<QListWidget>(path);
    std::vector<std::string> actual = getListWidgetContents(listWidget);
    CHECK_EQ("CheckListWidgetContents " << doubleQuote(path));
  }

  else if (funcName == "CheckListWidgetCurrentRow") {
    auto [path, expect] =
      gdvpToTuple<std::string, int>(parser);

    QListWidget *listWidget = getObjectFromPath<QListWidget>(path);
    int actual = listWidget->currentRow();
    CHECK_EQ("CheckListWidgetCurrentRow " << doubleQuote(path));
  }

  else if (funcName == "CheckTableWidgetCurrentRow") {
    auto [path, expect] =
      gdvpToTuple<std::string, int>(parser);

    QTableWidget *tableWidget = getObjectFromPath<QTableWidget>(path);
    int actual = tableWidget->currentRow();
    CHECK_EQ("CheckTableWidgetCurrentRow " << doubleQuote(path));
  }

  else if (funcName == "CheckTableWidgetRowCount") {
    auto [objPath, expect] =
      gdvpToTuple<std::string, int>(parser);
    EXN_CONTEXT_EXPR(objPath);

    QTableWidget *table = getObjectFromPath<QTableWidget>(objPath);
    int actual = table->rowCount();
    CHECK_EQ("CheckTableWidgetRowCount " << doubleQuote(objPath));
  }

  else if (funcName == "CheckTableWidgetRow") {
    auto [objPath, r, expect] =
      gdvpToTuple<std::string, int, std::vector<std::string>>(parser);

    QTableWidget *table = getObjectFromPath<QTableWidget>(objPath);
    std::vector<std::string> actual = getTableWidgetRow(table, r);
    CHECK_EQ("CheckTableWidgetRow " << doubleQuote(objPath) <<
             " " << r);
  }

  else if (funcName == "CheckTableWidgetContents") {
    auto [objPath, expect] =
      gdvpToTuple<std::string, GDValue>(parser);
    EXN_CONTEXT_EXPR(objPath);

    QTableWidget *table = getObjectFromPath<QTableWidget>(objPath);
    checkTableWidgetContents(table, GDValueParser(expect));
  }

  else if (funcName == "CheckTableWidgetCellMatches") {
    auto [objPath, r, c, expectRE] =
      gdvpToTuple<std::string, int, int, std::string>(parser);

    QTableWidget *table = getObjectFromPath<QTableWidget>(objPath);
    string actual = getTableWidgetCell(table, r, c);
    CHECK_RE_MATCH("CheckTableWidgetCellMatches " << doubleQuote(objPath) <<
                   " " << r << " " << c);
  }

  else if (funcName == "CheckMessageBoxTextMatches") {
    auto [path, expectRE] =
      gdvpToTuple<std::string, std::string>(parser);

    QMessageBox *mb = getObjectFromPath<QMessageBox>(path);
    string actual = toString(mb->text());
    CHECK_RE_MATCH("CheckMessageBoxTextMatches");
  }

  else if (funcName == "CheckMessageBoxDetailedText") {
    BIND_STRING_ARGS2(path, expect);

    QMessageBox *mb = getObjectFromPath<QMessageBox>(path);
    string actual = toString(mb->detailedText());
    CHECK_EQ("CheckMessageBoxDetailedText");
  }

  else if (funcName == "CheckClipboard") {
    BIND_STRING_ARGS1(expect);

    string actual = toString(QApplication::clipboard()->text());
    CHECK_EQ("CheckClipboard");
  }

  else if (funcName == "CheckActionChecked") {
    auto [path, expect] =
      gdvpToTuple<std::string, bool>(parser);

    QAction *action = getObjectFromPath<QAction>(path);
    bool actual = action->isChecked();
    CHECK_EQ("CheckActionChecked " << doubleQuote(path));
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
      xmessagesb("Failed to load screenshot image: " << expectFname);
    }

    EventReplayQueryable *q = getQueryableFromPath(path);
    QImage actualImage = q->eventReplayImage(what);

    if (actualImage != expectImage) {
      QString actualFname("failing-actual-image.png");
      if (!actualImage.save(actualFname, "PNG")) {
        xmessagesb("CheckImage: Does not match expected image " <<
                   expectFname << ".  Additionally, I "
                   "failed to save the actual image to " <<
                   actualFname);
      }
      else {
        xmessagesb("CheckImage: Does not match expected image " <<
                   expectFname << ".  Actual image "
                   "saved to " << actualFname);
      }
    }
  }

  else if (funcName == "CheckSize") {
    auto [path, expect] =
      gdvpToTuple<std::string, QSize>(parser);

    QWidget *widget = getObjectFromPath<QWidget>(path);
    QSize actual = widget->size();
    CHECK_EQ("CheckSize " << doubleQuote(path));
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
      TRACE1("TouchFile: unchanged file modification time, sleeping...");
      sleepForMS(1000);

      sfu.touchFile(fname);

      // There's not much point in checking again.  If it worked, great.
      // If not, I don't want to sit in a loop, so just keep going.
    }
  }

  else if (funcName == "RemoveFileIfExists") {
    auto [fname] =
      gdvpToTuple<std::string>(parser);

    SMFileUtil().removeFileIfExists(fname);
  }

  else if (funcName == "RecursivelyRemoveFilePath") {
    auto [path] =
      gdvpToTuple<std::string>(parser);

    // For safety, ensure the path is not absolute and has no "..".
    xassert(!SMFileUtil().isAbsolutePath(path));
    xassert(!hasSubstring(path, ".."));

    if (!SMFileUtil().pathExists(path)) {
      // Nothing to do.
    }
    else {
      // I should not need "-f" here, in part because I have confirmed
      // that the path exists.
      RunProcess::check_run(std::vector<std::string>{"rm", "-r", path});
    }
  }

  else if (funcName == "CheckPathExists") {
    auto [path, expect] =
      gdvpToTuple<std::string, bool>(parser);

    bool actual = SMFileUtil().pathExists(path);
    CHECK_EQ("CheckPathExists " << doubleQuote(path));
  }

  else if (funcName == "CheckFileContents") {
    auto [fname, expect] =
      gdvpToTuple<std::string, std::string>(parser);

    string actual = SMFileUtil().readFileAsString(fname);
    CHECK_EQ("CheckFileContentsSize " << doubleQuote(fname));
  }

  else if (funcName == "WriteFileContents") {
    auto [fname, contents] =
      gdvpToTuple<std::string, std::string>(parser);

    SMFileUtil().writeFileAsString(fname, contents);
  }

  else {
    xmessagesb("unrecognized function: " << doubleQuote(funcName));
  }
}


void EventReplay::sleepForMS(int ms)
{
  TRACE1("sleeping for " << ms << " ms");
  IncDecWaitingCounter idwc;
  sleepWhilePumpingEvents(ms);
  TRACE1("done sleeping");
}


void EventReplay::replayShortcut(
  std::string const &receiverPath,
  std::string const &keys)
{
  QObject *receiverObject = getQObjectFromPath(receiverPath);

  if (QLabel *label = dynamic_cast<QLabel*>(receiverObject)) {
    TRACE1("Shortcut receiver is a label: " << label->objectName());

    if (QWidget *buddy = label->buddy()) {
      TRACE1("Its buddy is: " << buddy->objectName());

      // If we replay this as a Shortcut event, it will not work (for
      // unknown reasons).  So manually set the focus.
      buddy->setFocus();
      return;
    }
  }

  // I think perhaps shortcuts only work directly when the target is a
  // menu item.  For a button I seem to have to click it myself.
  if (auto *button = dynamic_cast<QPushButton*>(receiverObject)) {
    TRACE1("Shortcut receiver is a button: " << button->objectName());
    button->click();
    return;
  }

  // Normal shortcut replay.
  QCoreApplication::postEvent(
    receiverObject,
    getShortcutEventFromString(keys));
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
      m_globalSelfCheck();
      return true;
    }

    // Get the next command.
    if (m_nextTestCommandIndex < m_testCommands.size()) {
      GDValue const &command =
        m_testCommands.at(m_nextTestCommandIndex++);

      TRACE1("replaying: " << command.sourceLocationIndicator() <<
             command);
      TRACE3("command dump: " << command.dumpToString());

      // Use the location of `command` as context.
      EXN_CONTEXT(command.sourceLocation());

      this->replayCall(command);
      m_globalSelfCheck();
      return true;
    }
    else {
      // EOF.
      TRACE1("end of commands reached");
      return false;
    }
  }
  catch (string const &msg) {
    m_testResult = msg;
  }
  catch (std::exception &x) {
    m_testResult = x.what();
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
  GDValue const &expect)
{
  auto gdvFunc = [receiver, state]() -> GDValue {
    EventReplayQueryable *q = getQueryableFromPath(receiver);
    return q->eventReplayQuery(state);
  };
  waitUntilCheckGDValueFunction(durationMS, gdvFunc, expect);
}


void EventReplay::waitUntilCheckGDValueFunction(
  long durationMS,
  std::function<GDValue ()> gdvFunc,
  GDValue const &expect)
{
  long startMS = getMilliseconds();
  int checkCount = 0;

  TRACE1("waiting for up to " << durationMS << " ms");

  // Put the `expect` location onto the context stack in case we are
  // executing a function from another file.
  EXN_CONTEXT(expect.sourceLocation().asString());

  // Arrange to receive an event after 'durationMS'.  We do not directly
  // handle the event; rather, we use it to cause 'processEvents' to
  // return.
  QTimer timer;
  timer.start(durationMS);

  while (true) {
    checkCount++;
    GDValue actual = gdvFunc();
    if (actual == expect) {
      break;
    }
    long elapsedMS = getMilliseconds() - startMS;
    long remainingMS = durationMS - elapsedMS;
    if (remainingMS <= 0) {
      xmessagesb("WaitUntilCheckQuery: Slept for " << elapsedMS <<
                 " ms but value is " << actual << ", not " <<
                 expect);
    }

    // Wait for something to happen.  This does not busy-wait.
    IncDecWaitingCounter idwc;
    QCoreApplication::processEvents(QEventLoop::WaitForMoreEvents);

    // I tried using the 'processEvents' that accepts a timeout, but
    // that one appears to ignore the 'WaitForMoreEvents' flag.
  }

  long elapsedMS = getMilliseconds() - startMS;
  TRACE1("condition satisfied after " << elapsedMS <<
         " ms and " << checkCount << " checks");
}


void EventReplay::postQuiescenceEvent()
{
  QCoreApplication::postEvent(this, new QuiescenceEvent());
}


void EventReplay::installTimer()
{
  xassert(m_eventReplayDelayMS);
  TRACE1("starting timer");

  this->killTimerIf();
  m_timerId = this->startTimer(m_eventReplayDelayMS);
  xassert(m_timerId != 0);
}


void EventReplay::killTimerIf()
{
  if (m_timerId != 0) {
    TRACE1("killing timer");
    this->killTimer(m_timerId);
    m_timerId = 0;
  }
}


string EventReplay::runTest()
{
  // Do one self-check at the start so a later failure is known to be
  // caused by something that happened while replaying.
  m_globalSelfCheck();

  if (m_eventReplayDelayMS) {
    // Use timer-based notification.
    TRACE1("installing first timer");
    this->installTimer();
  }
  else {
    // Arrange to get notified just before the event dispatcher yields
    // control to the OS.  See doc/event-replay.txt.
    TRACE2("connecting slot_aboutToBlock");
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
  TRACE1("runTest starting top-level event loop");
  m_eventLoop.exec();

  TRACE1("runTest finished; result: " << doubleQuote(m_testResult));
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
    TRACE2("test complete, stopping replay event loop");
    m_eventLoop.exit(0);
  }

  TRACE2("callReplayNextEvent returning " << ret);
  return ret;
}


bool EventReplay::event(QEvent *ev)
{
  if (ev->type() == s_quiescenceEventType) {
    TRACE2("received QuiescenceEvent");
    if (g_waitingCounter) {
      TRACE2("ignoring QuiescenceEvent because g_waitingCounter is " <<
             g_waitingCounter);
    }
    else if (this->callReplayNextEvent()) {
      // Test is continuing.  We don't have to do anything to keep
      // receiving 'aboutToBlock' and hence enqueueing QuiescenceEvents.
    }
    else {
      // Disconnect from the event dispatcher to stop getting signals.
      TRACE2("disconnecting slot_aboutToBlock");
      QObject::disconnect(QAbstractEventDispatcher::instance(), NULL,
                          this, NULL);
    }

    TRACE2("finished with QuiescenceEvent");
    return true;
  }

  if (ev->type() == QEvent::Timer) {
    TRACE2("received TimerEvent");
    this->killTimerIf();

    // Post the next event.
    if (this->callReplayNextEvent()) {
      // Test is continuing.  Arrange to receive another event.
      this->installTimer();
    }
    else {
      // The timer has been killed so we will not get any more events.
      TRACE2("refraining from installing another timer");
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

  TRACE2("in slot_aboutToBlock");

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
