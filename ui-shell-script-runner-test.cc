// ui-shell-script-runner-test.cc
// Tests for `ui-shell-script-runner` module.

// See license.txt for copyright and terms of use.

#include "ui-shell-script-runner-test.h"         // helper classes for this module
#include "ui-shell-script-runner.h"              // module under test
#include "unit-tests.h"                          // decl for my entry point

// editor
#include "ui-shell-test-handler.h"               // SequencingTestHandler

// smbase
#include "smbase/exc.h"                          // smbase::XMessage, xmessage, xfatal, GENERIC_CATCH_BEGIN/END
#include "smbase/gdvalue-parser-fwd.h"           // gdv::GDValueParser [f]
#include "smbase/gdvalue.h"                      // gdv::GDValue, gdv::fromGDVN
#include "smbase/sm-macros.h"                    // OPEN_ANONYMOUS_NAMESPACE, IMEMBFP
#include "smbase/sm-override.h"                  // OVERRIDE
#include "smbase/sm-test.h"                      // EXPECT_EQ, EXPECT_MATCHES_REGEX, EXPECT_EXN_REGEX
#include "smbase/sm-trace.h"                     // INIT_TRACE, etc.

// Qt
#include <QMetaObject>
#include <QTimer>

// libc++
#include <cstdlib>                               // std::abort
#include <iostream>                              // std::{cerr, endl}
#include <optional>                              // std::optional
#include <string>                                // std::string
#include <vector>                                // std::vector

using namespace gdv;
using namespace smbase;


INIT_TRACE("ui-shell-script-runner-test");


// -------------------------- NestedLoopHelper -------------------------
NestedLoopHelper::NestedLoopHelper(std::vector<std::string> &log)
:
  QObject(),
  IMEMBFP(log),
  m_nestedLoop()
{}


NestedLoopHelper::~NestedLoopHelper()
{}


void NestedLoopHelper::slot_runNestedLoop() NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  m_log.push_back("nested start");
  m_nestedLoop.exec();
  m_log.push_back("nested end");

  GENERIC_CATCH_END
}


void NestedLoopHelper::slot_timeout() NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  // Don't throw here since we're inside `NOEXCEPT`.
  std::cerr << "error: runTest timed out, aborting" << std::endl;
  std::abort();

  GENERIC_CATCH_END
}


// ------------------------- RunnerTestHandler -------------------------
OPEN_ANONYMOUS_NAMESPACE


/* Adds these commands to `SequencingTestHandler`:

     StartNestedLoop()  Queue a call to `slot_runNestedLoop`.  Like
                        ClickButton, this posts rather than running the
                        loop directly, since a loop started inside
                        `executeCommand` would block the executor.
     ExitNestedLoop()   Quit the nested loop.
*/
class RunnerTestHandler : public SequencingTestHandler {
public:      // data
  // Runs the nested loop.
  NestedLoopHelper &m_helper;

protected:   // funcs
  virtual GDValue executeDerivedCommand(
    std::string const &funcName,
    GDValueParser const &parser) OVERRIDE
  {
    if (funcName == "StartNestedLoop") {
      m_log.push_back("command " + funcName);
      QMetaObject::invokeMethod(
        &m_helper, &NestedLoopHelper::slot_runNestedLoop,
        Qt::QueuedConnection);
      return GDValue();
    }

    else if (funcName == "ExitNestedLoop") {
      m_log.push_back("command " + funcName);
      m_helper.m_nestedLoop.quit();
      return GDValue();
    }

    else {
      return SequencingTestHandler::executeDerivedCommand(
        funcName, parser);
    }
  }

public:      // funcs
  RunnerTestHandler(
    std::vector<std::string> &log,
    NestedLoopHelper &helper)
  :
    SequencingTestHandler(log),
    IMEMBFP(helper)
  {}
};


// ------------------------------ Fixture ------------------------------
// The objects needed to run a script.
class Fixture {
  NO_OBJECT_COPIES(Fixture);

public:      // data
  // Log shared by the handler and the helper.
  std::vector<std::string> m_log;

  // Number of times the global self-check has run.
  int m_selfCheckCount;

  // If not 0, the self-check call with this number (1-based) throws.
  int m_selfCheckThrowAt;

  // Helper for the nested-loop commands and the timeout.
  NestedLoopHelper m_helper;

  // Command handler for the runner.
  RunnerTestHandler m_handler;

  // Commands of the most recent `run`.  The runner keeps a reference
  // to this, so it is a member.
  std::vector<GDValue> m_commands;

public:      // funcs
  Fixture()
    : m_log(),
      m_selfCheckCount(0),
      m_selfCheckThrowAt(0),
      m_helper(m_log),
      m_handler(m_log, m_helper),
      m_commands()
  {}

  void globalSelfCheck()
  {
    ++m_selfCheckCount;
    m_log.push_back("selfCheck");
    if (m_selfCheckCount == m_selfCheckThrowAt) {
      xmessage("global self-check failed");
    }
  }

