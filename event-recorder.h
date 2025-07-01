// event-recorder.h
// EventRecorder class.

#ifndef EVENT_RECORDER_H
#define EVENT_RECORDER_H

// smbase
#include "smbase/sm-override.h"        // OVERRIDE
#include "smbase/str.h"                // string, stringBuilder

// Qt
#include <QKeyEvent>
#include <QObject>

// libc++
#include <fstream>                     // std::ofstream


// Acts as an event filter to record input events for later replay as
// part of an automated test.
class EventRecorder : public QObject {
private:     // data
  // File to which we are recording events.  Only the 'recordEvent'
  // method should directly write to this.
  std::ofstream m_outStream;

  // Queued sequence of ordinary key presses that should be written to
  // the stream before any other kind of event.
  stringBuilder m_ordinaryKeyChars;

private:     // funcs
  void recordKeyEvent(QObject *receiver, QKeyEvent const *keyEvent);
  void recordOrdinaryKeyPress(char c);
  void flushOrdinaryKeyChars();
  void recordEvent(string const &ev);

public:      // funcs
  // Automatically installs itself as an event filter for
  // QCoreApplication::instance().
  EventRecorder(string const &filename);
  ~EventRecorder();

  // QObject methods.
  virtual bool eventFilter(QObject *receiver, QEvent *event) OVERRIDE;
};


#endif // EVENT_RECORDER_H
