// ui-shell-executor-test.cc
// Tests for `ui-shell-executor` module.

// See license.txt for copyright and terms of use.

#include "ui-shell-executor-test.h"              // helper classes for this module
#include "ui-shell-executor.h"                   // module under test
#include "unit-tests.h"                          // decl for my entry point

// editor
#include "ui-shell-command-handler.h"            // UIShellCommandHandler
#include "waiting-counter.h"                     // g_waitingCounter, adjWaitingCounter

// smbase
#include "smbase/exc.h"                          // xmessagesb, GENERIC_CATCH_BEGIN/END
#include "smbase/gdvalue-parser.h"               // gdv::GDValueParser
#include "smbase/gdvalue-tuple.h"                // gdv::gdvpToTuple
#include "smbase/gdvalue.h"                      // gdv::fromGDVN
#include "smbase/sm-macros.h"                    // OPEN_ANONYMOUS_NAMESPACE, IMEMBFP
#include "smbase/sm-override.h"                  // OVERRIDE
#include "smbase/sm-test.h"                      // EXPECT_EQ, EXPECT_MATCHES_REGEX
#include "smbase/sm-trace.h"                     // INIT_TRACE, etc.
#include "smbase/stringb.h"                      // stringb
#include "smbase/xassert.h"                      // xassert, xfailure

// Qt
#include <QCoreApplication>

// libc++
#include <string>                                // std::string
#include <vector>                                // std::vector

using namespace gdv;
using namespace smbase;


INIT_TRACE("ui-shell-executor-test");


// --------------------------- ExecutorTester --------------------------
ExecutorTester::ExecutorTester(
  UIShellExecutor &executor,
  std::vector<std::string> &log)
:
  QObject(),
  IMEMBFP(executor),
  IMEMBFP(log),
  m_eventLoop(),
  m_timeoutTimer(),
  m_signalCount(0),
  m_busyAtSignal(false),
  m_takeResults(true),
  m_results(),
  m_chainedCommands(),
  m_quitAtSignalCount(0)
{
  QObject::connect(
    &m_executor, &UIShellExecutor::signal_hasCommandResult,
    this, &ExecutorTester::slot_hasCommandResult);

  m_timeoutTimer.setSingleShot(true);
  QObject::connect(&m_timeoutTimer, &QTimer::timeout,
                   this, &ExecutorTester::slot_timeout);
}


ExecutorTester::~ExecutorTester()
{
  // See doc/signals-and-dtors.txt.
  QObject::disconnect(&m_executor, nullptr, this, nullptr);
  QObject::disconnect(&m_timeoutTimer, nullptr, this, nullptr);
}


void ExecutorTester::runCommand(GDValue const &command)
{
  m_quitAtSignalCount =
    m_signalCount + 1 + static_cast<int>(m_chainedCommands.size());

  m_executor.startCommand(command);
  xassert(m_executor.isBusy());
  xassert(!m_executor.hasCommandResult());

  // A correct executor finishes these commands in well under a second.
  m_timeoutTimer.start(5000 /*ms*/);
  m_eventLoop.exec();
  m_timeoutTimer.stop();

  if (m_signalCount != m_quitAtSignalCount) {
    xfailure("timed out waiting for the command result");
  }
}


void ExecutorTester::slot_hasCommandResult() NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  TRACE1("in slot_hasCommandResult");

  ++m_signalCount;
  m_log.push_back("signal");
  m_busyAtSignal = m_executor.isBusy();

  if (m_takeResults) {
    m_results.push_back(m_executor.takeCommandResult());
  }

  if (!m_chainedCommands.empty()) {
    GDValue next(m_chainedCommands.front());
    m_chainedCommands.pop_front();
    m_executor.startCommand(next);
  }
  else if (m_signalCount >= m_quitAtSignalCount) {
    m_eventLoop.quit();
  }

  GENERIC_CATCH_END
}


void ExecutorTester::slot_timeout() NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  TRACE1("in slot_timeout");
  m_eventLoop.quit();

  GENERIC_CATCH_END
}


void ExecutorTester::slot_decrementWaitingCounter() NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  m_log.push_back("decrement");
  adjWaitingCounter(-1);

  GENERIC_CATCH_END
}


OPEN_ANONYMOUS_NAMESPACE


