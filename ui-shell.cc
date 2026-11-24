// ui-shell.cc
// code for ui-shell.h

#include "ui-shell.h"                            // this module

// NOTE: There should not be any dependencies here on the editor per se.
// If you want to, say, query an EditorWidget from a test script, use
// the "CheckQuery" generic query.

// editor
#include "debug-values.h"                        // DEBUG_VALUES
#include "file-mod-time.h"                       // getFileModTime
#include "ui-shell-queryable.h"                  // UIShellQueryable
#include "ui-shell-result.h"                     // UIShellResult
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
#include "smbase/exc.h"                          // smbase::{XBase, XMessage}, xmessagesb, EXN_CONTEXT, GENERIC_CATCH_BEGIN/END
#include "smbase/gdvalue-parser.h"               // gdv::{GDValueParser, gdvpTo}
#include "smbase/gdvalue-tuple.h"                // gdv::gdvpToTuple
#include "smbase/gdvalue-vector.h"               // gdv::toGDValue(std::vector)
#include "smbase/gdvalue.h"                      // gdv::GDValue
#include "smbase/optional-util.h"                // smbase::takeOptionalsValue
#include "smbase/overflow.h"                     // safeToInt
#include "smbase/nonport.h"                      // getMilliseconds
#include "smbase/run-process.h"                  // RunProcess
#include "smbase/sm-env.h"                       // smbase::envAsIntOr
#include "smbase/sm-file-util.h"                 // SMFileUtil
#include "smbase/sm-is-equal.h"                  // smbase::is_equal
#include "smbase/sm-macros.h"                    // IMEMBFP
#include "smbase/sm-platform.h"                  // PLATFORM_IS_WINDOWS
#include "smbase/sm-trace.h"                     // INIT_TRACE, etc.
#include "smbase/string-util.h"                  // doubleQuote
#include "smbase/stringb.h"                      // stringb
#include "smbase/syserr.h"                       // xsyserror
#include "smbase/xassert.h"                      // xassert, xassertPrecondition, xassertInvariant

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
#include <utility>                               // std::move

using namespace gdv;
using namespace smbase;


INIT_TRACE("ui-shell");


// -------------------------- UIShellExecutor --------------------------
int UIShellExecutor::s_quiescenceEventType = 0;


UIShellExecutor::QuiescenceEvent::QuiescenceEvent()
  : QEvent((QEvent::Type)s_quiescenceEventType)
{}

UIShellExecutor::QuiescenceEvent::~QuiescenceEvent()
{}


UIShellExecutor::UIShellExecutor(
  std::function<void ()> globalSelfCheck)
:
  QObject(),
  IMEMBFP(globalSelfCheck),
  m_busy(false),
  m_pendingCommand(),
  m_commandSuccessResult(),
  m_commandResult(),
  m_queuedFocusKeySequence(),
  m_uiShellDelayMS(envAsIntOr(0, "UI_SHELL_DELAY_MS")),
  m_timerId(),
  m_watchingAboutToBlock(false)
{
  if (s_quiescenceEventType == 0) {
    s_quiescenceEventType = QEvent::registerEventType();
    TRACE2(DEBUG_VALUES(s_quiescenceEventType));
    xassert(s_quiescenceEventType > 0);
  }

  selfCheck();
}


UIShellExecutor::~UIShellExecutor()
{
  this->killTimerIf();

  // See doc/signals-and-dtors.txt.
  //
  // It is possible we already disconnected, or never connected in the
  // first place, but this is still safe.
  QObject::disconnect(QAbstractEventDispatcher::instance(), nullptr,
                      this, nullptr);
}


void UIShellExecutor::selfCheck() const
{
  xassertInvariant(
    !(m_commandSuccessResult.has_value() &&
      m_commandResult.has_value())
  );

  if (m_uiShellDelayMS != 0) {
    xassertInvariant(m_watchingAboutToBlock == false);
  }
  else {
    xassertInvariant(m_watchingAboutToBlock == m_busy);
  }
}


bool UIShellExecutor::isBusy() const
{
  return m_busy;
}


void UIShellExecutor::startCommand(GDValue const &command)
{
  xassertPrecondition(!m_busy);
  selfCheck();

  m_commandSuccessResult.reset();
  m_commandResult.reset();
  m_pendingCommand = command;
  m_busy = true;

  this->startWatching();

  selfCheck();
}


bool UIShellExecutor::hasCommandResult() const
{
  return m_commandResult.has_value();
}


