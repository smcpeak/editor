// waiting-counter.h
// Declaration of 'g_waitingCounter'.

#ifndef EDITOR_WAITING_COUNTER_H
#define EDITOR_WAITING_COUNTER_H


// This is for use by EventReplay (event-replay.h).  It is not a member
// of that class because that header has some dependencies that I do not
// want to impose on modules that manipulate this value.
//
// When this is non-zero, some component is currently waiting for an
// event that will come from an external source, and consequently this
// application should not be considered quiescent, even if the event
// queue is empty.
//
// One example is when a timer has been scheduled, and we are waiting
// for its event.  Another is when a request has been sent to a server
// process, and a reply is expected.
//
// In contrast, if the only thing the app is waiting for is user
// input, then this should be 0, its default value.
extern int g_waitingCounter;


// Basically 'g_waitingCounter += amt;', but with an assertion that the
// value remains non-negative.
void adjWaitingCounter(int amt);


// Increment the waiting counter on entry, decrement on leaving.
class IncDecWaitingCounter {
  NO_OBJECT_COPIES(IncDecWaitingCounter);

public:      // methods
  IncDecWaitingCounter()
  {
    adjWaitingCounter(+1);
  }

  ~IncDecWaitingCounter()
  {
    adjWaitingCounter(-1);
  }
};



#endif // EDITOR_WAITING_COUNTER_H
