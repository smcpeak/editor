// ui-shell-test-handler.h
// `SequencingTestHandler`, a UI shell command handler for tests.

// See license.txt for copyright and terms of use.

#ifndef EDITOR_UI_SHELL_TEST_HANDLER_H
#define EDITOR_UI_SHELL_TEST_HANDLER_H

// editor
#include "ui-shell-command-handler.h"            // UIShellCommandHandler
#include "ui-shell-queryable-fwd.h"              // UIShellQueryable [f]

// smbase
#include "smbase/gdvalue-fwd.h"                  // gdv::GDValue [f]
#include "smbase/gdvalue-parser-fwd.h"           // gdv::GDValueParser [f]
#include "smbase/sm-override.h"                  // OVERRIDE

// libc++
#include <string>                                // std::string
#include <vector>                                // std::vector


/* UIShellCommandHandler for tests.  Records calls in a log and adds
   these commands for testing the executor and script runner:

     Mark("a")       Log "mark a" and return null.
     Return(v)       Return v.
     Throw("msg")    Throw XMessage("msg").
     Steps(n)        Return null and leave n pending steps.
     StepThrows(n k) Like Steps(n), but step k (1-based) throws.

   Each command other than Mark is logged as "command <name>".
*/
class SequencingTestHandler : public UIShellCommandHandler {
public:      // data
  // Log of calls, shared with the test.
  std::vector<std::string> &m_log;

  // Number of the next pending step to perform, 1-based.
  int m_nextStep;

  // Number of steps in the current command.
  int m_numSteps;

  // Step that throws, or 0 for none.
  int m_throwAtStep;

protected:   // funcs
  // UIShellCommandHandler methods.
  virtual gdv::GDValue executeDerivedCommand(
    std::string const &funcName,
    gdv::GDValueParser const &parser) OVERRIDE;

public:      // funcs
  explicit SequencingTestHandler(std::vector<std::string> &log);
  ~SequencingTestHandler();

  // UIShellCommandHandler methods.
  virtual UIShellQueryable *getQueryableFromPath(
    std::string const &path) OVERRIDE;
  virtual bool hasPendingStep() const OVERRIDE;
  virtual void executePendingStep() OVERRIDE;
  virtual void discardPendingSteps() OVERRIDE;
};


#endif // EDITOR_UI_SHELL_TEST_HANDLER_H
