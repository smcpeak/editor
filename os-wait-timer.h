// os-wait-timer.h
// `OSWaitTimer`, a class to track time spent waiting for an OS event.
// See `doc/os-wait-timer.txt`.

// See license.txt for copyright and terms of use.

#ifndef EDITOR_OS_WAIT_TIMER_H
#define EDITOR_OS_WAIT_TIMER_H

// smbase
#include "smbase/htt-fwd.h"            // smbase::HTT_Node
#include "smbase/sm-macros.h"          // NO_OBJECT_COPIES, NULLABLE

// Qt
#include <QObject>

// C++
#include <cstdint>                     // std::int32_t

class QAbstractEventDispatcher;


// React to `QAbstractEventDispatcher` events `aboutToBlock` and `awake`
// to start and stop a timer associated with being blocked on the OS.
// See `doc/os-wait-timer.txt`.
class OSWaitTimer : public QObject {
  Q_OBJECT
  NO_OBJECT_COPIES(OSWaitTimer);

private:     // data
  // The dispatcher active at time of creation.
  QAbstractEventDispatcher * const m_dispatcher;

  // Index for the label used with the timer.
  std::int32_t const m_labelIndex;

  // False when this object is in a do-nothing mode, in particular not
  // receiving signals, due to this mechanism being disabled on the
  // current platform.
  bool const m_active;

  // If the timer is running, the associated node.  Otherwise nullptr.
  smbase::HTT_Node * NULLABLE m_node;

public:      // funcs
  // Connect the signals.
  OSWaitTimer();

  // Disconnect the signals.
  virtual ~OSWaitTimer();

public Q_SLOTS:
  // React to the signals.
  void slot_aboutToBlock();
  void slot_awake();
};


#endif // EDITOR_OS_WAIT_TIMER_H
