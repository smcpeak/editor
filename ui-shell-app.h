// ui-shell-app.h
// Support for GUI programs that can be driven by the UI shell: command
// line options, running a script, and running a list of tests.

// See license.txt for copyright and terms of use.

#ifndef EDITOR_UI_SHELL_APP_H
#define EDITOR_UI_SHELL_APP_H

#include "ui-shell-app-fwd.h"                    // fwds for this module

// editor
#include "ui-shell-command-handler-fwd.h"        // UIShellCommandHandler [f]
#include "ui-shell-script-file.h"                // UIShellScriptFile

// smbase
#include "smbase/std-optional-fwd.h"             // std::optional [f]

// libc++
#include <functional>                            // std::function
#include <string>                                // std::string

class QApplication;


// The UI shell command line options shared by GUI programs that can be
// driven by the UI shell.
class UIShellOptions {
public:      // data
  // From "-uisCmds=FILE": FILE, or empty if the option is absent.
  std::string m_scriptFileName;

  // The contents of `m_scriptFileName`, if it is not empty.
  UIShellScriptFile m_script;

  // True if "-record" was given: record input events to
  // "recorded.uis".
  bool m_recordInputEvents;

public:      // funcs
  UIShellOptions();
  ~UIShellOptions();

  // Lines to include in a program's "-help" output, describing the
  // options handled here, including "-testCommands" and NOQUIT.
  static char const *helpText();

  // If `arg` is one of the options handled here, process it and return
  // true.  Otherwise, return false so the caller can try its own
  // options.  Throws `XFormat` for a repeated "-uisCmds", or for
  // "-testCommands" (which is only valid as the first argument, and
  // is handled by `runUIShellTestCommandsIfRequested` before this is
  // called).  Errors from reading the script propagate.
  bool processOption(std::string const &arg);

  // True if "-uisCmds" was given.
  bool hasScript() const;

  // True if "-uisCmds" or "-record" was given, meaning the program
  // should start in a reproducible state (e.g., no user settings).
  bool isTestMode() const;
};


// Run the commands in `options.m_script` with `handler`, calling
// `globalSelfCheck` after each one, and once more at the end if all
// passed.  Return 0 if the script passed and 2 if not.
//
// This also sets `g_abortUponDevWarning`, prints "running test:" and
// "test passed", puts the script name into the exception context,
// suppresses message box sounds, and implements NOQUIT (running
// `app.exec()` before returning).
//
// Requires: options.hasScript()
int runUIShellScript(
  QApplication &app,
  UIShellOptions const &options,
  UIShellCommandHandler &handler,
  std::function<void ()> globalSelfCheck);


// If `argv[1]` is "-testCommands=FILE", run each command line in FILE,
// a GDVN sequence of sequences of strings, by calling `innerMain` with
// it as `argc/argv`, stopping at the first one that fails.  A failing
// command line is retried up to $UI_SHELL_TEST_RETRIES times (default
// 1, i.e., no retries).  Return 0 if all passed, otherwise the exit
// code of the one that failed.
//
// If `argv[1]` is anything else, return `std::nullopt` and do nothing.
//
// `innerMain` must create its own `QApplication`, since it is called
// once for each command line.  The program name in each command line
// becomes `argv[0]`, but is otherwise ignored; the command lines are
// run in this process.
std::optional<int> runUIShellTestCommandsIfRequested(
  int argc, char **argv,
  std::function<int (int argc, char **argv)> innerMain);


#endif // EDITOR_UI_SHELL_APP_H
