// ui-shell-gui-test.cc
// Code for `ui-shell-gui-test`, a program for testing the GUI UI shell
// commands.  See doc/plans/claude-ui-shell-gui-test.txt.

// See license.txt for copyright and terms of use.

#include "ui-shell-gui-test.h"                   // this module

// NOTE: This program must not depend on the editor per se, since part
// of its purpose is to show that the UI shell does not.

// editor
#include "event-recorder.h"                      // EventRecorder
#include "ui-shell-app.h"                        // UIShellOptions, runUIShellScript, runUIShellTestCommandsIfRequested
#include "ui-shell-command-util.h"               // regexSearch
#include "ui-shell-gui-command-handler.h"        // UIShellGUICommandHandler

// smqtutil
#include "smqtutil/qstringb.h"                   // qstringb
#include "smqtutil/qtguiutil.h"                  // keysString, showRaiseAndActivateWindow
#include "smqtutil/qtutil.h"                     // installSMQtUtilMessageHandler, toString

// smbase
#include "smbase/chained-cond.h"                 // smbase::cc::z_le_lt
#include "smbase/exc.h"                          // smbase::{XBase, xformatsb, xmessagesb}, GENERIC_CATCH_BEGIN/END
#include "smbase/gdvalue-parser.h"               // gdv::GDValueParser
#include "smbase/gdvalue-tuple.h"                // gdv::gdvpToTuple
#include "smbase/gdvalue.h"                      // gdv::GDValue
#include "smbase/sm-macros.h"                    // IMEMBFP, OPEN_ANONYMOUS_NAMESPACE, TABLESIZE
#include "smbase/string-util.h"                  // beginsWith, doubleQuote
#include "smbase/xassert.h"                      // xassert, xfailure

// Qt
#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QGridLayout>
#include <QImage>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMouseEvent>
#include <QPushButton>
#include <QString>
#include <QTextEdit>
#include <QTimer>

// libc++
#include <exception>                             // std::exception
#include <iostream>                              // std::{cout, cerr, endl}
#include <memory>                                // std::unique_ptr
#include <optional>                              // std::optional
#include <string>                                // std::string

using namespace gdv;
using namespace smbase;


// ------------------------- UISGTKeyLogWidget -------------------------
UISGTKeyLogWidget::UISGTKeyLogWidget(QLabel *label, QWidget *parent)
  : QFrame(parent),
    IMEMBFP(label)
{
  setFrameStyle(QFrame::Box);
  setFocusPolicy(Qt::StrongFocus);
  setFixedSize(100, 30);
}


UISGTKeyLogWidget::~UISGTKeyLogWidget()
{}


/*static*/ QString UISGTKeyLogWidget::initialLabelText()
{
  return "key: (none)";
}


void UISGTKeyLogWidget::reportKeyEvent(char const *kind,
                                       QKeyEvent *event)
{
  m_label->setText(qstringb(
    "key: " << kind << " " << keysString(*event) << " " <<
    doubleQuote(toString(event->text()))));
}


void UISGTKeyLogWidget::keyPressEvent(QKeyEvent *event)
{
  reportKeyEvent("press", event);
}


void UISGTKeyLogWidget::keyReleaseEvent(QKeyEvent *event)
{
  reportKeyEvent("release", event);
}


// --------------------------- UISGTMousePad ---------------------------
UISGTMousePad::UISGTMousePad(QLabel *label, QWidget *parent)
  : QFrame(parent),
    IMEMBFP(label)
{
  setFrameStyle(QFrame::Box);
  setFixedSize(100, 100);

  // Needed to receive moves with no button pressed.
  setMouseTracking(true);
}


UISGTMousePad::~UISGTMousePad()
{}


/*static*/ QString UISGTMousePad::initialLabelText()
{
  return "mouse: (none)";
}


void UISGTMousePad::reportMouseEvent(char const *kind,
                                     QMouseEvent *event)
{
  // The fields are in the same order as the arguments of the commands
  // that make these events: button, position, buttons, modifiers.
  m_label->setText(qstringb(
    "mouse: " << kind << " " <<
    toString(Qt::MouseButtons(event->button())) << " " <<
    toString(event->pos()) << " " <<
    toString(event->buttons()) << " " <<
    toString(event->modifiers())));
}


