// event-replay.h
// EventReplay class.

#ifndef EVENT_REPLAY_H
#define EVENT_REPLAY_H

// smbase
#include "refct-serf.h"                // SerfRefCount
#include "sm-override.h"               // OVERRIDE
#include "str.h"                       // string

// Qt
#include <QEvent>
#include <QEventLoop>

// libc++
#include <fstream>                     // std::ifstream

class QRegularExpressionMatch;


// Interface that the replay engine uses to query application state.
class EventReplayQuery : virtual public SerfRefCount {
public:
  // Return some application-specific state.  This result must match the
  // argument to the "Check" command in test scripts for the test to
  // pass.
  //
  // It can throw any exception to indicate a syntax error with 'state';
  // that will cause the test to fail.  It can also just return an error
  // message, as that will not be what the test script is expecting.
  virtual string eventReplayQuery(string const &state) = 0;
};


// Read events and test assertions from a file.  Synthesize the events
// to drive the app, and check the assertions.
class EventReplay : public QObject {
  Q_OBJECT

private:     // types
  // The user event that indicates near-quiescence.
  class QuiescenceEvent : public QEvent {
  public:
    QuiescenceEvent();
    ~QuiescenceEvent();
  };

private:     // class data
  // Event ID of the QuiescenceEvent, or 0 if not assigned yet.
  static int s_quiescenceEventType;

private:     // instance data
  // Name of file being read.
  string m_fname;

  // File we are reading from.
  std::ifstream m_in;

  // Current line number in 'm_in'.
  int m_lineNumber;

  // Mechanism that will respond to queries.  Never NULL.
  RCSerf<EventReplayQuery> m_query;

  // If the test has failed, this describes how.  Otherwise (test still
  // running, or it succeeded) it is "".
  string m_testResult;

  // Event loop object started by 'runTest'.
  QEventLoop m_eventLoop;

  // If non-zero, wait this long between events.  Normally it should be
  // fine to use 0 since the mechanism always waits for quiescence, but
  // a delay can help during debugging.  Currently this can only be set
  // with an envvar.
  int m_eventReplayDelayMS;

  // Active timer ID, or 0 if none.
  int m_timerId;

private:     // funcs
  // Replay a single call line as matched by 'match'.  Throw a string
  // object if there is a problem.
  void replayCall(QRegularExpressionMatch &match);

  // Read the next event from 'm_in' and replay it.  Return true if
  // the test should continue after doing that.
  bool replayNextEvent();

  // Post either a timer or quiescence event to be delivered in the
  // future.
  void postFutureEvent();

  // Kill the timer if it is active.
  void killTimerIf();

  // Post a QuiescenceEvent to the application event queue.
  void postQuiescenceEvent();

public:      // funcs
  EventReplay(string const &fname, EventReplayQuery *query);
  ~EventReplay();

  // Inject all the events and check all the assertions.  If everything
  // is ok, return "".  Otherwise return a string describing the test
  // failure.
  string runTest();

  // QObject methods.
  virtual bool event(QEvent *ev) OVERRIDE;
};


#endif // EVENT_REPLAY_H
