// ui-shell-app.cc
// Code for `ui-shell-app` module.

// See license.txt for copyright and terms of use.

#include "ui-shell-app.h"                        // this module

// editor
#include "ui-shell-script-runner.h"              // UIShellScriptRunner

// smqtutil
#include "smqtutil/silent-mbox-filter.h"         // SilentMessageBoxFilter

// smbase
#include "smbase/dev-warning.h"                  // g_abortUponDevWarning
#include "smbase/exc.h"                          // smbase::xformat, EXN_CONTEXT
#include "smbase/gdvalue-parser.h"               // gdv::{GDValueParser, gdvpTo}
#include "smbase/gdvalue-vector.h"               // gdv::gdvpTo(std::vector)
#include "smbase/gdvalue.h"                      // gdv::GDValue
#include "smbase/htt.h"                          // HTT_TASK_DYNAMIC_LABEL
#include "smbase/sm-env.h"                       // smbase::envAsIntOr
#include "smbase/string-util.h"                  // beginsWith, shellDoubleQuoteCommand
#include "smbase/xassert.h"                      // xassertPrecondition

// Qt
#include <QApplication>

// libc++
#include <cstdlib>                               // std::{atoi, getenv}
#include <cstring>                               // std::{memcpy, strlen}
#include <exception>                             // std::exception
#include <iostream>                              // std::{cout, endl}
#include <optional>                              // std::optional
#include <string>                                // std::string
#include <vector>                                // std::vector

using namespace gdv;
using namespace smbase;


// -------------------------- UIShellOptions ---------------------------
UIShellOptions::UIShellOptions()
  : m_scriptFileName(),
    m_script(),
    m_recordInputEvents(false)
{}


UIShellOptions::~UIShellOptions()
{}


char const *UIShellOptions::helpText()
{
  return
    "  -uisCmds=file.uis\n"
    "                  Execute UI shell commands in file.uis, then quit.\n"
    "  -testCommands=tests.gdvn\n"
    "                  Execute all tests in tests.gdvn.  This must be\n"
    "                  the first argument.\n"
    "  -record         Record events to recorded.uis.\n"
    "\n"
    "With -uisCmds, set envvar NOQUIT=1 to stop if failure and NOQUIT=0 to\n"
    "stop after execution regardless of failure.\n"
    ;
}


bool UIShellOptions::processOption(std::string const &arg)
{
  if (beginsWith(arg, "-uisCmds=")) {
    // Execute a sequence of UI shell commands as part of a test.
    if (hasScript()) {
      xformat("Multiple \"-uisCmds=\" options are not allowed.");
    }
    m_scriptFileName = arg.substr(std::strlen("-uisCmds="));
    m_script = UIShellScriptFile(m_scriptFileName);
    return true;
  }

  else if (beginsWith(arg, "-testCommands=")) {
    xformat("\"-testCommands=\" must be the first argument.");
  }

  else if (arg == "-record") {
    // Record events to seed a new test.
    m_recordInputEvents = true;
    return true;
  }

  else {
    return false;
  }
}


bool UIShellOptions::hasScript() const
{
  return !m_scriptFileName.empty();
}


bool UIShellOptions::isTestMode() const
{
  return hasScript() || m_recordInputEvents;
}


// ------------------------- runUIShellScript --------------------------
int runUIShellScript(
  QApplication &app,
  UIShellOptions const &options,
  UIShellCommandHandler &handler,
  std::function<void ()> globalSelfCheck)
{
  xassertPrecondition(options.hasScript());

  g_abortUponDevWarning = true;
  std::cout << "running test: " << options.m_scriptFileName
            << std::endl;

  // Cause the UI shell script name to be included in any failure
  // message.
  EXN_CONTEXT(options.m_scriptFileName);
  HTT_TASK_DYNAMIC_LABEL(options.m_scriptFileName);

  // Disable message box sounds since they are annoying to listen to
  // while the automated tests run.
  SilentMessageBoxFilter silentFilter(&app);

  int ret;
  UIShellScriptRunner runner(
    handler, options.m_script.m_commands, globalSelfCheck);
  runner.m_printFailureToStdout = true;
  std::optional<std::string> error = runner.runTest();
  if (!error.has_value()) {
    std::cout << "test passed" << std::endl;

    // Check all invariants before declaring victory.  (This is
    // redundant with the calls to `globalSelfCheck` done during
    // execution, but harmless.)
    globalSelfCheck();

    ret = 0;
  }
  else {
    // In the failure case, the runner prints the failure due to
    // setting its `m_printFailureToStdout`.
    ret = 2;
  }

  // TODO: This is a bizarre interface.  Improve it.
  if (char const *noQuit = std::getenv("NOQUIT")) {
    // If we are going to return at least $NOQUIT, keep the app running
    // so I can inspect its state.  Often I will use NOQUIT=1 to stop
    // iff there is a test failure.  NOQUIT=0 will stop
    // unconditionally.
    if (ret >= std::atoi(noQuit)) {
      std::cout << "leaving app running due to NOQUIT" << std::endl;
      app.exec();
    }
  }

  return ret;
}