void UISGTMousePad::mousePressEvent(QMouseEvent *event)
{
  reportMouseEvent("press", event);
}


void UISGTMousePad::mouseReleaseEvent(QMouseEvent *event)
{
  reportMouseEvent("release", event);
}


void UISGTMousePad::mouseDoubleClickEvent(QMouseEvent *event)
{
  // The default would call `mousePressEvent`.
  reportMouseEvent("dblclick", event);
}


void UISGTMousePad::mouseMoveEvent(QMouseEvent *event)
{
  reportMouseEvent("move", event);
}


// -------------------------- UISGTMainWindow --------------------------
UISGTMainWindow::UISGTMainWindow()
  : QMainWindow(),
    UIShellGUIQueryable(),
    m_clickCount(0),
    m_delayState(DS_IDLE),
    m_delayTimer(nullptr),
    m_central(nullptr),
    m_clickButton(nullptr),
    m_clickCountLabel(nullptr),
    m_nameLabel(nullptr),
    m_nameEdit(nullptr),
    m_copyButton(nullptr),
    m_nameResultLabel(nullptr),
    m_featureLabel(nullptr),
    m_delayButton(nullptr),
    m_delayLabel(nullptr),
    m_messageLabel(nullptr),
    m_colorCombo(nullptr),
    m_keyLogLabel(nullptr),
    m_keyLog(nullptr),
    m_mouseLabel(nullptr),
    m_mousePad(nullptr),
    m_notes(nullptr),
    m_quitAction(nullptr),
    m_featureAction(nullptr),
    m_resetAction(nullptr)
{
  setObjectName("mainWindow");
  setWindowTitle("UI Shell GUI Test");

  buildMenus();

  m_central = new QWidget(this);
  m_central->setObjectName("central");
  setCentralWidget(m_central);

  // Widgets are created as direct children of `m_central` so their
  // paths are short.
  QGridLayout *layout = new QGridLayout(m_central);
  int row = 0;

  m_clickButton = new QPushButton("&Click Me", m_central);
  m_clickButton->setObjectName("clickButton");
  layout->addWidget(m_clickButton, row, 0);
  QObject::connect(m_clickButton, &QPushButton::clicked,
                   this, &UISGTMainWindow::on_clickButtonClicked);

  m_clickCountLabel = new QLabel(clickCountLabelText(), m_central);
  m_clickCountLabel->setObjectName("clickCountLabel");
  layout->addWidget(m_clickCountLabel, row, 1);
  ++row;

  m_nameLabel = new QLabel("&Name:", m_central);
  m_nameLabel->setObjectName("nameLabel");
  layout->addWidget(m_nameLabel, row, 0);

  m_nameEdit = new QLineEdit(m_central);
  m_nameEdit->setObjectName("nameEdit");
  m_nameLabel->setBuddy(m_nameEdit);
  layout->addWidget(m_nameEdit, row, 1);

  m_copyButton = new QPushButton("Copy", m_central);
  m_copyButton->setObjectName("copyButton");
  layout->addWidget(m_copyButton, row, 2);
  QObject::connect(m_copyButton, &QPushButton::clicked,
                   this, &UISGTMainWindow::on_copyButtonClicked);
  ++row;

  m_nameResultLabel = new QLabel("name result: (none)", m_central);
  m_nameResultLabel->setObjectName("nameResultLabel");
  layout->addWidget(m_nameResultLabel, row, 0, 1, 3);
  ++row;

  m_featureLabel = new QLabel(featureLabelText(), m_central);
  m_featureLabel->setObjectName("featureLabel");
  layout->addWidget(m_featureLabel, row, 0, 1, 3);
  ++row;

  m_delayButton = new QPushButton("&Delayed Update", m_central);
  m_delayButton->setObjectName("delayButton");
  layout->addWidget(m_delayButton, row, 0);
  QObject::connect(m_delayButton, &QPushButton::clicked,
                   this, &UISGTMainWindow::on_delayButtonClicked);

  m_delayLabel = new QLabel(delayLabelText(), m_central);
  m_delayLabel->setObjectName("delayLabel");
  layout->addWidget(m_delayLabel, row, 1, 1, 2);
  ++row;

  m_delayTimer = new QTimer(this);
  m_delayTimer->setSingleShot(true);
  QObject::connect(m_delayTimer, &QTimer::timeout,
                   this, &UISGTMainWindow::on_delayTimerTimeout);

  m_messageLabel = new QLabel("message: (none)", m_central);
  m_messageLabel->setObjectName("messageLabel");
  layout->addWidget(m_messageLabel, row, 0, 1, 3);
  ++row;

  QLabel *colorLabel = new QLabel("Color:", m_central);
  colorLabel->setObjectName("colorLabel");
  layout->addWidget(colorLabel, row, 0);

  m_colorCombo = new QComboBox(m_central);
  m_colorCombo->setObjectName("colorCombo");
  m_colorCombo->addItem("Red");
  m_colorCombo->addItem("Green");
  m_colorCombo->addItem("Blue");
  colorLabel->setBuddy(m_colorCombo);
  layout->addWidget(m_colorCombo, row, 1);
  ++row;

  m_keyLogLabel =
    new QLabel(UISGTKeyLogWidget::initialLabelText(), m_central);
  m_keyLogLabel->setObjectName("keyLogLabel");
  m_keyLog = new UISGTKeyLogWidget(m_keyLogLabel, m_central);
  m_keyLog->setObjectName("keyLog");
  layout->addWidget(m_keyLog, row, 0);
  layout->addWidget(m_keyLogLabel, row, 1, 1, 2);
  ++row;

  m_mouseLabel =
    new QLabel(UISGTMousePad::initialLabelText(), m_central);
  m_mouseLabel->setObjectName("mouseLabel");
  m_mousePad = new UISGTMousePad(m_mouseLabel, m_central);
  m_mousePad->setObjectName("mousePad");
  layout->addWidget(m_mousePad, row, 0);
  layout->addWidget(m_mouseLabel, row, 1, 1, 2);
  ++row;

  m_notes = new QTextEdit(m_central);
  m_notes->setObjectName("notes");
  m_notes->setAcceptRichText(false);
  layout->addWidget(m_notes, row, 0, 1, 3);

  // Give all extra space to `m_notes`, so that `ResizeEvent` can set
  // its size exactly.
  layout->setRowStretch(row, 1);
  layout->setColumnStretch(1, 1);
  ++row;

  // Start with the focus on the button.
  m_clickButton->setFocus();
}