UIShellResult UIShellExecutor::takeCommandResult()
{
  xassertPrecondition(hasCommandResult());
  selfCheck();

  return takeOptionalsValue(std::move(m_commandResult));
}


void UIShellExecutor::resizeChildWidget(
  QWidget *widget, QSize const &targetSize)
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
  // now, I will just override it while executing UI shell commands.
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
static QObject *getQObjectFromPath(std::string const &path)
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
T *getObjectFromPath(std::string const &path)
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
// with qobject_cast, but for UIShellQueryable I want to use
// ordinary dynamic_cast since I don't want to make it a QObject due to
// potential multiple inheritance issues.  Perhaps I should only use
// dynamic_cast?  Although getting readable class names using C++ RTTI
// is annoying.
template <class T>
T *getObjectFromPathDC(std::string const &path)
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


static UIShellQueryable *getQueryableFromPath(std::string const &path)
{
  return getObjectFromPathDC<UIShellQueryable>(path);
}


QWidget *UIShellExecutor::getFocusWidget(QString const &funcName)
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
static bool regexSearch(std::string const &str, std::string const &re)
{
  std::regex regex(re.c_str(), std::regex::ECMAScript);
  return std::regex_search(str.c_str(), regex);
}


static void checkRegexSearch(
  std::string const &actual, std::string const &expectRE)
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


// Bind 'arg1' to the single expected string argument of a UI shell
// command.
#define BIND_STRING_ARGS1(arg1)                     \
  CHECK_NUM_ARGS(1);                                \
  std::string arg1 = GET_STRING_ARG(0) /* user ; */

#define BIND_STRING_ARGS2(arg1, arg2)               \
  CHECK_NUM_ARGS(2);                                \
  std::string arg1 = GET_STRING_ARG(0);             \
  std::string arg2 = GET_STRING_ARG(1) /* user ; */

#define BIND_STRING_ARGS3(arg1, arg2, arg3)         \
  CHECK_NUM_ARGS(3);                                \
  std::string arg1 = GET_STRING_ARG(0);             \
  std::string arg2 = GET_STRING_ARG(1);             \
  std::string arg3 = GET_STRING_ARG(2) /* user ; */

#define BIND_STRING_ARGS4(arg1, arg2, arg3, arg4)   \
  CHECK_NUM_ARGS(4);                                \
  std::string arg1 = GET_STRING_ARG(0);             \
  std::string arg2 = GET_STRING_ARG(1);             \
  std::string arg3 = GET_STRING_ARG(2);             \
  std::string arg4 = GET_STRING_ARG(3) /* user ; */


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


