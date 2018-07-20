// event-replay.h
// EventReplay class.

#ifndef EVENT_REPLAY_H
#define EVENT_REPLAY_H

// smbase
#include "refct-serf.h"                // SerfRefCount
#include "str.h"                       // string

// libc++
#include <fstream>                     // std::ifstream


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
class EventReplay {
private:     // data
  // Name of file being read.
  string m_fname;

  // File we are reading from.
  std::ifstream m_in;

  // Mechanism that will respond to queries.  Never NULL.
  RCSerf<EventReplayQuery> m_query;

public:
  EventReplay(string const &fname, EventReplayQuery *query);
  ~EventReplay();

  // Inject all the events and check all the assertions.  If everything
  // is ok, return "".  Otherwise return a string describing the test
  // failure.
  string runTest();
};


#endif // EVENT_REPLAY_H