// ----------------- runUIShellTestCommandsIfRequested -----------------
// Return a dynamically-allocated, NUL-terminated array holding the
// contents of `s`.
static char *dupString(std::string const &s)
{
  char *ret = new char[s.size() + 1];
  std::memcpy(ret, s.data(), s.size());
  ret[s.size()] = 0;
  return ret;
}


// Run `command` as if it were this program's `argc/argv`, by calling
// `innerMain`.  Return non-zero if the attempt fails.
static int runOneCommand(
  std::vector<std::string> const &command,
  std::function<int (int argc, char **argv)> const &innerMain)
{
  std::cout << "Command: "
            << shellDoubleQuoteCommand(command) << "\n";

  // The tests unfortunately have some race conditions I have not been
  // able to fully eliminate, so optionally retry.
  //
  // 2025-09-01: It has been a while since I saw a failure.  Let's try
  // setting the default value to 1 so we do not retry at all.
  int attempts = 0;
  int const retryLimit = envAsIntOr(1, "UI_SHELL_TEST_RETRIES");

  while (true) {
    ++attempts;

    // The `argc/argv` passed to the `QApplication` ctor must be
    // mutable, so make copies of everything.
    char **argv = new char*[command.size()+1];
    std::vector<char*> argvPointers;
    for (std::size_t i=0; i < command.size(); ++i) {
      argv[i] = dupString(command.at(i));
      argvPointers.push_back(argv[i]);
    }
    argv[command.size()] = nullptr;

    // Run the constructed command line.
    int result = innerMain(command.size(), argv);

    // Free the `argv` array.
    delete[] argv;

    // Free the individual strings.  We do not read `argv` to find them
    // because `QApplication` can modify the `argv` array.
    for (char *p : argvPointers) {
      delete[] p;
    }

    if (result == 0) {
      // Test passed.
      return 0;
    }
    else {
      if (attempts < retryLimit) {
        std::cout << "Attempt " << attempts << " of " << retryLimit
                  << " failed (code=" << result << "), retrying...\n";
      }
      else {
        std::cout << "All " << retryLimit
                  << " attempts failed, stopping with code "
                  << result << "\n";
        return result;
      }
    }
  }

  // Not reached.
}


// Read `cmdsFname` as GDVN and treat its contents as a sequence of
// command lines to run, in sequence, with `innerMain`.  The
// expectation is these are automated GUI tests.
//
// It is faster to run all the tests in one process if possible rather
// than starting a new process for each.
static int runCommandList(
  std::string const &cmdsFname,
  std::function<int (int argc, char **argv)> const &innerMain)
{
  try {
    std::cout << "Executing commands from " << cmdsFname << "\n";

    // Parse the file as GDVN and parse the GDV as a sequence of
    // sequences of strings.
    GDValue commandGDV = GDValue::readFromFile(cmdsFname);
    std::vector<std::vector<std::string>> allCommands =
      gdvpTo<decltype(allCommands)>(GDValueParser(commandGDV));

    // Treat each sequence of strings as a command line.
    for (std::vector<std::string> const &command : allCommands) {
      int result = runOneCommand(command, innerMain);
      if (result != 0) {
        return result;
      }
    }

    // All tests passed.
    return 0;
  }

  catch (std::exception &x) {
    // At least for now, if a test fails with an exception, I do not
    // treat that as retryable.
    std::cout << "Test failed: " << x.what() << "\n";
    return 2;
  }
}


std::optional<int> runUIShellTestCommandsIfRequested(
  int argc, char **argv,
  std::function<int (int argc, char **argv)> innerMain)
{
  if (argc >= 2) {
    std::string firstArg(argv[1]);
    char const *prefix = "-testCommands=";
    if (beginsWith(firstArg, prefix)) {
      std::string cmdsFname = firstArg.substr(std::strlen(prefix));
      return runCommandList(cmdsFname, innerMain);
    }
  }

  return std::nullopt;
}


// EOF