GDValue UIShellExecutor::executeCommand(GDValue const &command)
{
  selfCheck();

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

    // Return so 'executeNextCommand' will process them.
  }

  else if (funcName == "Shortcut") {
    BIND_STRING_ARGS2(receiverPath, keys);

    executeShortcut(receiverPath, keys);
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

    postClickButton(getObjectFromPath<QAbstractButton>(path));
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

    UIShellQueryable *q = getQueryableFromPath(receiver);
    GDValue actual = q->uiShellQuery(state);
    CHECK_EQ("CheckQuery " << doubleQuote(receiver) << ' ' <<
             doubleQuote(state));
  }

  else if (funcName == "CheckQueryMatches") {
    auto [receiver, state, expectRE] =
      gdvpToTuple<std::string, std::string, std::string>(parser);

    UIShellQueryable *q = getQueryableFromPath(receiver);
    GDValue actualValue = q->uiShellQuery(state);
    if (!actualValue.isString()) {
      xmessagesb("CheckQueryMatches: the actual value " <<
        actualValue << " is not a string; expected it to the regex " <<
        doubleQuote(expectRE) << ".");
    }
    std::string actual = actualValue.stringGet();
    CHECK_RE_MATCH("CheckQueryMatches " << doubleQuote(receiver) << ' ' <<
                   doubleQuote(state));
  }

  else if (funcName == "CheckLabel") {
    BIND_STRING_ARGS2(path, expect);

    QLabel *label = getObjectFromPath<QLabel>(path);
    std::string actual = toString(label->text());
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
    std::string actual = toString(label->text());
    CHECK_RE_MATCH("CheckLabelMatches " << doubleQuote(path));
  }

  else if (funcName == "CheckComboBoxText") {
    BIND_STRING_ARGS2(path, expect);

    QComboBox *cbox = getObjectFromPath<QComboBox>(path);
    std::string actual = toString(cbox->currentText());
    CHECK_EQ("CheckComboBoxText " << doubleQuote(path));
  }

  else if (funcName == "CheckLineEditText") {
    BIND_STRING_ARGS2(path, expect);

    QLineEdit *lineEdit = getObjectFromPath<QLineEdit>(path);
    std::string actual = toString(lineEdit->text());
    CHECK_EQ("CheckLineEditText " << doubleQuote(path));
  }

  else if (funcName == "CheckTextEditText") {
    auto [path, expect] =
      gdvpToTuple<std::string, std::string>(parser);

    auto *textEdit = getObjectFromPath<QTextEdit>(path);
    std::string actual = toString(textEdit->toPlainText());
    CHECK_EQ("CheckTextEditText " << doubleQuote(path));
  }

  else if (funcName == "CheckListViewSelectedItem") {
    auto [path, expect] =
      gdvpToTuple<std::string, std::string>(parser);

    auto *listView = getObjectFromPath<QListView>(path);

    // https://stackoverflow.com/a/38772280
    QModelIndex index = listView->currentIndex();
    QString itemText = index.data(Qt::DisplayRole).toString();

    std::string actual = toString(itemText);
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
    std::string actual = getTableWidgetCell(table, r, c);
    CHECK_RE_MATCH("CheckTableWidgetCellMatches " << doubleQuote(objPath) <<
                   " " << r << " " << c);
  }

  else if (funcName == "CheckMessageBoxTextMatches") {
    auto [path, expectRE] =
      gdvpToTuple<std::string, std::string>(parser);

    QMessageBox *mb = getObjectFromPath<QMessageBox>(path);
    std::string actual = toString(mb->text());
    CHECK_RE_MATCH("CheckMessageBoxTextMatches");
  }

  else if (funcName == "CheckMessageBoxDetailedText") {
    BIND_STRING_ARGS2(path, expect);

    QMessageBox *mb = getObjectFromPath<QMessageBox>(path);
    std::string actual = toString(mb->detailedText());
    CHECK_EQ("CheckMessageBoxDetailedText");
  }

  else if (funcName == "CheckClipboard") {
    BIND_STRING_ARGS1(expect);

    std::string actual = toString(QApplication::clipboard()->text());
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
    std::string actual =
      toString(getFocusWidget(funcName)->window()->windowTitle());
    CHECK_EQ("CheckFocusWindowTitle");
  }

  else if (funcName == "CheckFocusWindowTitleMatches") {
    BIND_STRING_ARGS1(expectRE);

    checkFocusWorkaround();
    std::string actual =
      toString(getFocusWidget(funcName)->window()->windowTitle());
    CHECK_RE_MATCH("CheckFocusWindowTitleMatches");
  }

  else if (funcName == "CheckWindowTitle") {
    BIND_STRING_ARGS2(path, expect);

    QWidget *widget = getObjectFromPath<QWidget>(path);
    std::string actual = toString(widget->window()->windowTitle());
    CHECK_EQ("CheckWindowTitle");
  }

  else if (funcName == "CheckFocusWindow") {
    BIND_STRING_ARGS1(expect);

    checkFocusWorkaround();
    std::string actual = qObjectPath(getFocusWidget(funcName)->window());
    CHECK_EQ("CheckFocusWindow");
  }

  else if (funcName == "CheckFocusWidget") {
    BIND_STRING_ARGS1(expect);

    checkFocusWorkaround();
    std::string actual = qObjectPath(getFocusWidget(funcName));
    CHECK_EQ("CheckFocusWidget");
  }

  else if (funcName == "CheckImage") {
    BIND_STRING_ARGS3(path, what, expectFname);

    QImage expectImage;
    if (!expectImage.load(toQString(expectFname), "PNG")) {
      xmessagesb("Failed to load screenshot image: " << expectFname);
    }

    UIShellQueryable *q = getQueryableFromPath(path);
    QImage actualImage = q->uiShellImage(what);

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

    FileTimestamp beforeModUnixTime = getFileModTime(fname);

    SMFileUtil sfu;
    sfu.touchFile(fname);

    FileTimestamp afterModUnixTime = getFileModTime(fname);

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

    std::string actual = SMFileUtil().readFileAsString(fname);
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

  selfCheck();

  // None of the existing commands produce a value.
  return GDValue();
}


