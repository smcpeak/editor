// ui-shell-script-runner.h
// `UIShellScriptRunner`, part of the UI shell.  See doc/ui-shell.txt.

// See license.txt for copyright and terms of use.

#ifndef EDITOR_UI_SHELL_SCRIPT_RUNNER_H
#define EDITOR_UI_SHELL_SCRIPT_RUNNER_H

// editor
#include "ui-shell-command-handler-fwd.h"        // UIShellCommandHandler [f]
#include "ui-shell-executor.h"                   // UIShellExecutor

// smbase
#include "smbase/gdvalue-fwd.h"                  // gdv::GDValue [f]
#include "smbase/sm-noexcept.h"                  // NOEXCEPT
#include "smbase/std-vector-fwd.h"               // std::vector [f]

// Qt
#include <QEventLoop>
#include <QObject>

// libc++
#include <cstddef>                               // std::size_t
#include <functional>                            // std::function
#include <optional>                              // std::optional


// Read events and test assertions from a file.  Synthesize the events
// to drive the app, and check the assertions.
//
// See doc/ui-shell.txt for theory of operation.
class UIShellScriptRunner : public QObject {
  Q_OBJECT

public:      // data
  // If true, print a failure to stdout as soon as it happens.
  // Initially false.
  bool m_printFailureToStdout;

private:     // instance data
  // Sequence of UI shell commands to execute.
  std::vector<gdv::GDValue> const &m_testCommands;

  // Index in `m_testCommands` to execute next.
  std::size_t m_nextTestCommandIndex;

  // If the test has failed, this describes how.  Otherwise (test still
  // running, or it succeeded) it is absent.
  std::optional<std::string> m_testFailureMessage;

  // Event loop object started by 'runTest'.
  QEventLoop m_eventLoop;

  // Invariant check, called once before the first command.  (The
  // executor calls it after each command.)
  std::function<void ()> m_globalSelfCheck;

  // Executes the commands.
  UIShellExecutor m_executor;

private:     // funcs
  // Start the next command in `m_testCommands`.
  //
  // Requires: m_nextTestCommandIndex < m_testCommands.size()
  void startNextCommand();

public:      // funcs
  // `handler` interprets the commands.  It and `testCommands` must
  // outlive this object.
  UIShellScriptRunner(
    UIShellCommandHandler &handler,
    std::vector<gdv::GDValue> const &testCommands,
    std::function<void ()> globalSelfCheck);
  ~UIShellScriptRunner();

  // Inject all the events and check all the assertions.  If everything
  // is ok, return nothing.  Otherwise return a string describing the
  // test failure.
  std::optional<std::string> runTest();

private Q_SLOTS:
  // Called when `m_executor` has a result for the current command.
  void slot_hasCommandResult() NOEXCEPT;
};


#endif // EDITOR_UI_SHELL_SCRIPT_RUNNER_H
