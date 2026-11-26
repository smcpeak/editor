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
#include "smqtutil/qtguiutil.h"                  // showRaiseAndActivateWindow
#include "smqtutil/qtutil.h"                     // installSMQtUtilMessageHandler, toString

// smbase
#include "smbase/exc.h"                          // smbase::{XBase, xformatsb, xmessagesb}, GENERIC_CATCH_BEGIN/END
#include "smbase/gdvalue-parser.h"               // gdv::GDValueParser
#include "smbase/gdvalue-tuple.h"                // gdv::gdvpToTuple
#include "smbase/gdvalue.h"                      // gdv::GDValue
#include "smbase/sm-macros.h"                    // OPEN_ANONYMOUS_NAMESPACE
#include "smbase/string-util.h"                  // beginsWith, doubleQuote
#include "smbase/xassert.h"                      // xassert

// Qt
#include <QApplication>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QString>

// libc++
#include <exception>                             // std::exception
#include <iostream>                              // std::{cout, cerr, endl}
#include <memory>                                // std::unique_ptr
#include <optional>                              // std::optional
#include <string>                                // std::string

using namespace gdv;
using namespace smbase;


// -------------------------- UISGTMainWindow --------------------------
UISGTMainWindow::UISGTMainWindow()
  : QMainWindow(),
    UIShellGUIQueryable(),
    m_clickCount(0),
    m_central(nullptr),
    m_clickButton(nullptr),
    m_clickCountLabel(nullptr)
{
  setObjectName("mainWindow");
  setWindowTitle("UI Shell GUI Test");

  m_central = new QWidget(this);
  m_central->setObjectName("central");
  setCentralWidget(m_central);

  // Widgets are created as direct children of `m_central` so their
  // paths are short.
  QGridLayout *layout = new QGridLayout(m_central);

  m_clickButton = new QPushButton("&Click Me", m_central);
  m_clickButton->setObjectName("clickButton");
  layout->addWidget(m_clickButton, 0, 0);
  QObject::connect(m_clickButton, &QPushButton::clicked,
                   this, &UISGTMainWindow::on_clickButtonClicked);

  m_clickCountLabel = new QLabel(clickCountLabelText(), m_central);
  m_clickCountLabel->setObjectName("clickCountLabel");
  layout->addWidget(m_clickCountLabel, 0, 1);

  // Start with the focus on the button.
  m_clickButton->setFocus();
}


UISGTMainWindow::~UISGTMainWindow()
{
  // See doc/signals-and-dtors.txt.
  QObject::disconnect(m_clickButton, nullptr, this, nullptr);
}


QString UISGTMainWindow::clickCountLabelText() const
{
  return QString("clicks: %1").arg(m_clickCount);
}


void UISGTMainWindow::selfCheck() const
{
  xassert(m_clickCount >= 0);
  xassert(m_clickCountLabel->text() == clickCountLabelText());
}


GDValue UISGTMainWindow::uiShellQuery(std::string const &state)
{
  if (state == "clickCount") {
    return GDValue(m_clickCount);
  }
  else {
    return UIShellGUIQueryable::uiShellQuery(state);
  }
}


void UISGTMainWindow::on_clickButtonClicked() NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  ++m_clickCount;
  m_clickCountLabel->setText(clickCountLabelText());

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