void UIShellExecutor::postClickButton(QAbstractButton *button)
{
  QObject::connect(this, &UIShellExecutor::signal_clickButton,
                   button, &QAbstractButton::click,
                   Qt::QueuedConnection);
  Q_EMIT signal_clickButton();
  QObject::disconnect(this, &UIShellExecutor::signal_clickButton,
                      button, &QAbstractButton::click);
}


void UIShellExecutor::sleepForMS(int ms)
{
  TRACE1("sleeping for " << ms << " ms");
  IncDecWaitingCounter idwc;
  sleepWhilePumpingEvents(ms);
  TRACE1("done sleeping");
}


void UIShellExecutor::executeShortcut(
  std::string const &receiverPath,
  std::string const &keys)
{
  QObject *receiverObject = getQObjectFromPath(receiverPath);

  if (QLabel *label = dynamic_cast<QLabel*>(receiverObject)) {
    TRACE1("Shortcut receiver is a label: " << label->objectName());

    if (QWidget *buddy = label->buddy()) {
      TRACE1("Its buddy is: " << buddy->objectName());

      // If we execute this as a Shortcut event, it will not work (for
      // unknown reasons).  So manually set the focus.
      buddy->setFocus();
      return;
    }
  }

  // I think perhaps shortcuts only work directly when the target is a
  // menu item.  For a button I seem to have to click it myself.
  if (auto *button = dynamic_cast<QPushButton*>(receiverObject)) {
    TRACE1("Shortcut receiver is a button: " << button->objectName());
    postClickButton(button);
    return;
  }

  // Normal shortcut execution.
  QCoreApplication::postEvent(
    receiverObject,
    getShortcutEventFromString(keys));
}


// Workaround for problem on Linux I haven't solved: wait a little
// before checking focus.
//
// The problem manifests with `file-open-dialog1.uis` (and this sleep
// disabled), among others.  We execute the Return keypress that should
// close the dialog, but if we immediately check focus, then no widget
// has focus.  I think there is a problem with how my event loops are
// nested.
//
// TODO: Debug and fix.
void UIShellExecutor::checkFocusWorkaround()
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


void UIShellExecutor::executeFocusKey(char c)
{
  QWidget *focusWidget = getFocusWidget("executeFocusKey");

  // As for 'FocusKeyPR', posting both events at once should be safe.
  QCoreApplication::postEvent(
    focusWidget,
    charToKeyEvent(QEvent::KeyPress, c));
  QCoreApplication::postEvent(
    focusWidget,
    charToKeyEvent(QEvent::KeyRelease, c));
}


void UIShellExecutor::executeNextStep()
{
  xassertPrecondition(m_busy);
  selfCheck();

  // If set, the command failed, and this describes how.
  std::optional<std::string> errorMessage;

  try {
    // Execute the pending command, if any.
    if (m_pendingCommand) {
      GDValue command(takeOptionalsValue(std::move(m_pendingCommand)));

      TRACE1("executing: " << command.sourceLocationIndicator() <<
             command);
      TRACE3("command dump: " << command.dumpToString());

      // Use the location of `command` as context.  A command that was
      // not read from a file might not have one.
      //
      // TODO: Create EXN_CONTEXT_STRING_IF to handle this better.
      //
      // TODO: Also use `sourceLocationIndicator` rather than the GDVN
      // of the location.
      EXN_CONTEXT_STRING(command.hasSourceLocation()?
        stringb(command.sourceLocation()) :
        std::string("(no source location)"));

      m_commandSuccessResult = this->executeCommand(command);
      m_globalSelfCheck();
      selfCheck();
      return;
    }

    // Process any queued focus keys.
    if (!m_queuedFocusKeySequence.isEmpty()) {
      // Note that the error reporting in the 'catch' blocks is
      // appropriate since the file and line number are still correct
      // when we get here.
      //
      // TODO: I don't think the above is true.  Where did file/line get
      // set?  I think this comment is from before I added the
      // EXN_CONTEXT_STRING call.
      this->executeFocusKey(m_queuedFocusKeySequence.pop());
      m_globalSelfCheck();
      selfCheck();
      return;
    }

    // Nothing remains, so the command has finished successfully.
  }
  catch (std::string const &msg) {
    // TODO: Am I still throwing strings somewhere?  If so, then
    // `tryUIShellQuery` needs updating.
    errorMessage = msg;
  }
  catch (std::exception &x) {
    errorMessage = x.what();
  }
  catch (...) {
    errorMessage = std::string("caught unknown exception!");
  }

  if (errorMessage) {
    // Discard whatever remains of the failed command.
    m_queuedFocusKeySequence.clear();

    this->finishCommand(UIShellResult::makeRight(
      *errorMessage));
  }
  else if (!m_commandSuccessResult) {
    // This shouldn't happen, but if it does, an error (rather than an
    // assertion) suffices.
    this->finishCommand(UIShellResult::makeRight(
      "Finished command but no result?"));
  }
  else {
    this->finishCommand(UIShellResult::makeLeft(
      std::move(*m_commandSuccessResult)));
  }
}


