// ui-shell-command-handler.h
// `UIShellCommandHandler`, part of the UI shell.  See doc/ui-shell.txt.

// See license.txt for copyright and terms of use.

#ifndef EDITOR_UI_SHELL_COMMAND_HANDLER_H
#define EDITOR_UI_SHELL_COMMAND_HANDLER_H

#include "ui-shell-command-handler-fwd.h"        // fwds for this module

// editor
#include "ui-shell-queryable-fwd.h"              // UIShellQueryable [f]

// smbase
#include "smbase/gdvalue-parser-fwd.h"           // gdv::GDValueParser [f]
#include "smbase/gdvalue-fwd.h"                  // gdv::GDValue [f]
#include "smbase/refct-serf.h"                   // SerfRefCount
#include "smbase/sm-macros.h"                    // NO_OBJECT_COPIES

// libc++
#include <functional>                            // std::function
#include <string>                                // std::string


// Executes UI shell commands for `UIShellExecutor`.
//
// This class implements the commands that do not need widgets.  A
// derived class supplies object lookup, and may add commands.
//
// A command is executed in one or more steps.  `UIShellExecutor`
// calls `executeCommand` at one quiescence point, then, while
// `hasPendingStep()`, calls `executePendingStep` once per subsequent
// quiescence point.  Any of these may throw to indicate failure, in
// which case the executor calls `discardPendingSteps`.
class UIShellCommandHandler : public SerfRefCount {
  NO_OBJECT_COPIES(UIShellCommandHandler);

protected:   // funcs
  // Execute a command that this class does not recognize.  `funcName`
  // is its tag, and `parser` is positioned at it.  Return its result,
  // which is null for an action.
  //
  // The default throws "unrecognized function: $funcName".
  virtual gdv::GDValue executeDerivedCommand(
    std::string const &funcName,
    gdv::GDValueParser const &parser);

  // Sleep for 'ms' milliseconds while pumping the event queue.
  void sleepForMS(int ms);

  // Wait for up to `durationMS` for `gdvFunc` to return `expect`.
  // If we timeout, throw.
  void waitUntilCheckGDValueFunction(
    long durationMS,
    std::function<gdv::GDValue ()> gdvFunc,
    gdv::GDValue const &expect);

private:     // funcs
  // Execute the "WaitUntilCheckQuery" command.
  void waitUntilCheckQuery(
    long durationMS,
    std::string const &receiver,
    std::string const &state,
    gdv::GDValue const &expect);

public:      // funcs
  UIShellCommandHandler();
  virtual ~UIShellCommandHandler();

  // Execute the first (often only) step of `command`, a UI Shell
  // Language tagged tuple.  Return its result, which is null for an
  // action.  Throw to indicate an error.
  //
  // This handles the commands that do not need widgets, and passes
  // any other to `executeDerivedCommand`.
  gdv::GDValue executeCommand(gdv::GDValue const &command);

  // Find the queryable object at `path`, or throw if there is none.
  virtual UIShellQueryable *getQueryableFromPath(
    std::string const &path) = 0;

  // True if the most recent command has steps that remain to be
  // performed.  The default returns false.
  virtual bool hasPendingStep() const;

  // Perform the next remaining step.  Throw to indicate an error.
  //
  // Requires: hasPendingStep()
  //
  // The default fails an assertion, since by default there are no
  // pending steps.
  virtual void executePendingStep();

  // Discard any remaining steps, after an error.  The default does
  // nothing.
  //
  // Ensures: !hasPendingStep()
  virtual void discardPendingSteps();
};


#endif // EDITOR_UI_SHELL_COMMAND_HANDLER_H