/* Handler that records its calls in a log, and adds these commands:

     Return(v)       Return v.
     Throw("msg")    Throw XMessage("msg").
     Steps(n)        Return null and leave n pending steps.
     StepThrows(n k) Like Steps(n), but step k (1-based) throws.
*/
class SequencingTestHandler : public UIShellCommandHandler {
public:      // data
  // Log of calls, shared with `ExecutorTester`.
  std::vector<std::string> &m_log;

  // Number of the next pending step to perform, 1-based.
  int m_nextStep;

  // Number of steps in the current command.
  int m_numSteps;

  // Step that throws, or 0 for none.
  int m_throwAtStep;

protected:   // funcs
  virtual GDValue executeDerivedCommand(
    std::string const &funcName,
    GDValueParser const &parser) OVERRIDE
  {
    m_log.push_back("command " + funcName);

    if (funcName == "Return") {
      auto [v] = gdvpToTuple<GDValue>(parser);
      return v;
    }

    else if (funcName == "Throw") {
      auto [msg] = gdvpToTuple<std::string>(parser);
      xmessage(msg);
    }

    else if (funcName == "Steps") {
      auto [n] = gdvpToTuple<int>(parser);
      m_nextStep = 1;
      m_numSteps = n;
      m_throwAtStep = 0;
    }

    else if (funcName == "StepThrows") {
      auto [n, k] = gdvpToTuple<int, int>(parser);
      m_nextStep = 1;
      m_numSteps = n;
      m_throwAtStep = k;
    }

    else {
      return UIShellCommandHandler::executeDerivedCommand(
        funcName, parser);
    }

    return GDValue();
  }

public:      // funcs
  explicit SequencingTestHandler(std::vector<std::string> &log)
    : UIShellCommandHandler(),
      m_log(log),
      m_nextStep(1),
      m_numSteps(0),
      m_throwAtStep(0)
  {}

  virtual UIShellQueryable *getQueryableFromPath(
    std::string const &path) OVERRIDE
  {
    xmessagesb("no queryables: " << path);
    return nullptr;    // not reached
  }

  virtual bool hasPendingStep() const OVERRIDE
  {
    return m_nextStep <= m_numSteps;
  }

  virtual void executePendingStep() OVERRIDE
  {
    xassertPrecondition(hasPendingStep());

    int step = m_nextStep++;
    m_log.push_back(stringb("step " << step));

    if (step == m_throwAtStep) {
      xmessagesb("step " << step << " failed");
    }
  }

  virtual void discardPendingSteps() OVERRIDE
  {
    m_log.push_back("discard");
    m_nextStep = 1;
    m_numSteps = 0;
  }
};


// The objects needed to run commands through an executor.
class Fixture {
  NO_OBJECT_COPIES(Fixture);

public:      // data
  // Log shared by the handler and the tester.
  std::vector<std::string> m_log;

  // Number of times the global self-check has run.
  int m_selfCheckCount;

  // If true, the global self-check throws.
  bool m_selfCheckThrows;

  // These are destroyed in the reverse order, so the handler outlives
  // the executor, which outlives the tester.
  SequencingTestHandler m_handler;
  UIShellExecutor m_executor;
  ExecutorTester m_tester;

public:      // funcs
  Fixture()
    : m_log(),
      m_selfCheckCount(0),
      m_selfCheckThrows(false),
      m_handler(m_log),
      m_executor(m_handler, [this]() { this->globalSelfCheck(); }),
      m_tester(m_executor, m_log)
  {}

  void globalSelfCheck()
  {
    ++m_selfCheckCount;
    if (m_selfCheckThrows) {
      xmessage("global self-check failed");
    }
  }

  // Run the command written as `commandGDVN`.
  void run(std::string const &commandGDVN)
  {
    m_tester.runCommand(fromGDVN(commandGDVN));
  }

  // The log entries, separated by " | ".
  std::string logString() const
  {
    std::string ret;
    for (std::string const &entry : m_log) {
      if (!ret.empty()) {
        ret += " | ";
      }
      ret += entry;
    }
    return ret;
  }

  // The only result taken so far.
  UIShellResult const &onlyResult() const
  {
    xassert(m_tester.m_results.size() == 1);
    return m_tester.m_results.at(0);
  }
};


// Check that `res` is a success with value `expect`.
void expectLeft(UIShellResult const &res, GDValue const &expect)
{
  EXPECT_TRUE(res.isLeft());
  EXPECT_EQ(res.left(), expect);
}


// Check that `res` is an error whose message matches `expectRE`.
void expectRight(UIShellResult const &res, char const *expectRE)
{
  EXPECT_TRUE(res.isRight());
  EXPECT_MATCHES_REGEX(res.right(), expectRE);
}


