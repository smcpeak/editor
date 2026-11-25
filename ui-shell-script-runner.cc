// ui-shell-script-runner.cc
// Code for `ui-shell-script-runner` module.

#include "ui-shell-script-runner.h"    // this module

// editor
#include "ui-shell-result.h"           // UIShellResult

// smbase
#include "smbase/exc.h"                // GENERIC_CATCH_BEGIN/END
#include "smbase/gdvalue.h"            // gdv::GDValue
#include "smbase/sm-macros.h"          // IMEMBFP
#include "smbase/sm-trace.h"           // INIT_TRACE, etc.
#include "smbase/string-util.h"        // doubleQuote
#include "smbase/xassert.h"            // xassertPrecondition

// libc++
#include <functional>                  // std::function
#include <iostream>                    // std::cout
#include <optional>                    // std::optional
#include <string>                      // std::string

using namespace gdv;
using namespace smbase;


INIT_TRACE("ui-shell-script-runner");


UIShellScriptRunner::UIShellScriptRunner(
  UIShellCommandHandler &handler,
  std::vector<gdv::GDValue> const &testCommands,
  std::function<void ()> globalSelfCheck)
:
  QObject(),
  m_printFailureToStdout(false),
  IMEMBFP(testCommands),
  m_nextTestCommandIndex(0),
  m_testFailureMessage(),
  m_eventLoop(),
  IMEMBFP(globalSelfCheck),
  m_executor(handler, globalSelfCheck)
{
  // We use a direct (rather than queued) connection so that, when the
  // slot starts the next command, the executor runs it in the same
  // quiescence cycle.  But this makes no measurable difference in
  // performance.
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
    if (m_printFailureToStdout) {
      std::cout << "test FAILED: " << *m_testFailureMessage << std::endl;
    }
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


// EOF