void UIShellExecutor::finishCommand(UIShellResult &&result)
{
  TRACE2("command finished: " << toGDValue(result));

  this->stopWatching();
  m_busy = false;
  m_commandSuccessResult.reset();
  m_commandResult = std::move(result);

  // Check before we send the signal, since sending it effectively
  // publishes our state.
  selfCheck();

  Q_EMIT signal_hasCommandResult();

  selfCheck();

  if (m_busy && m_pendingCommand) {
    // The signal handler started another command.  Execute it during
    // this same quiescence cycle, rather than waiting for the next,
    // so each command runs at the first quiescence after the previous
    // one finished.
    //
    // This (I think!) isn't necessary for correctness, but is how the
    // code worked prior to splitting `UIShellExecutor` off, and is a
    // slight efficiency win.
    //
    // TODO: Test if it is necessary.
    TRACE2("executing the next command immediately");
    this->executeNextStep();
  }
}


void UIShellExecutor::waitUntilCheckQuery(
  long durationMS,
  std::string const &receiver,
  std::string const &state,
  GDValue const &expect)
{
  auto gdvFunc = [receiver, state]() -> GDValue {
    UIShellQueryable *q = getQueryableFromPath(receiver);
    return q->uiShellQuery(state);
  };
  waitUntilCheckGDValueFunction(durationMS, gdvFunc, expect);
}