void test_return()
{
  Fixture f;
  f.run("Return(3)");

  expectLeft(f.onlyResult(), 3);
  EXPECT_EQ(f.m_tester.m_signalCount, 1);
  EXPECT_FALSE(f.m_tester.m_busyAtSignal);
  EXPECT_FALSE(f.m_executor.isBusy());
  EXPECT_FALSE(f.m_executor.hasCommandResult());
  EXPECT_EQ(f.logString(), "command Return | signal");
  EXPECT_EQ(f.m_selfCheckCount, 1);
}


void test_throw()
{
  Fixture f;
  f.run("Throw(\"x\")");

  expectRight(f.onlyResult(), "funcName=Throw: x$");
  EXPECT_EQ(f.logString(), "command Throw | discard | signal");
}


void test_steps()
{
  Fixture f;
  f.run("Steps(3)");

  expectLeft(f.onlyResult(), GDValue());
  EXPECT_EQ(f.logString(),
    "command Steps | step 1 | step 2 | step 3 | signal");
  EXPECT_EQ(f.m_selfCheckCount, 4);
}


void test_stepThrows()
{
  Fixture f;
  f.run("StepThrows(3 1)");

  expectRight(f.onlyResult(), "step 1 failed$");
  EXPECT_EQ(f.logString(),
    "command StepThrows | step 1 | discard | signal");
  EXPECT_FALSE(f.m_handler.hasPendingStep());
}


void test_globalSelfCheckThrows()
{
  Fixture f;
  f.m_selfCheckThrows = true;
  f.run("Return(3)");

  expectRight(f.onlyResult(), "global self-check failed$");
  EXPECT_EQ(f.logString(), "command Return | discard | signal");
}


void test_chaining()
{
  Fixture f;
  f.m_tester.m_chainedCommands.push_back(fromGDVN("Return(2)"));
  f.run("Return(1)");

  EXPECT_EQ(f.m_tester.m_results.size(), 2);
  expectLeft(f.m_tester.m_results.at(0), 1);
  expectLeft(f.m_tester.m_results.at(1), 2);
  EXPECT_EQ(f.logString(),
    "command Return | signal | command Return | signal");
}


void test_untakenResultDiscarded()
{
  Fixture f;
  f.m_tester.m_takeResults = false;
  f.run("Return(1)");
  EXPECT_TRUE(f.m_executor.hasCommandResult());

  // `runCommand` checks that `startCommand` discards the old result.
  f.m_tester.m_takeResults = true;
  f.run("Return(2)");

  expectLeft(f.onlyResult(), 2);
}


void test_idle()
{
  Fixture f;
  f.run("Return(1)");
  std::string const logAfterCommand = f.logString();

  // Nothing should happen while no command is active.
  for (int i=0; i < 5; ++i) {
    QCoreApplication::processEvents();
  }

  EXPECT_EQ(f.logString(), logAfterCommand);
  EXPECT_EQ(f.m_tester.m_signalCount, 1);
  EXPECT_EQ(f.m_selfCheckCount, 1);
  EXPECT_FALSE(f.m_executor.isBusy());
}


void test_baseClassCommand()
{
  Fixture f;
  f.run("Sleep(0)");

  expectLeft(f.onlyResult(), GDValue());

  // The base class handles it, so it does not get logged.
  EXPECT_EQ(f.logString(), "signal");
}


void test_waitingCounterDefers()
{
  Fixture f;
  int const origCounter = g_waitingCounter;

  // While the counter is positive, the app is not considered
  // quiescent, so the command waits until the timer decrements it.
  adjWaitingCounter(+1);
  QTimer::singleShot(50 /*ms*/,
    &f.m_tester, &ExecutorTester::slot_decrementWaitingCounter);
  f.run("Return(1)");

  expectLeft(f.onlyResult(), 1);
  EXPECT_EQ(f.logString(), "decrement | command Return | signal");
  EXPECT_EQ(g_waitingCounter, origCounter);
}


CLOSE_ANONYMOUS_NAMESPACE


// Called from unit-tests.cc.
void test_ui_shell_executor(CmdlineArgsSpan args)
{
  test_return();
  test_throw();
  test_steps();
  test_stepThrows();
  test_globalSelfCheckThrows();
  test_chaining();
  test_untakenResultDiscarded();
  test_idle();
  test_baseClassCommand();
  test_waitingCounterDefers();
}


// EOF
