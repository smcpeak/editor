// ui-shell-executor.h
// `UIShellExecutor`, part of the UI shell.  See doc/ui-shell.txt.

// See license.txt for copyright and terms of use.

#ifndef EDITOR_UI_SHELL_EXECUTOR_H
#define EDITOR_UI_SHELL_EXECUTOR_H

// editor
#include "ui-shell-command-handler-fwd.h"        // UIShellCommandHandler [f]
#include "ui-shell-result.h"                     // UIShellResult

// smbase
#include "smbase/gdvalue.h"                      // gdv::GDValue
#include "smbase/refct-serf.h"                   // NNRCSerf
#include "smbase/sm-noexcept.h"                  // NOEXCEPT
#include "smbase/sm-macros.h"                    // NO_OBJECT_COPIES
#include "smbase/sm-override.h"                  // OVERRIDE

// Qt
#include <QEvent>
#include <QObject>

// libc++
#include <functional>                            // std::function
#include <optional>                              // std::optional


/*
  Execute UI shell commands one at a time.  Each command is executed
  when the app is quiescent, and its result becomes available once the
  app has become quiescent again afterward.

  The semantics of individual commands is provided by
  `UIShellCommandHandler` (or a subclass thereof), which is accessed via
  the `m_handler` member.

  See doc/ui-shell.txt for theory of operation.
*/
class UIShellExecutor : public QObject {
  Q_OBJECT
  NO_OBJECT_COPIES(UIShellExecutor);

private:     // types
  // The user event that indicates application quiescence (i.e., the
  // state where nothing more would happen if no more external events
  // arrived).  Receiving this event is what causes us to perform the
  // next step of executing a command.
  class QuiescenceEvent : public QEvent {
  public:
    QuiescenceEvent();
    ~QuiescenceEvent();
  };

private:     // class data
  // Event ID of the QuiescenceEvent, or 0 if not assigned yet.
  static int s_quiescenceEventType;

private:     // instance data
  // Interprets the commands.
  NNRCSerf<UIShellCommandHandler> m_handler;

  // Function to invoke after every command step to check invariants.  This should throw an exception to indicate a
  // failure.
  std::function<void ()> m_globalSelfCheck;

  // True from `startCommand` until just before
  // `signal_hasCommandResult` is emitted.
  bool m_busy;

  // Command passed to `startCommand` that has not been executed yet.
  std::optional<gdv::GDValue> m_pendingCommand;

  // Result of the command that was executed most recently, to be
  // wrapped in `UIShellResult` once the command finishes.
  //
  // This has a value only between the calls to `executeCommand` and
  // `finishCommand`, at which point it is either moved into
  // `m_commandResult` (left side) or discarded when the final result is
  // an error.
  std::optional<gdv::GDValue> m_commandSuccessResult;

  // Result (success or error) for the most recently finished command,
  // if it has not yet been taken.
  //
  // Invariant: !(m_commandSuccessResult.has_value() &&
  //              m_commandResult.has_value())
  std::optional<UIShellResult> m_commandResult;

  // If non-zero, use a delay timer *instead* of the quiescence
  // mechanism.  A timer is less reliable but can help during debugging.
  // Currently this can only be set with an envvar.
  int const m_uiShellDelayMS;

  // Active timer ID if one is active.
  std::optional<int> m_timerId;

  // True if we are connected to the event dispatcher's `aboutToBlock`
  // signal.
  //
  // Invariant:
  //   if m_uiShellDelayMS != 0:
  //     m_watchingAboutToBlock == false
  //   else:
  //     m_watchingAboutToBlock == m_busy
  //
  // Although they are normally equal, `m_busy` is meant to reflect the
  // client's externally-visible model of our state, while
  // `m_watchingAboutToBlock` is more about our internal,
  // non-client-visible state.
  //
  bool m_watchingAboutToBlock;

private:     // funcs
  // Perform the next step of the current command, called when the app
  // is quiescent: execute the pending command, or perform one pending
  // step of it, or, if neither remains, finish the command.
  //
  // Requires: m_busy
  //
  // Ensures that, if there was something left to do, we have made
  // progress toward doing it, either by consuming `m_pendingCommand`,
  // or by performing one of the handler's pending steps.
  void executeNextStep();

  // Store `result`, stop watching for quiescence, and emit
  // `signal_hasCommandResult`.  If the signal handler starts another
  // command, execute it now.
  void finishCommand(UIShellResult &&result);

  // Arrange to be notified of quiescence, if we are not already.
  void startWatching();

  // Stop being notified of quiescence.
  void stopWatching();

  // Post a QuiescenceEvent to the application event queue.
  void postQuiescenceEvent();

  // Install a timer to trigger the next step.
  void installTimer();

  // Kill the timer if it is active.
  void killTimerIf();

public:      // funcs
  // `handler` interprets the commands, and must outlive this object.
  //
  // `globalSelfCheck` is called after each command, and after each
  // pending step (such as a queued FocusKeySequence character).  It
  // should throw to indicate a failure, which then becomes an error
  // result.
  UIShellExecutor(UIShellCommandHandler &handler,
                  std::function<void ()> globalSelfCheck);

  // Disconnects from the event dispatcher and kills any timer.
  ~UIShellExecutor();

  // Assert invariants.
  void selfCheck() const;

  // True from `startCommand` until just before
  // `signal_hasCommandResult` is emitted.
  bool isBusy() const;

  /* Execute `command`, a UI Shell Language tagged tuple, the next time
     the app is quiescent:

       1. `startCommand` stores the command and starts watching for
          quiescence: it connects to aboutToBlock, or, if
          UI_SHELL_DELAY_MS is set, starts the timer.

       2. At the next quiescence, the executor executes the command
          (`executeCommand`), then calls the self-check.

       3. At each following quiescence, if the handler has a pending
          step (such as a queued FocusKeySequence character), it
          performs it and calls the self-check.

       4. At the first quiescence with nothing pending, it stops watching
          for quiescence, clears the busy flag, stores `null` as the
          successful result, and finally emits
          `signal_hasCommandResult`.

     When this is called, if there is a result that hasn't been taken,
     it is discarded.

     Requires: !isBusy()

     Ensures: isBusy() && !hasCommandResult()
  */
  void startCommand(gdv::GDValue const &command);

  // True if a result from a prior command is ready and has not been
  // retrieved yet.
  bool hasCommandResult() const;

  // Take the result out of this executor object.
  //
  // Requires: hasCommandResult()
  //
  // Ensures: !hasCommandResult()
  UIShellResult takeCommandResult();

  // QObject methods.
  virtual bool event(QEvent *ev) OVERRIDE;

Q_SIGNALS:
  // Emitted, at a quiescence point, when the command passed to
  // `startCommand` has finished, the app has settled, and
  // `hasCommandResult()` is true.  The slot handler may call
  // `startCommand` to begin the next command.
  void signal_hasCommandResult();

private Q_SLOTS:
  // Called when the event dispatcher is about to block.
  void slot_aboutToBlock() NOEXCEPT;
};


#endif // EDITOR_UI_SHELL_EXECUTOR_H
