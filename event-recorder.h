// event-recorder.h
// EventRecorder class.

#ifndef EVENT_RECORDER_H
#define EVENT_RECORDER_H

// smbase
#include "sm-override.h"               // OVERRIDE
#include "str.h"                       // string

// Qt
#include <QObject>

// libc++
#include <fstream>                     // std::ofstream


// Acts as an event filter to record input events for later replay as
// part of an automated test.
class EventRecorder : public QObject {
private:     // data
  // File to which we are recording events.
  std::ofstream m_out;

public:      // funcs
  // Automatically installs itself as an event filter for
  // QCoreApplication::instance().
  EventRecorder(string const &filename);
  ~EventRecorder();

  // QObject methods.
  virtual bool eventFilter(QObject *receiver, QEvent *event) OVERRIDE;
};


#endif // EVENT_RECORDER_H
