// ui-shell-executor-test.h
// Classes for ui-shell-executor-test.cc.

// See license.txt for copyright and terms of use.

#ifndef EDITOR_UI_SHELL_EXECUTOR_TEST_H
#define EDITOR_UI_SHELL_EXECUTOR_TEST_H

// editor
#include "ui-shell-executor.h"                   // UIShellExecutor
#include "ui-shell-result.h"                     // UIShellResult

// smbase
#include "smbase/gdvalue.h"                      // gdv::GDValue
#include "smbase/sm-macros.h"                    // NO_OBJECT_COPIES
#include "smbase/sm-noexcept.h"                  // NOEXCEPT

// Qt
#include <QEventLoop>
#include <QObject>
#include <QTimer>

// libc++
#include <deque>                                 // std::deque
#include <string>                                // std::string
#include <vector>                                // std::vector


// Runs commands through a `UIShellExecutor` using a local event loop,
// and records what happens.
class ExecutorTester : public QObject {
  Q_OBJECT
  NO_OBJECT_COPIES(ExecutorTester);

public:      // data
  // Executor under test.
  UIShellExecutor &m_executor;

  // Log of events, shared with the command handler so the relative
  // order of handler calls and signals is visible.
  std::vector<std::string> &m_log;

  // Loop that `runCommand` executes.
  QEventLoop m_eventLoop;

  // Fails the test if the loop runs for too long.
  QTimer m_timeoutTimer;

  // Number of times `signal_hasCommandResult` has been emitted.
  int m_signalCount;

  // `m_executor.isBusy()` as of the most recent signal.
  bool m_busyAtSignal;

  // If true, the signal handler takes each result.
  bool m_takeResults;

  // Results taken by the signal handler, in order.
  std::vector<UIShellResult> m_results;

  // Commands for the signal handler to start, one per signal.
  std::deque<gdv::GDValue> m_chainedCommands;

  // `runCommand` stops the loop once `m_signalCount` reaches this.
  int m_quitAtSignalCount;

public:      // funcs
  ExecutorTester(
    UIShellExecutor &executor,
    std::vector<std::string> &log);
  ~ExecutorTester();

  // Start `command`, then run the event loop until it, and any
  // commands in `m_chainedCommands`, have finished.
  void runCommand(gdv::GDValue const &command);

public Q_SLOTS:
  // Record the signal, take the result, and possibly start the next
  // command or stop the loop.
  void slot_hasCommandResult() NOEXCEPT;

  // Stop the loop because it has taken too long.
  void slot_timeout() NOEXCEPT;

  // Log "decrement" and decrement `g_waitingCounter`.
  void slot_decrementWaitingCounter() NOEXCEPT;
};


#endif // EDITOR_UI_SHELL_EXECUTOR_TEST_H
