// ui-shell-executor.cc
// Code for `ui-shell-executor` module.

#include "ui-shell-executor.h"         // this module

// editor
#include "debug-values.h"              // DEBUG_VALUES
#include "ui-shell-command-handler.h"  // UIShellCommandHandler
#include "ui-shell-result.h"           // UIShellResult
#include "waiting-counter.h"           // g_waitingCounter

// smqtutil
#include "smqtutil/qtutil.h"           // disconnectSignals

// smbase
#include "smbase/exc.h"                // EXN_CONTEXT_STRING_OPT, GENERIC_CATCH_BEGIN/END
#include "smbase/gdvalue.h"            // gdv::GDValue
#include "smbase/optional-util.h"      // smbase::takeOptionalsValue
#include "smbase/sm-env.h"             // smbase::envAsIntOr
#include "smbase/sm-macros.h"          // IMEMBFP
#include "smbase/sm-trace.h"           // INIT_TRACE, etc.
#include "smbase/stringb.h"            // stringb
#include "smbase/xassert.h"            // xassert, xassertPrecondition, xassertInvariant

// Qt
#include <QAbstractEventDispatcher>
#include <QCoreApplication>
#include <QEvent>
#include <QObject>

// libc++
#include <exception>                   // std::exception
#include <functional>                  // std::function
#include <optional>                    // std::optional
#include <string>                      // std::string
#include <utility>                     // std::move

using namespace gdv;
using namespace smbase;


INIT_TRACE("ui-shell-executor");


int UIShellExecutor::s_quiescenceEventType = 0;


UIShellExecutor::QuiescenceEvent::QuiescenceEvent()
  : QEvent((QEvent::Type)s_quiescenceEventType)
{}

UIShellExecutor::QuiescenceEvent::~QuiescenceEvent()
{}


UIShellExecutor::UIShellExecutor(
  UIShellCommandHandler &handler,
  std::function<void ()> globalSelfCheck)
:
  QObject(),
  m_handler(&handler),
  IMEMBFP(globalSelfCheck),
  m_busy(false),
  m_pendingCommand(),
  m_commandSuccessResult(),
  m_commandResult(),
  m_uiShellDelayMS(envAsIntOr(0, "UI_SHELL_DELAY_MS")),
  m_timerId(),
  m_watchingAboutToBlock(false)
{
  if (s_quiescenceEventType == 0) {
    s_quiescenceEventType = QEvent::registerEventType();
    TRACE2(DEBUG_VALUES(s_quiescenceEventType));
    xassert(s_quiescenceEventType > 0);
  }

  selfCheck();
}


UIShellExecutor::~UIShellExecutor()
{
  this->killTimerIf();

  // It is possible we already disconnected, or never connected in the
  // first place, but this is still safe.
  disconnectSignals(QAbstractEventDispatcher::instance(), this);
}


void UIShellExecutor::selfCheck() const
{
  xassertInvariant(
    !(m_commandSuccessResult.has_value() &&
      m_commandResult.has_value())
  );

  if (m_uiShellDelayMS != 0) {
    xassertInvariant(m_watchingAboutToBlock == false);
  }
  else {
    xassertInvariant(m_watchingAboutToBlock == m_busy);
  }
}


bool UIShellExecutor::isBusy() const
{
  return m_busy;
}


void UIShellExecutor::startCommand(GDValue const &command)
{
  xassertPrecondition(!m_busy);
  selfCheck();

  m_commandSuccessResult.reset();
  m_commandResult.reset();
  m_pendingCommand = command;
  m_busy = true;

  this->startWatching();

  selfCheck();
}


bool UIShellExecutor::hasCommandResult() const
{
  return m_commandResult.has_value();
}


UIShellResult UIShellExecutor::takeCommandResult()
{
  xassertPrecondition(hasCommandResult());
  selfCheck();

  return takeOptionalsValue(std::move(m_commandResult));
}


void UIShellExecutor::executeNextStep()
{
  xassertPrecondition(m_busy);
  selfCheck();

  // If set, the command failed, and this describes how.
  std::optional<std::string> errorMessage;

  try {
    // Execute the pending command, if any.
    if (m_pendingCommand) {
      GDValue command(takeOptionalsValue(std::move(m_pendingCommand)));

      TRACE1("executing: " << command.sourceLocationIndicator() <<
             command);
      TRACE3("command dump: " << command.dumpToString());

      // Use the location of `command` as context.  A command that was
      // not read from a file might not have one.
      EXN_CONTEXT_STRING_OPT(command.hasSourceLocation()?
        std::make_optional(stringb(command.sourceLocation())) :
        std::nullopt);

      m_commandSuccessResult = m_handler->executeCommand(command);
      m_globalSelfCheck();
      selfCheck();
      return;
    }

    // Perform any pending steps of the command.
    if (m_handler->hasPendingStep()) {
      // Here, we are not in an EXN_CONTEXT block, so will not get
      // source location info if the call throws.  Fortunately, the
      // existing pending steps just inject ordinary keystrokes, and
      // that basically never fails.
      m_handler->executePendingStep();

      m_globalSelfCheck();
      selfCheck();
      return;
    }

    // Nothing remains, so the command has finished successfully.
  }
  catch (std::exception &x) {
    errorMessage = x.what();
  }
  catch (...) {
    errorMessage = std::string("caught unknown exception!");
  }

  if (errorMessage) {
    // Discard whatever remains of the failed command.
    m_handler->discardPendingSteps();

    this->finishCommand(UIShellResult::makeRight(
      *errorMessage));
  }
  else if (!m_commandSuccessResult) {
    // This shouldn't happen, but if it does, an error (rather than an
    // assertion) suffices.
    this->finishCommand(UIShellResult::makeRight(
      "Finished command but no result?"));
  }
  else {
    this->finishCommand(UIShellResult::makeLeft(
      std::move(*m_commandSuccessResult)));
  }
}


