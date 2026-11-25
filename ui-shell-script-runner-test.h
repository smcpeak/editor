// ui-shell-script-runner-test.h
// Classes for ui-shell-script-runner-test.cc.

// See license.txt for copyright and terms of use.

#ifndef EDITOR_UI_SHELL_SCRIPT_RUNNER_TEST_H
#define EDITOR_UI_SHELL_SCRIPT_RUNNER_TEST_H

// smbase
#include "smbase/sm-macros.h"                    // NO_OBJECT_COPIES
#include "smbase/sm-noexcept.h"                  // NOEXCEPT
#include "smbase/std-string-fwd.h"               // std::string [f]
#include "smbase/std-vector-fwd.h"               // stdfwd::vector [f]

// Qt
#include <QEventLoop>
#include <QObject>


// Runs a nested event loop, as a modal dialog would, and provides the
// hang protection timeout slot.
class NestedLoopHelper : public QObject {
  Q_OBJECT
  NO_OBJECT_COPIES(NestedLoopHelper);

public:      // data
  // Log shared with the command handler.
  stdfwd::vector<std::string> &m_log;

  // Loop run by `slot_runNestedLoop`.
  QEventLoop m_nestedLoop;

public:      // funcs
  explicit NestedLoopHelper(stdfwd::vector<std::string> &log);
  ~NestedLoopHelper();

public Q_SLOTS:
  // Log "nested start", exec `m_nestedLoop`, then log "nested end".
  void slot_runNestedLoop() NOEXCEPT;

  // Abort the program, because `runTest` has taken too long.
  void slot_timeout() NOEXCEPT;
};


#endif // EDITOR_UI_SHELL_SCRIPT_RUNNER_TEST_H
