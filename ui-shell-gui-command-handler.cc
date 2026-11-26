// ui-shell-gui-command-handler.cc
// Code for `ui-shell-gui-command-handler` module.

#include "ui-shell-gui-command-handler.h"        // this module

// NOTE: There should not be any dependencies here on the editor per se.
// If you want to, say, query an EditorWidget from a test script, use
// the "CheckQuery" generic query.

// editor
#include "ui-shell-command-util.h"               // BIND_STRING_ARGS1, etc.
#include "ui-shell-gui-queryable.h"              // UIShellGUIQueryable
#include "ui-shell-queryable.h"                  // UIShellQueryable

// smqtutil
#include "smqtutil/gdvalue-qt.h"                 // gdvpTo(QSize)
#include "smqtutil/qstringb.h"                   // qstringb
#include "smqtutil/qtguiutil.h"                  // showRaiseAndActivateWindow
#include "smqtutil/qtutil.h"                     // operator<<(QString), toQPointF, qObjectPath

// smbase
#include "smbase/chained-cond.h"                 // smbase::cc::z_le_lt
#include "smbase/exc.h"                          // smbase::{XBase, XMessage}, xmessagesb, EXN_CONTEXT
#include "smbase/gdvalue-parser.h"               // gdv::{GDValueParser, gdvpTo}
#include "smbase/gdvalue-tuple.h"                // gdv::gdvpToTuple
#include "smbase/gdvalue-vector.h"               // gdv::toGDValue(std::vector)
#include "smbase/gdvalue.h"                      // gdv::GDValue
#include "smbase/overflow.h"                     // safeToInt
#include "smbase/sm-platform.h"                  // PLATFORM_IS_WINDOWS
#include "smbase/sm-trace.h"                     // INIT_TRACE, etc.
#include "smbase/string-util.h"                  // doubleQuote
#include "smbase/xassert.h"                      // xassert

// Qt
#include <QAbstractButton>
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
#include <QWidget>

// libc++
#include <string>                                // std::string
#include <tuple>                                 // std::tuple [h]
#include <typeinfo>                              // typeid
#include <vector>                                // std::vector

using namespace gdv;
using namespace smbase;


INIT_TRACE("ui-shell-gui-command-handler");


UIShellGUICommandHandler::UIShellGUICommandHandler()
:
  QObject(),
  UIShellCommandHandler(),
  m_queuedFocusKeySequence()
{}


UIShellGUICommandHandler::~UIShellGUICommandHandler()
{}


bool UIShellGUICommandHandler::hasPendingStep() const
{
  return !m_queuedFocusKeySequence.isEmpty();
}


void UIShellGUICommandHandler::executePendingStep()
{
  xassertPrecondition(hasPendingStep());

  this->executeFocusKey(m_queuedFocusKeySequence.pop());
}


void UIShellGUICommandHandler::discardPendingSteps()
{
  m_queuedFocusKeySequence.clear();
}


void UIShellGUICommandHandler::resizeChildWidget(
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


/* Get an object from its path from a top-level window, or throw
   `XMessage` if we cannot find it.

   Beware: This function can find an object by starting from any
   top-level widget, i.e., one that has an associated window in the
   desktop window system.  However, modal dialogs are both top-level
   widgets *and* they have a non-null parent.  This means that a widget
   inside a modal dialog can be found *both* as "modalDialog.widget" and
   as "mainWindow.modalDialog.widget".

   In contrast, `qObjectPath` walks all the way up the parent tree, so
   if such a widget has its path computed by that function, it would be
   "mainWindow.modalDialog.widget".  This can be confusing when reading
   or writing .uis scripts since the same widget can seem to have
   multiple names.

   TODO: Probably the right solution is to change this function to skip
   top-level widgets that have non-null parents so that every widget is
   consistently referred to by one name.
*/
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


UIShellQueryable *UIShellGUICommandHandler::getQueryableFromPath(
  std::string const &path)
{
  return getObjectFromPathDC<UIShellQueryable>(path);
}


QWidget *UIShellGUICommandHandler::getFocusWidget(QString const &funcName)
{
  QWidget *focusWidget = QApplication::focusWidget();
  if (!focusWidget) {
    xmessagesb("No widget has focus.");
  }

  TRACE2(toString(funcName) << ": focusWidget: " <<
         qObjectPath(focusWidget));

  return focusWidget;
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


GDValue UIShellGUICommandHandler::executeDerivedCommand(
  std::string const &funcNameString,
  GDValueParser const &parser)
{
  // The command's tag, as a `QString` for convenient comparison.  The
  // base class has already put it into the exception context.
  QString const funcName(toQString(funcNameString));

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

    // Return so 'executePendingStep' will process them.
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

    QClipboard *cb = QApplication::clipboard();
    std::string actual = toString(cb->text());
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

    UIShellGUIQueryable *q =
      getObjectFromPathDC<UIShellGUIQueryable>(path);
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

  else {
    return UIShellCommandHandler::executeDerivedCommand(
      funcNameString, parser);
  }

  // None of the commands implemented here produce a value.
  return GDValue();
}


void UIShellGUICommandHandler::postClickButton(QAbstractButton *button)
{
  QObject::connect(this, &UIShellGUICommandHandler::signal_clickButton,
                   button, &QAbstractButton::click,
                   Qt::QueuedConnection);
  Q_EMIT signal_clickButton();
  QObject::disconnect(this, &UIShellGUICommandHandler::signal_clickButton,
                      button, &QAbstractButton::click);
}


void UIShellGUICommandHandler::executeShortcut(
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
void UIShellGUICommandHandler::checkFocusWorkaround()
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


void UIShellGUICommandHandler::executeFocusKey(char c)
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


// EOF
