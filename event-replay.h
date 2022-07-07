// event-replay.h
// EventReplay class.

#ifndef EVENT_REPLAY_H
#define EVENT_REPLAY_H

// smbase
#include "array.h"                     // ArrayStack
#include "refct-serf.h"                // SerfRefCount
#include "sm-override.h"               // OVERRIDE
#include "str.h"                       // string

// Qt
#include <QEvent>
#include <QEventLoop>

// libc++
#include <fstream>                     // std::ifstream

class QImage;
class QRegularExpressionMatch;
class QWidget;


// Interface that the replay engine uses to query arbitrary state
// elements of objects.
//
// This is also used during recording to do some filtering.
class EventReplayQueryable {
public:
  // Return some application-specific state.  This result must match the
  // argument to the "CheckQuery" command in test scripts for the test
  // to pass.
  //
  // It can throw any exception to indicate a syntax error with 'state';
  // that will cause the test to fail.  It can also just return an error
  // message, as that will not be what the test script is expecting.
  //
  // The default returns the string "unknown state: $state",
  virtual string eventReplayQuery(string const &state);

  // Return an app-specific image, identified by 'what'.  Corresponds to
  // the "CheckQuery" command.  The default implementation returns a
  // null QImage.
  virtual QImage eventReplayImage(string const &what);

  // Return true if resize events should be recorded for this object.
  // Doing so facilitates writing tests for resizing.  The default is
  // false.
  virtual bool wantResizeEventsRecorded();
};


// Read events and test assertions from a file.  Synthesize the events
// to drive the app, and check the assertions.
//
// See doc/event-replay.txt for theory of operation.
class EventReplay : public QObject {
  Q_OBJECT

private:     // types
  // The user event that indicates application quiescence (i.e., the
  // state where nothing more would happen if no more external events
  // arrived).  Receiving this event is what causes us to perform the
  // next scripted event or check.
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

  // Queue of ordinary typing characters to replay before continuing
  // with the events in 'm_in'.
  ArrayStack<char> m_queuedFocusKeySequence;

  // If the test has failed, this describes how.  Otherwise (test still
  // running, or it succeeded) it is "".
  string m_testResult;

  // Event loop object started by 'runTest'.
  QEventLoop m_eventLoop;

  // If non-zero, use a delay timer *instead* of the quiescence
  // mechanism.  A timer is less reliable but can help during debugging.
  // Currently this can only be set with an envvar.
  int m_eventReplayDelayMS;

  // Active timer ID, or 0 if none.
  int m_timerId;

  // True while we are sleeping.  We will wake up and continue
  // processing events, so should do nothing when quiescence is
  // detected.
  bool m_sleeping;

private:     // funcs
  // Change the size of the top-level window containing 'widget' so that
  // the widget itself reaches the target size.  This assumes that the
  // widget expands or contracts the same amount as its parent windows.
  void resizeChildWidget(QWidget *widget, QSize const &targetSize);

  // Get the focus widget, throwing if there is none.
  QWidget *getFocusWidget();

  // Replay a single call line as matched by 'match'.  Throw a string
  // object if there is a problem.
  void replayCall(QRegularExpressionMatch &match);

  // Sleep for 'ms' milliseconds while pumping the event queue.
  void sleepForMS(int ms);

  // Replay an ordinary typing event.
  void replayFocusKey(char c);

  // Read the next event from 'm_in' and replay it.  Return true if
  // the test should continue after doing that.
  bool replayNextEvent();

  // Wrapper around 'replayNextEvent' that shuts down the event loop if
  // the test should stop.
  bool callReplayNextEvent();

  // Execute the "WaitUntilCheckQuery" command.
  void waitUntilCheckQuery(
    long durationMS,
    string const &receiver,
    string const &state,
    string const &expect);

  // Post a QuiescenceEvent to the application event queue.
  void postQuiescenceEvent();

  // Install a timer to trigger the next replay event.
  void installTimer();

  // Kill the timer if it is active.
  void killTimerIf();

public:      // funcs
  explicit EventReplay(string const &fname);
  ~EventReplay();

  // Inject all the events and check all the assertions.  If everything
  // is ok, return "".  Otherwise return a string describing the test
  // failure.
  string runTest();

  // QObject methods.
  virtual bool event(QEvent *ev) OVERRIDE;

private Q_SLOTS:
  // Called when the event dispatcher is about to block.
  void slot_aboutToBlock() NOEXCEPT;
};


#endif // EVENT_REPLAY_H