  // Run the GDVN sequence of commands `commandsGDVN`, and return the
  // runner's result.
  std::optional<std::string> run(std::string const &commandsGDVN)
  {
    m_commands = fromGDVN(commandsGDVN).sequenceGet();

    UIShellScriptRunner runner(
      m_handler, m_commands, [this]() { this->globalSelfCheck(); });

    // Hang protection.
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(&timer, &QTimer::timeout,
                     &m_helper, &NestedLoopHelper::slot_timeout);
    timer.start(5000 /*ms*/);

    return runner.runTest();
  }

  // The log entries, separated by " | ".  If `withSelfChecks` is
  // false, omit the "selfCheck" entries.
  std::string logString(bool withSelfChecks = true) const
  {
    std::string ret;
    for (std::string const &entry : m_log) {
      if (!withSelfChecks && entry == "selfCheck") {
        continue;
      }
      if (!ret.empty()) {
        ret += " | ";
      }
      ret += entry;
    }
    return ret;
  }
};


// ------------------------------ helpers ------------------------------
// Check that `res` indicates success.
void expectSuccess(std::optional<std::string> const &res)
{
  if (res.has_value()) {
    xfailure_stringbc("unexpected failure: " << *res);
  }
}


// Check that `res` is a failure whose message matches `expectRE`.
void expectFailure(
  std::optional<std::string> const &res,
  char const *expectRE)
{
  EXPECT_TRUE(res.has_value());
  EXPECT_MATCHES_REGEX(*res, expectRE);
}


// ------------------------------- tests -------------------------------
void test_success()
{
  Fixture f;
  expectSuccess(f.run("[Mark(\"a\") Mark(\"b\") Mark(\"c\")]"));
  EXPECT_EQ(f.logString(),
    "selfCheck | mark a | selfCheck | mark b | selfCheck | "
    "mark c | selfCheck");
}


void test_empty()
{
  Fixture f;
  expectSuccess(f.run("[]"));

  // The runner substitutes `Sleep(0)`, which the base class handles
  // without logging.
  EXPECT_EQ(f.logString(), "selfCheck | selfCheck");
}


void test_failureStops()
{
  Fixture f;
  expectFailure(f.run("[Mark(\"a\") Throw(\"x\") Mark(\"c\")]"),
                "funcName=Throw: x$");
  EXPECT_EQ(f.logString(),
    "selfCheck | mark a | selfCheck | command Throw | discard");
}


void test_failingStep()
{
  Fixture f;
  expectFailure(f.run("[StepThrows(3 2) Mark(\"c\")]"),
                "step 2 failed$");
  EXPECT_EQ(f.logString(false),
    "command StepThrows | step 1 | step 2 | discard");
}


void test_stepsBeforeNextCommand()
{
  Fixture f;
  expectSuccess(f.run("[Steps(2) Mark(\"c\")]"));
  EXPECT_EQ(f.logString(),
    "selfCheck | command Steps | selfCheck | step 1 | selfCheck | "
    "step 2 | selfCheck | mark c | selfCheck");
}


void test_selfCheckFailsAfterCommand()
{
  Fixture f;

  // The first call is the one `runTest` makes before any command.
  f.m_selfCheckThrowAt = 2;

  expectFailure(f.run("[Mark(\"a\") Mark(\"b\")]"),
                "global self-check failed$");
  EXPECT_EQ(f.logString(),
    "selfCheck | mark a | selfCheck | discard");
}


void test_selfCheckFailsAtStart()
{
  Fixture f;
  f.m_selfCheckThrowAt = 1;

  // `runTest` calls the self-check directly, outside the executor, so
  // the exception propagates.
  EXPECT_EXN_REGEX(f.run("[Mark(\"a\")]"), XMessage,
                   "global self-check failed");
  EXPECT_EQ(f.logString(), "selfCheck");
}


void test_nestedLoop()
{
  Fixture f;
  expectSuccess(f.run(
    "[Mark(\"a\") StartNestedLoop() Mark(\"b\") "
    "ExitNestedLoop() Mark(\"c\")]"));
  EXPECT_EQ(f.logString(false),
    "mark a | command StartNestedLoop | nested start | mark b | "
    "command ExitNestedLoop | nested end | mark c");
}


void test_twoRunners()
{
  Fixture f;
  expectSuccess(f.run("[Mark(\"a\")]"));
  expectSuccess(f.run("[Mark(\"b\")]"));
  EXPECT_EQ(f.logString(false), "mark a | mark b");
}


CLOSE_ANONYMOUS_NAMESPACE


// Called from unit-tests.cc.
void test_ui_shell_script_runner(CmdlineArgsSpan args)
{
  test_success();
  test_empty();
  test_failureStops();
  test_failingStep();
  test_stepsBeforeNextCommand();
  test_selfCheckFailsAfterCommand();
  test_selfCheckFailsAtStart();
  test_nestedLoop();
  test_twoRunners();
}


// EOF