UISGTMainWindow::~UISGTMainWindow()
{
  // See doc/signals-and-dtors.txt.
  QObject::disconnect(m_clickButton, nullptr, this, nullptr);
  QObject::disconnect(m_copyButton, nullptr, this, nullptr);
  QObject::disconnect(m_delayButton, nullptr, this, nullptr);
  QObject::disconnect(m_delayTimer, nullptr, this, nullptr);
  QObject::disconnect(m_quitAction, nullptr, this, nullptr);
  QObject::disconnect(m_featureAction, nullptr, this, nullptr);
  QObject::disconnect(m_resetAction, nullptr, this, nullptr);
}


void UISGTMainWindow::buildMenus()
{
  menuBar()->setObjectName("menuBar");

  {
    QMenu *menu = addMenu("&File", "fileMenu");

    m_quitAction = addAction(menu, "&Quit", "quitAction", "Ctrl+Q");
    QObject::connect(m_quitAction, &QAction::triggered,
                     this, &UISGTMainWindow::on_quitActionTriggered);
  }

  {
    QMenu *menu = addMenu("&Options", "optionsMenu");

    m_featureAction =
      addAction(menu, "&Feature", "featureAction", "Ctrl+E");
    m_featureAction->setCheckable(true);
    QObject::connect(m_featureAction, &QAction::toggled,
                     this, &UISGTMainWindow::on_featureActionToggled);

    m_resetAction = addAction(menu, "&Reset", "resetAction", "Ctrl+R");
    QObject::connect(m_resetAction, &QAction::triggered,
                     this, &UISGTMainWindow::on_resetActionTriggered);
  }
}


QMenu *UISGTMainWindow::addMenu(char const *title, char const *name)
{
  QMenu *menu = menuBar()->addMenu(title);
  menu->setObjectName(name);
  return menu;
}