void UIShellExecutor::finishCommand(UIShellResult &&result)
{
  TRACE2("command finished: " << toGDValue(result));

  this->stopWatching();
  m_busy = false;
  m_commandSuccessResult.reset();
  m_commandResult = std::move(result);

  // Check before we send the signal, since sending it effectively
  // publishes our state.
  selfCheck();

  Q_EMIT signal_hasCommandResult();

  selfCheck();

  if (m_busy && m_pendingCommand) {
    // The signal handler started another command.  Execute it during
    // this same quiescence cycle, rather than waiting for the next,
    // so each command runs at the first quiescence after the previous
    // one finished.
    //
    // This isn't necessary for correctness.  In theory it's a slight
    // efficiency win, but in practice the benefit cannot be measured.
    TRACE2("executing the next command immediately");
    this->executeNextStep();
  }
}


void UIShellExecutor::startWatching()
{
  if (m_uiShellDelayMS) {
    // Use timer-based notification.
    if (!m_timerId.has_value()) {
      TRACE1("installing first timer");
      this->installTimer();
    }
  }
  else if (!m_watchingAboutToBlock) {
    // Arrange to get notified just before the event dispatcher yields
    // control to the OS.  See doc/ui-shell.txt.
    TRACE2("connecting slot_aboutToBlock");
    QObject::connect(QAbstractEventDispatcher::instance(),
                     &QAbstractEventDispatcher::aboutToBlock,
                     this, &UIShellExecutor::slot_aboutToBlock);
    m_watchingAboutToBlock = true;
  }
}


void UIShellExecutor::stopWatching()
{
  this->killTimerIf();

  if (m_watchingAboutToBlock) {
    // Disconnect from the event dispatcher to stop getting signals.
    TRACE2("disconnecting slot_aboutToBlock");
    QObject::disconnect(QAbstractEventDispatcher::instance(), nullptr,
                        this, nullptr);
    m_watchingAboutToBlock = false;
  }
}


void UIShellExecutor::postQuiescenceEvent()
{
  QCoreApplication::postEvent(this, new QuiescenceEvent());
}


void UIShellExecutor::installTimer()
{
  xassert(m_uiShellDelayMS);
  TRACE1("starting timer");

  this->killTimerIf();
  m_timerId = this->startTimer(m_uiShellDelayMS);
}


void UIShellExecutor::killTimerIf()
{
  if (m_timerId.has_value()) {
    TRACE1("killing timer");
    this->killTimer(*m_timerId);
    m_timerId.reset();
  }
}


bool UIShellExecutor::event(QEvent *ev)
{
  if (ev->type() == s_quiescenceEventType) {
    TRACE2("received QuiescenceEvent");

    if (!m_watchingAboutToBlock) {
      // This can happen if the event was posted before we stopped
      // watching for quiescence.
      TRACE2("ignoring QuiescenceEvent because no command is active");
    }
    else if (g_waitingCounter) {
      TRACE2("ignoring QuiescenceEvent because g_waitingCounter is " <<
             g_waitingCounter);
    }
    else {
      // If this finishes the command, it also stops watching, unless
      // another command gets started.  Otherwise, we will continue to
      // receive 'aboutToBlock' and hence enqueue QuiescenceEvents.
      this->executeNextStep();
    }

    TRACE2("finished with QuiescenceEvent");
    return true;
  }

  if (ev->type() == QEvent::Timer) {
    TRACE2("received TimerEvent");
    this->killTimerIf();

    if (m_busy) {
      this->executeNextStep();
    }

    if (m_busy && !m_timerId.has_value()) {
      // The command is continuing.  Arrange to receive another event.
      // (If the command finished and the handler started another,
      // `startWatching` has already installed a timer.)
      this->installTimer();
    }
    else {
      TRACE2("refraining from installing another timer");
    }

    return true;
  }

  // This object should only be receiving the above two events, and I
  // don't think QObject::event does anything with them (besides return
  // false), but I will do this just for overall cleanliness.
  return QObject::event(ev);
}


void UIShellExecutor::slot_aboutToBlock() NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  if (g_waitingCounter > 0) {
    // Ignore the quiescence, and don't print anything so we don't spam
    // the log.
    return;
  }

  TRACE2("in slot_aboutToBlock");

  // Getting here means the application really is quiescent; if we did
  // not do this, the app would block waiting for some external event.
  // So, post an event that will trigger the next command execution.
  this->postQuiescenceEvent();

  // At this point, we're about to return to the site of a call to
  // WaitForMultipleObjectsEx (or poll/select on unix) and we know the
  // OS event queue is empty.  In this state, WaitFor will block, but we
  // want to keep running in order to process the QuiescenceEvent we
  // just posted to the Qt event queue.  So, tell the dispatcher to wake
  // up.  That posts a message to the OS event queue which will ensure
  // that WaitFor returns immediately.  Then, control will cycle through
  // the usual event dispatch steps, including handling the
  // QuiescenceEvent, before eventually returning to 'aboutToBlock' once
  // the app is again quiescent.
  QAbstractEventDispatcher::instance()->wakeUp();

  GENERIC_CATCH_END
}


// EOF
