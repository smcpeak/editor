// ui-shell.h
// Classes that implement the "UI shell".  See doc/ui-shell.txt.

// See license.txt for copyright and terms of use.

#ifndef EDITOR_UI_SHELL_H
#define EDITOR_UI_SHELL_H

// editor
#include "ui-shell-result.h"           // UIShellResult

// smbase
#include "smbase/array.h"              // ArrayStack
#include "smbase/gdvalue.h"            // gdv::GDValue
#include "smbase/refct-serf.h"         // SerfRefCount
#include "smbase/sm-noexcept.h"        // NOEXCEPT
#include "smbase/sm-macros.h"          // NO_OBJECT_COPIES
#include "smbase/sm-override.h"        // OVERRIDE
#include "smbase/std-vector-fwd.h"     // std::vector [f]

// Qt
#include <QEvent>
#include <QEventLoop>

// libc++
#include <cstddef>                     // std::size_t
#include <functional>                  // std::function
#include <optional>                    // std::optional
#include <string>                      // std::string

class QAbstractButton;
class QWidget;


// Execute UI shell commands one at a time.  Each command is executed
// when the app is quiescent, and its result becomes available once the
// app has become quiescent again afterward.
//
// See doc/ui-shell.txt for theory of operation.
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
  // Function to invoke after every command and queued focus key to
  // check invariants.  This should throw an exception to indicate a
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

  // Queue of ordinary typing characters to execute before the current
  // command is considered finished.
  ArrayStack<char> m_queuedFocusKeySequence;

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
  // Change the size of the top-level window containing 'widget' so that
  // the widget itself reaches the target size.  This assumes that the
  // widget expands or contracts the same amount as its parent windows.
  void resizeChildWidget(QWidget *widget, QSize const &targetSize);

  // Get the focus widget, throwing if there is none.  `funcName` is
  // the function we are performing, and used in a `TRACE` call.
  QWidget *getFocusWidget(QString const &funcName);

  // Execute a single `command`.  Throw an exception if there is a
  // problem.  Return the command's result, which is null for an
  // action.
  gdv::GDValue executeCommand(gdv::GDValue const &command);

  // Do an extra sleep on Linux as a workaround for a bug.  See comments
  // on implementation.
  void checkFocusWorkaround();

  // Like `button->click()`, but posting the event rather than sending
  // it for immediate execution.  This ensures we don't get trapped in
  // a new event loop if the button, say, pops up a modal dialog.
  void postClickButton(QAbstractButton *button);

  // Sleep for 'ms' milliseconds while pumping the event queue.
  void sleepForMS(int ms);

  // Execute a "Shortcut" event.
  void executeShortcut(
    std::string const &receiverPath,
    std::string const &keys);

  // Execute an ordinary typing event.
  void executeFocusKey(char c);

  // Perform the next step of the current command, called when the app
  // is quiescent: execute the pending command, or deliver one queued
  // focus key, or, if neither remains, finish the command.
  //
  // Requires: m_busy
  //
  // Ensures that, if there was something left to do, we have made
  // progress toward doing it, either by consuming `m_pendingCommand`,
  // or by taking an element from `m_queuedFocusKeySequence`.
  void executeNextStep();

  // Store `result`, stop watching for quiescence, and emit
  // `signal_hasCommandResult`.  If the signal handler starts another
  // command, execute it now.
  void finishCommand(UIShellResult &&result);

  // Execute the "WaitUntilCheckQuery" command.
  void waitUntilCheckQuery(
    long durationMS,
    std::string const &receiver,
    std::string const &state,
    gdv::GDValue const &expect);

  // Wait for up to `durationMS` for `gdvFunc` to return `expect`.
  // If we timeout, throw.
  void waitUntilCheckGDValueFunction(
    long durationMS,
    std::function<gdv::GDValue ()> gdvFunc,
    gdv::GDValue const &expect);

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
  // `globalSelfCheck` is called after each command, and after each
  // queued FocusKeySequence character.  It should throw to indicate a
  // failure, which then becomes an error result.
  explicit UIShellExecutor(std::function<void ()> globalSelfCheck);

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

       3. At each following quiescence, if FocusKeySequence characters
          are queued, it delivers one and calls the self-check.

       4. At the first quiescence with nothing queued, it stops watching
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

  // Internal: temporarily connected to a button's slot to enqueue a
  // click (ClickButton).  It is public only because Qt makes all
  // signals public.
  void signal_clickButton();

private Q_SLOTS:
  // Called when the event dispatcher is about to block.
  void slot_aboutToBlock() NOEXCEPT;
};


// Read events and test assertions from a file.  Synthesize the events
// to drive the app, and check the assertions.
//
// See doc/ui-shell.txt for theory of operation.
class UIShellScriptRunner : public QObject {
  Q_OBJECT

private:     // instance data
  // Sequence of UI shell commands to execute.
  std::vector<gdv::GDValue> const &m_testCommands;

  // Index in `m_testCommands` to execute next.
  std::size_t m_nextTestCommandIndex;

  // If the test has failed, this describes how.  Otherwise (test still
  // running, or it succeeded) it is "".
  //
  // TODO: Rename to `m_testFailureMessage`.
  std::string m_testResult;

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
  explicit UIShellScriptRunner(
    std::vector<gdv::GDValue> const &testCommands,
    std::function<void ()> globalSelfCheck);
  ~UIShellScriptRunner();

  // Inject all the events and check all the assertions.  If everything
  // is ok, return "".  Otherwise return a string describing the test
  // failure.
  std::string runTest();

private Q_SLOTS:
  // Called when `m_executor` has a result for the current command.
  void slot_hasCommandResult() NOEXCEPT;
};


#endif // EDITOR_UI_SHELL_H