QAction *UISGTMainWindow::addAction(
  QMenu *menu, char const *title, char const *name,
  char const *shortcut)
{
  // The action is a child of `menu`, so its path goes through it.
  QAction *action = menu->addAction(title);
  action->setObjectName(name);
  action->setShortcut(QKeySequence(shortcut));
  return action;
}


QString UISGTMainWindow::clickCountLabelText() const
{
  return QString("clicks: %1").arg(m_clickCount);
}


QString UISGTMainWindow::featureLabelText() const
{
  bool const on = m_featureAction && m_featureAction->isChecked();
  return on? "feature: on" : "feature: off";
}


QString UISGTMainWindow::delayLabelText() const
{
  return qstringb("delayed: " << delayStateString(m_delayState));
}


/*static*/ char const *UISGTMainWindow::delayStateString(
  DelayState state)
{
  switch (state) {
    case DS_IDLE:    return "idle";
    case DS_PENDING: return "pending";
    case DS_DONE:    return "done";
  }
  xfailure("bad DelayState");
}


void UISGTMainWindow::selfCheck() const
{
  xassert(m_clickCount >= 0);
  xassert(m_clickCountLabel->text() == clickCountLabelText());
  xassert(m_featureLabel->text() == featureLabelText());
  xassert(m_delayLabel->text() == delayLabelText());
  xassert(m_delayTimer->isActive() == (m_delayState == DS_PENDING));
}


GDValue UISGTMainWindow::uiShellQuery(std::string const &state)
{
  if (state == "clickCount") {
    return GDValue(m_clickCount);
  }
  else if (state == "featureOn") {
    return GDValue(m_featureAction->isChecked());
  }
  else if (state == "delayState") {
    return GDValue(delayStateString(m_delayState));
  }
  else {
    return UIShellGUIQueryable::uiShellQuery(state);
  }
}


QImage UISGTMainWindow::uiShellImage(std::string const &what)
{
  if (what == "swatch") {
    // An image with no text, so it does not depend on fonts.
    static Qt::GlobalColor const colors[] = {
      Qt::red,
      Qt::green,
      Qt::blue,
    };
    int const index = m_colorCombo->currentIndex();
    xassert(cc::z_le_lt(index, TABLESIZE(colors)));

    QImage image(8, 8, QImage::Format_RGB32);
    image.fill(colors[index]);
    return image;
  }
  else {
    return UIShellGUIQueryable::uiShellImage(what);
  }
}


void UISGTMainWindow::on_clickButtonClicked() NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  ++m_clickCount;
  m_clickCountLabel->setText(clickCountLabelText());

  GENERIC_CATCH_END
}


void UISGTMainWindow::on_copyButtonClicked() NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  QApplication::clipboard()->setText(m_nameEdit->text());

  GENERIC_CATCH_END
}


void UISGTMainWindow::on_delayButtonClicked() NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  m_delayState = DS_PENDING;
  m_delayLabel->setText(delayLabelText());
  m_delayTimer->start(300 /*ms*/);

  GENERIC_CATCH_END
}


void UISGTMainWindow::on_delayTimerTimeout() NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  m_delayState = DS_DONE;
  m_delayLabel->setText(delayLabelText());

  GENERIC_CATCH_END
}


void UISGTMainWindow::on_quitActionTriggered() NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  QApplication::closeAllWindows();

  GENERIC_CATCH_END
}


void UISGTMainWindow::on_featureActionToggled(bool) NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  m_featureLabel->setText(featureLabelText());

  GENERIC_CATCH_END
}


void UISGTMainWindow::on_resetActionTriggered() NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  m_clickCount = 0;
  m_clickCountLabel->setText(clickCountLabelText());

  m_delayTimer->stop();
  m_delayState = DS_IDLE;
  m_delayLabel->setText(delayLabelText());

  m_nameResultLabel->setText("name result: (none)");
  m_messageLabel->setText("message: (none)");
  m_keyLogLabel->setText(UISGTKeyLogWidget::initialLabelText());
  m_mouseLabel->setText(UISGTMousePad::initialLabelText());

  GENERIC_CATCH_END
}