void UIShellExecutor::waitUntilCheckGDValueFunction(
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


void UIShellExecutor::startWatching()
{
  if (m_uiShellDelayMS) {
    // Use timer-based notification.
    if (!m_timerId.has_value()) {
      TRACE1("installing first timer");
      this->installTimer();
    }
  }
  else if (!m_watchingAboutToBlock) {
    // Arrange to get notified just before the event dispatcher yields
    // control to the OS.  See doc/ui-shell.txt.
    TRACE2("connecting slot_aboutToBlock");
    QObject::connect(QAbstractEventDispatcher::instance(),
                     &QAbstractEventDispatcher::aboutToBlock,
                     this, &UIShellExecutor::slot_aboutToBlock);
    m_watchingAboutToBlock = true;
  }
}


void UIShellExecutor::stopWatching()
{
  this->killTimerIf();

  if (m_watchingAboutToBlock) {
    // Disconnect from the event dispatcher to stop getting signals.
    TRACE2("disconnecting slot_aboutToBlock");
    QObject::disconnect(QAbstractEventDispatcher::instance(), nullptr,
                        this, nullptr);
    m_watchingAboutToBlock = false;
  }
}


void UIShellExecutor::postQuiescenceEvent()
{
  QCoreApplication::postEvent(this, new QuiescenceEvent());
}


void UIShellExecutor::installTimer()
{
  xassert(m_uiShellDelayMS);
  TRACE1("starting timer");

  this->killTimerIf();
  m_timerId = this->startTimer(m_uiShellDelayMS);
}


void UIShellExecutor::killTimerIf()
{
  if (m_timerId.has_value()) {
    TRACE1("killing timer");
    this->killTimer(*m_timerId);
    m_timerId.reset();
  }
}


// ------------------------ UIShellScriptRunner ------------------------
UIShellScriptRunner::UIShellScriptRunner(
  std::vector<gdv::GDValue> const &testCommands,
  std::function<void ()> globalSelfCheck)
:
  QObject(),
  IMEMBFP(testCommands),
  m_nextTestCommandIndex(0),
  m_testFailureMessage(),
  m_eventLoop(),
  IMEMBFP(globalSelfCheck),
  m_executor(globalSelfCheck)
{
  // The connection must be direct so that, when the slot starts the
  // next command, the executor runs it in the same quiescence cycle.
  //
  // TODO: Clarify how necessary that is.
  QObject::connect(&m_executor, &UIShellExecutor::signal_hasCommandResult,
                   this, &UIShellScriptRunner::slot_hasCommandResult,
                   Qt::DirectConnection);
}


UIShellScriptRunner::~UIShellScriptRunner()
{
  // See doc/signals-and-dtors.txt.
  QObject::disconnect(&m_executor, nullptr, this, nullptr);
}


void UIShellScriptRunner::startNextCommand()
{
  xassertPrecondition(m_nextTestCommandIndex < m_testCommands.size());

  m_executor.startCommand(m_testCommands.at(m_nextTestCommandIndex++));
}


std::optional<std::string> UIShellScriptRunner::runTest()
{
  // Do one self-check at the start so a later failure is known to be
  // caused by something that happened while executing.
  m_globalSelfCheck();

  if (m_testCommands.empty()) {
    // Even with no commands, run the event loop until the app is first
    // quiescent, so it gets to finish starting up.  `Sleep(0)` is
    // effectively a no-op command.
    m_executor.startCommand(GDValue::readFromString("Sleep(0)"));
  }
  else {
    this->startNextCommand();
  }

  // Process events until the test completes.
  //
  // NOTE: This is simply the substitute for the top-level application
  // event loop.  While the test runs, the app may start other event
  // loops, e.g., for modal dialogs.  Thus, it would not work to simply
  // unpack this loop and execute commands here.
  TRACE1("runTest starting top-level event loop");
  m_eventLoop.exec();

  if (m_testFailureMessage) {
    TRACE1("runTest finished with error: " <<
           doubleQuote(*m_testFailureMessage));
  }
  else {
    TRACE1("runTest finished successfully");
  }
  return m_testFailureMessage;
}


void UIShellScriptRunner::slot_hasCommandResult() NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  UIShellResult result(m_executor.takeCommandResult());

  if (result.isRight()) {
    m_testFailureMessage = result.right();

    // Our event loop might not be the innermost event loop at the
    // moment, e.g., if we're running a modal dialog.  So, print the
    // result to the console now so the user can see that the test has
    // failed.  (Otherwise it looks like the test fails when the dialog
    // is closed, which is misleading.)
    cout << "test FAILED: " << *m_testFailureMessage << endl;
  }
  else if (m_nextTestCommandIndex < m_testCommands.size()) {
    // Test is continuing.
    this->startNextCommand();
    return;
  }
  else {
    TRACE1("end of commands reached");

    // In the context of the script runner, which is not interactive, we
    // just discard the successful `result`, which is normally just the
    // `null` GDValue.  The UI shell server will return it to the
    // client.
  }

  // Test is complete (pass or fail).  Stop the event loop we started in
  // 'runTest'.
  TRACE2("test complete, stopping UI shell event loop");
  m_eventLoop.exit(0);

  GENERIC_CATCH_END
}


// -------------------- UIShellExecutor (continued) --------------------
bool UIShellExecutor::event(QEvent *ev)
{
  if (ev->type() == s_quiescenceEventType) {
    TRACE2("received QuiescenceEvent");

    // TODO: Change to `m_watchingAboutToBlock`.
    if (!m_busy) {
      // This can happen if the event was posted before we stopped
      // watching for quiescence.
      TRACE2("ignoring QuiescenceEvent because no command is active");
    }
    else if (g_waitingCounter) {
      TRACE2("ignoring QuiescenceEvent because g_waitingCounter is " <<
             g_waitingCounter);
    }
    else {
      // If this finishes the command, it also stops watching, unless
      // another command gets started.  Otherwise, we will continue to
      // receive 'aboutToBlock' and hence enqueue QuiescenceEvents.
      this->executeNextStep();
    }

    TRACE2("finished with QuiescenceEvent");
    return true;
  }

  if (ev->type() == QEvent::Timer) {
    TRACE2("received TimerEvent");
    this->killTimerIf();

    if (m_busy) {
      this->executeNextStep();
    }

    if (m_busy && !m_timerId.has_value()) {
      // The command is continuing.  Arrange to receive another event.
      // (If the command finished and the handler started another,
      // `startWatching` has already installed a timer.)
      this->installTimer();
    }
    else {
      TRACE2("refraining from installing another timer");
    }

    return true;
  }

  // This object should only be receiving the above two events, and I
  // don't think QObject::event does anything with them (besides return
  // false), but I will do this just for overall cleanliness.
  return QObject::event(ev);
}


void UIShellExecutor::slot_aboutToBlock() NOEXCEPT
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
  // So, post an event that will trigger the next command execution.
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
