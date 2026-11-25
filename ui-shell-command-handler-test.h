// ui-shell-command-handler-test.h
// Classes for ui-shell-command-handler-test.cc.

// See license.txt for copyright and terms of use.

#ifndef EDITOR_UI_SHELL_COMMAND_HANDLER_TEST_H
#define EDITOR_UI_SHELL_COMMAND_HANDLER_TEST_H

// editor
#include "ui-shell-queryable.h"        // UIShellQueryable

// smbase
#include "smbase/gdvalue.h"            // gdv::GDValue
#include "smbase/sm-noexcept.h"        // NOEXCEPT
#include "smbase/sm-override.h"        // OVERRIDE

// Qt
#include <QObject>

// libc++
#include <map>                         // std::map
#include <set>                         // std::set
#include <string>                      // std::string


// Queryable whose states are entries in a map.
class FakeQueryable : public QObject, public UIShellQueryable {
  Q_OBJECT

public:      // data
  // Map from state name to its value.  Names not in the map are
  // passed to the base class, which throws.
  std::map<std::string, gdv::GDValue> m_values;

  // States whose query throws.
  std::set<std::string> m_throwingStates;

  // State and value that `slot_applyDelayedSet` stores in `m_values`.
  std::string m_delayedState;
  gdv::GDValue m_delayedValue;

public:      // funcs
  FakeQueryable();
  ~FakeQueryable();

  // UIShellQueryable methods.
  virtual gdv::GDValue uiShellQuery(std::string const &state) OVERRIDE;

public Q_SLOTS:
  // Set `m_values[m_delayedState]` to `m_delayedValue`.  This is meant
  // to be invoked by a timer.
  void slot_applyDelayedSet() NOEXCEPT;
};


#endif // EDITOR_UI_SHELL_COMMAND_HANDLER_TEST_H