// ------------------------ UISGTCommandHandler ------------------------
OPEN_ANONYMOUS_NAMESPACE


/* Adds this command to `UIShellGUICommandHandler`:

     CheckFails(<check> "<regex>")
       Execute <check>, which must be a Check* or WaitUntilCheck*
       command, and fail unless it throws an exception whose message
       matches <regex>.  Also fail if it leaves pending steps.

   The .uis scripts can only express tests that pass, so this lets a
   script confirm that a check reports a wrong expected value.
*/
class UISGTCommandHandler : public UIShellGUICommandHandler {
private:     // funcs
  void checkFails(GDValue const &command, std::string const &expectRE)
  {
    GDValueParser commandParser(command);
    commandParser.checkIsTuple();
    std::string const innerName(
      commandParser.taggedContainerGetTagName());
    if (!beginsWith(innerName, "Check") &&
        !beginsWith(innerName, "WaitUntilCheck")) {
      xmessagesb("CheckFails: not a check command: " <<
                 doubleQuote(innerName));
    }

    std::optional<std::string> failure;
    try {
      executeCommand(command);
    }
    catch (std::exception &x) {
      failure = x.what();
    }

    if (hasPendingStep()) {
      discardPendingSteps();
      xmessagesb("CheckFails: " << innerName << " left pending steps.");
    }

    if (!failure) {
      xmessagesb("CheckFails: " << innerName <<
                 " succeeded, but was expected to fail.");
    }

    if (!regexSearch(*failure, expectRE)) {
      xmessagesb("CheckFails: " << innerName << " failed with " <<
                 doubleQuote(*failure) <<
                 ", which does not match the regex " <<
                 doubleQuote(expectRE) << ".");
    }
  }

protected:   // funcs
  virtual GDValue executeDerivedCommand(
    std::string const &funcName,
    GDValueParser const &parser) OVERRIDE
  {
    if (funcName == "CheckFails") {
      auto [command, expectRE] =
        gdvpToTuple<GDValue, std::string>(parser);
      checkFails(command, expectRE);
      return GDValue();
    }

    else {
      return UIShellGUICommandHandler::executeDerivedCommand(
        funcName, parser);
    }
  }
};


CLOSE_ANONYMOUS_NAMESPACE


// ------------------------------- main --------------------------------
static void printUsage(char const *argv0)
{
  std::cout << "usage: " << argv0 << " [options]\n"
               "\n"
               "options:\n"
               "  -help           Print this message and exit.\n"
               "\n"
               "UI shell options:\n"
            << UIShellOptions::helpText();
}


static int innerMain(int argc, char **argv)
{
  try {
    // Suppress "Unable to set geometry", and provide advice about
    // "platform plugin" errors.
    installSMQtUtilMessageHandler();

    QApplication app(argc, argv);

    UIShellOptions uisOptions;
    for (int i=1; i < argc; ++i) {
      std::string const arg(argv[i]);
      if (uisOptions.processOption(arg)) {
        // Handled.
      }
      else if (arg == "-help") {
        printUsage(argv[0]);
        return 0;
      }
      else {
        xformatsb("Unknown argument: " << doubleQuote(arg) <<
                  ".  Try -help.");
      }
    }

    if (uisOptions.hasScript() && !uisOptions.m_script.m_args.empty()) {
      xformat("This program does not accept `args` in a .uis file.");
    }

    UISGTMainWindow mainWindow;
    showRaiseAndActivateWindow(&mainWindow);

    std::unique_ptr<EventRecorder> recorder;
    if (uisOptions.m_recordInputEvents) {
      recorder.reset(new EventRecorder("recorded.uis"));
    }

    if (uisOptions.hasScript()) {
      UISGTCommandHandler handler;
      return runUIShellScript(app, uisOptions, handler,
        [&mainWindow]() -> void { mainWindow.selfCheck(); });
    }
    else {
      return app.exec();
    }
  }
  catch (XBase &x) {
    std::cerr << x.why() << std::endl;
    return 2;
  }
}


int main(int argc, char **argv)
{
  if (std::optional<int> code =
        runUIShellTestCommandsIfRequested(argc, argv, innerMain)) {
    return *code;
  }
  return innerMain(argc, argv);
}


// EOF
